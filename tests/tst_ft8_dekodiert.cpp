// SPDX-License-Identifier: GPL-3.0-or-later
//
// FT8, erster Schritt: zuhoeren.
//
// Betreiber, 2026-10-15: „ft8 und ft4 sollte auch angedacht werden.
// zeus, aether usw. haben da schon arbeit geleistet."
//
// Nachgesehen, und der Befund fiel anders aus als erwartet: Thetis und
// AetherSDR dekodieren NICHT selbst — beide schicken den Ton ueber ein
// virtuelles Kabel an WSJT-X. Zeus dekodiert nativ, mit ft8_lib
// (Kārlis Goba, MIT). Longpath hatte bis heute den WSJT-X-Weg.
//
// ── Eine geschlossene Schleife, ohne Tondatei ───────────────────────
//
// Dieser Lauf braucht weder ein Funkgeraet noch eine Aufnahme: er
// erzeugt die Nachricht mit ft8_libs EIGENEM Sender, moduliert sie mit
// DERSELBEN Rechnung, mit der spaeter auch gesendet wuerde, schickt
// sie durch den Dekoder und vergleicht den Wortlaut.
//
// Das ist mehr als „laeuft durch": geht an der Kette irgendwo etwas
// kaputt — Abtastrate, Blockgroesse, Durchlassbereich, LDPC —, kommt
// nichts oder etwas anderes heraus.
//
// Gegenprobe (2026-10-15): mit einer Tonlage ausserhalb des
// Durchlassbereichs (4000 Hz statt 1500) findet derselbe Lauf nichts.

#include <QtTest>

#include "core/Ft8Decoder.h"

#include <cmath>

using namespace Longpath;

class TstFt8Dekodiert : public QObject
{
    Q_OBJECT

private slots:
    void eineNachrichtGehtHinUndZurueck_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("protokoll");   // 0 = FT8, 1 = FT4

        QTest::newRow("CQ FT8")   << QStringLiteral("CQ OE5SOS JN78") << 0;
        QTest::newRow("Rapport")  << QStringLiteral("JA1XYZ OE5SOS -11") << 0;
        QTest::newRow("RR73")     << QStringLiteral("OE5SOS JA1XYZ RR73") << 0;
    }

    void eineNachrichtGehtHinUndZurueck()
    {
        QFETCH(QString, text);
        QFETCH(int, protokoll);
        const auto p = (protokoll == 1) ? Ft8Decoder::Protocol::Ft4
                                        : Ft8Decoder::Protocol::Ft8;

        QVector<float> ton;
        QVERIFY2(Ft8Decoder::encodeBlock(text, 1500.0f, p, &ton),
                 qPrintable(QStringLiteral("liess sich nicht packen: %1")
                                .arg(text)));
        QVERIFY2(ton.size() > 10000,
                 qPrintable(QStringLiteral("nur %1 Proben").arg(ton.size())));

        const auto funde = Ft8Decoder::decodeBlock(ton.constData(),
                                                   ton.size(), p);
        QStringList wortlaute;
        for (const Ft8Decode& d : funde) { wortlaute << d.text; }
        qInfo().noquote() << text << "->" << wortlaute;

        QVERIFY2(!funde.isEmpty(),
                 qPrintable(QStringLiteral("nichts dekodiert aus %1 Proben")
                                .arg(ton.size())));
        QVERIFY2(wortlaute.contains(text),
                 qPrintable(QStringLiteral("gesendet %1 -- gelesen %2")
                                .arg(text,
                                     wortlaute.join(QLatin1String(" | ")))));

        // Und die Begleitwerte muessen stimmen, nicht nur der Text.
        const Ft8Decode& d = funde.first();
        QVERIFY2(std::abs(d.freqHz - 1500.0f) < 20.0f,
                 qPrintable(QStringLiteral("Tonlage %1 Hz statt 1500")
                                .arg(static_cast<double>(d.freqHz))));
        QVERIFY2(d.score >= 10, "Sync-Wert unter der Schwelle");
    }

    /// ── FT4: dekodiert, aber nur ohne die Stille davor ─────────────
    ///
    /// Gemessen am 2026-10-15, und hier festgehalten statt verschwiegen:
    /// dieselbe Nachricht, mit ft8_libs eigenem FT4-Sender moduliert,
    /// wird gefunden, wenn der Block GENAU bei den Daten beginnt — und
    /// NICHT, wenn davor die Stille steht, mit der das Vorbild sein
    /// Zeitfenster auffuellt (demo/gen_ft8.c:174-175).
    ///
    ///     ganzes Fenster, 90000 Proben   ->  0 Funde
    ///     nur die Daten,  60480 Proben   ->  1 Fund
    ///     Fenster plus Luft hinten       ->  0 Funde
    ///
    /// FT8 hat dieses Verhalten NICHT — dort wird mit Stille gefunden.
    ///
    /// Ursache gefunden (2026-10-15, spaeter am Tag):
    /// `third_party/ft8_lib/ft8/decode.c:205` sucht den Zeitversatz in
    /// einem FESTEN Bereich von -10 bis +19 SYMBOLEN. Mal die
    /// Symboldauer gerechnet:
    ///
    ///     FT8: 19 x 0,160 s = 3,04 s   Stille im Block 1,18 s  -> gefunden
    ///     FT4: 19 x 0,048 s = 0,91 s   Stille im Block 1,23 s  -> nicht
    ///
    /// Kein Fehler im eigenen Code, sondern die Reichweite der Suche.
    /// Im Betrieb beginnt eine FT4-Sendung rund 0,5 s nach der
    /// Fenstergrenze und liegt damit INNERHALB der 0,91 s — aber
    /// knapp. Ausfuehrlich in
    /// docs/architecture/2026-10-15-ft8-nativ.md.
    void ft4FindetSichOhneStilleDavor()
    {
        const QString text = QStringLiteral("CQ OE5SOS JN78");
        QVector<float> ton;
        QVERIFY(Ft8Decoder::encodeBlock(text, 1500.0f,
                                        Ft8Decoder::Protocol::Ft4, &ton));

        const int daten  = 60480;                 // 105 Toene a 0,048 s
        const int stille = (ton.size() - daten) / 2;
        const auto funde = Ft8Decoder::decodeBlock(ton.constData() + stille,
                                                   daten,
                                                   Ft8Decoder::Protocol::Ft4);
        QStringList wortlaute;
        for (const Ft8Decode& d : funde) { wortlaute << d.text; }
        qInfo().noquote() << "FT4 ohne Stille:" << wortlaute;
        QVERIFY2(wortlaute.contains(text),
                 qPrintable(QStringLiteral("FT4 gelesen: %1")
                                .arg(wortlaute.join(QLatin1String(" | ")))));

        // Und der Gegenbefund, damit er nicht in Vergessenheit geraet.
        const auto mitStille = Ft8Decoder::decodeBlock(ton.constData(),
                                                       ton.size(),
                                                       Ft8Decoder::Protocol::Ft4);
        qInfo() << "FT4 mit Stille:" << mitStille.size() << "Funde";
        QVERIFY2(mitStille.isEmpty(),
                 "FT4 findet sich jetzt AUCH mit Stille davor — dann ist "
                 "der Befund oben ueberholt und der Kommentar gehoert weg");
    }

    /// Ausserhalb des Durchlassbereichs ist nichts zu holen — das
    /// haelt fest, dass wirklich gesucht und nicht geraten wird.
    void ausserhalbDesDurchlassbereichsNichts()
    {
        QVector<float> ton;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ OE5SOS JN78"),
                                        4000.0f,
                                        Ft8Decoder::Protocol::Ft8, &ton));
        const auto funde = Ft8Decoder::decodeBlock(ton.constData(),
                                                   ton.size());
        qInfo() << "bei 4000 Hz gefunden:" << funde.size();
        QVERIFY2(funde.isEmpty(),
                 "ausserhalb von 200..3000 Hz darf nichts gefunden werden");
    }

    /// Stille ist Stille.
    void ausStilleKommtNichts()
    {
        QVector<float> ruhe(12000 * 15, 0.0f);
        const auto funde = Ft8Decoder::decodeBlock(ruhe.constData(),
                                                   ruhe.size());
        QVERIFY2(funde.isEmpty(),
                 "aus einem stummen Fenster kam eine Nachricht");
    }
};

QTEST_MAIN(TstFt8Dekodiert)
#include "tst_ft8_dekodiert.moc"
