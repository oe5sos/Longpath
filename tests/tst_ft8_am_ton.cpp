// SPDX-License-Identifier: GPL-3.0-or-later
//
// FT8 am laufenden Ton — die Uhr ist der Punkt.
//
// Ft8Decoder ist eine reine Funktion und als solche schon geprueft
// (tst_ft8_dekodiert). Im Betrieb fehlen zwei Dinge: WANN ein Fenster
// zu Ende ist, und WOHER die Proben kommen.
//
// FT8 teilt die Minute in vier Fenster zu 15 Sekunden, und zwar nach
// UTC ab der vollen Minute. Wer die Grenze verfehlt, dekodiert nichts
// und sucht den Fehler beim Funkgeraet — genau deshalb ist die Uhr in
// Ft8Stream austauschbar und dieser Lauf wartet nicht fuenfzehn
// Sekunden, sondern laesst die Zeit springen.
//
// Der Ton kommt hier so herein, wie ihn WDSP ausgibt: Stereo, 48 kHz,
// derselbe Abgriff wie beim CW-Dekoder.
//
// Gegenprobe (2026-10-15): ohne den Grenzuebertritt (Uhr steht still)
// meldet der Lauf nichts — das Fenster wird nie geschlossen.

#include <QtTest>
#include <QSignalSpy>

#include "core/Ft8Decoder.h"
#include "core/Ft8Stream.h"

using namespace Longpath;

class TstFt8AmTon : public QObject
{
    Q_OBJECT

private:
    /// Ein ganzes FT8-Fenster als Stereo mit 48 kHz, so wie es aus
    /// WDSP kaeme.
    static QVector<float> fensterAlsStereo48k(const QString& text)
    {
        QVector<float> mono12k;
        if (!Ft8Decoder::encodeBlock(text, 1500.0f,
                                     Ft8Decoder::Protocol::Ft8, &mono12k)) {
            return {};
        }
        // Einfaches Hochtasten um den Faktor vier: jede Probe viermal.
        // Grob, aber fuer 1500 Hz weit unterhalb der neuen Nyquist-
        // Grenze und damit gut genug — der Umtaster im Stream rechnet
        // es ohnehin wieder sauber herunter.
        QVector<float> stereo;
        stereo.reserve(mono12k.size() * 8);
        for (float v : mono12k) {
            for (int k = 0; k < 4; ++k) { stereo << v << v; }
        }
        return stereo;
    }

    /// Ein FT4-Fenster (7,5 s) als Stereo mit 48 kHz, in dem die
    /// Nachricht `versatzMs` nach der Fenstergrenze beginnt — so wie
    /// eine echte FT4-Sendung, die rund 0,5 s nach der Grenze anfaengt
    /// und auf dem Weg durch WDSP, Umtaster und Ring noch Laufzeit
    /// mitbekommt.
    static QVector<float> ft4FensterAlsStereo48k(const QString& text,
                                                 int versatzMs)
    {
        QVector<float> ganz12k;
        if (!Ft8Decoder::encodeBlock(text, 1500.0f,
                                     Ft8Decoder::Protocol::Ft4, &ganz12k)) {
            return {};
        }
        // encodeBlock legt die Daten MITTIG in sein Fenster (wie
        // demo/gen_ft8.c). Fuer diesen Lauf brauchen wir sie blank,
        // um sie selbst an die richtige Stelle zu setzen.
        const int daten = 60480;                       // 105 Toene a 0,048 s
        if (ganz12k.size() < daten) { return {}; }
        const int stilleVorn = (ganz12k.size() - daten) / 2;

        const int rate = Ft8Decoder::kSampleRate;
        const int fenster = rate * 7500 / 1000;
        QVector<float> mono12k(fenster, 0.0f);
        const int start = rate * versatzMs / 1000;
        for (int i = 0; i < daten; ++i) {
            const int z = start + i;
            if (z >= 0 && z < fenster) {
                mono12k[z] = ganz12k[stilleVorn + i];
            }
        }

        QVector<float> stereo;
        stereo.reserve(mono12k.size() * 8);
        for (float v : mono12k) {
            for (int k = 0; k < 4; ++k) { stereo << v << v; }
        }
        return stereo;
    }

    /// Ein FT4-Fenster durch den Strom schicken und die Wortlaute
    /// zurueckgeben. Leere Liste heisst: nichts gefunden.
    static QStringList ft4DurchDenStrom(const QString& text, int versatzMs)
    {
        const QVector<float> ton = ft4FensterAlsStereo48k(text, versatzMs);
        if (ton.isEmpty()) { return {}; }

        Ft8Stream strom;
        strom.setProtocol(Ft8Decoder::Protocol::Ft4);
        // 999 999 990 000 ms ist durch 7 500 teilbar — eine echte
        // FT4-Fenstergrenze.
        qint64 jetzt = 999999990000LL;
        strom.setClock([&jetzt]() { return jetzt; });
        QSignalSpy spion(&strom, &Ft8Stream::decoded);

        const int stueck = 4096;
        for (int pos = 0; pos + 2 * stueck <= ton.size(); pos += 2 * stueck) {
            strom.feedAudio(ton.constData() + pos, stueck);
        }
        jetzt += 7500;
        strom.feedAudio(ton.constData(), stueck);

        QStringList wortlaute;
        if (spion.count() != 1) { return wortlaute; }
        const auto funde =
            spion.at(0).at(0).value<QVector<Longpath::Ft8Decode>>();
        for (const Ft8Decode& d : funde) { wortlaute << d.text; }
        return wortlaute;
    }

private slots:
    void anDerFenstergrenzeWirdDekodiert()
    {
        const QString text = QStringLiteral("CQ OE5SOS JN78");
        const QVector<float> ton = fensterAlsStereo48k(text);
        QVERIFY2(!ton.isEmpty(), "kein Pruefton erzeugt");

        Ft8Stream strom;
        // Eine Uhr, die wir in der Hand haben. Start auf einer
        // Fenstergrenze: 1 000 000 000 000 ms ist durch 15 000 teilbar.
        // 999 999 990 000 ms ist durch 15 000 teilbar — eine echte
        // Fenstergrenze, keine ungefaehre.
        qint64 jetzt = 999999990000LL;
        QVERIFY2(jetzt % 15000 == 0, "Startzeit liegt nicht auf der Grenze");
        strom.setClock([&jetzt]() { return jetzt; });

        QSignalSpy spion(&strom, &Ft8Stream::decoded);

        // Das ganze Fenster hineingeben, in Stuecken wie im Betrieb.
        const int stueck = 4096;
        for (int pos = 0; pos + 2 * stueck <= ton.size(); pos += 2 * stueck) {
            strom.feedAudio(ton.constData() + pos, stueck);
        }
        qInfo() << "im Fenster:" << strom.bufferedSamples() << "Proben bei 12 kHz";
        QVERIFY2(strom.bufferedSamples() > 100000,
                 "zu wenig im Fenster — der Umtaster hat nichts geliefert");

        // Noch steht nichts: das Fenster ist nicht zu Ende.
        QCOMPARE(spion.count(), 0);

        // Jetzt ueber die Grenze. Ein kurzer Nachschub reicht, um den
        // Uebertritt zu melden.
        jetzt += 15000;
        strom.feedAudio(ton.constData(), stueck);

        QVERIFY2(spion.count() == 1,
                 qPrintable(QStringLiteral("%1 Meldungen statt einer")
                                .arg(spion.count())));
        const auto funde =
            spion.at(0).at(0).value<QVector<Longpath::Ft8Decode>>();
        QStringList wortlaute;
        for (const Ft8Decode& d : funde) { wortlaute << d.text; }
        qInfo().noquote() << "gelesen:" << wortlaute;
        QVERIFY2(wortlaute.contains(text),
                 qPrintable(QStringLiteral("gelesen: %1")
                                .arg(wortlaute.join(QLatin1String(" | ")))));

        // Und die Anfangszeit des Fensters muss stimmen — ohne sie
        // laesst sich ein Decode nicht eintragen.
        const QDateTime beginn = spion.at(0).at(1).toDateTime();
        QCOMPARE(beginn.toMSecsSinceEpoch(), 999999990000LL);
    }

    /// Steht die Uhr, wird kein Fenster geschlossen — und nichts
    /// gemeldet. Das ist die Gegenprobe zur Grenze.
    void ohneGrenzuebertrittNichts()
    {
        const QVector<float> ton =
            fensterAlsStereo48k(QStringLiteral("CQ OE5SOS JN78"));
        QVERIFY(!ton.isEmpty());

        Ft8Stream strom;
        qint64 jetzt = 999999990000LL;
        strom.setClock([&jetzt]() { return jetzt; });
        QSignalSpy spion(&strom, &Ft8Stream::decoded);

        for (int pos = 0; pos + 8192 <= ton.size(); pos += 8192) {
            strom.feedAudio(ton.constData() + pos, 4096);
        }
        QCOMPARE(spion.count(), 0);
    }

    /// FT4 hat ein halb so langes Fenster. Steht das nicht, liegt
    /// jede Grenze falsch.
    void ft4HatEinHalbesFenster()
    {
        Ft8Stream strom;
        QCOMPARE(strom.slotMs(), 15000);
        strom.setProtocol(Ft8Decoder::Protocol::Ft4);
        QCOMPARE(strom.slotMs(), 7500);
    }

    /// ── FT4 haelt auch eine verspaetete Sendung (2026-10-17) ───────
    ///
    /// Die Zahlen und der Grund stehen in Ft8Stream.h bei
    /// kFt4BlockVersatzMs: ft8_lib sucht den Zeitversatz nur von -10
    /// bis +19 Symbolen, bei FT4 also -0,48 bis +0,91 s. Eine
    /// FT4-Sendung beginnt rund 0,5 s nach der Grenze und hatte damit
    /// nur 0,4 s Luft nach oben, waehrend unten 1,0 s frei lagen.
    ///
    /// Geprueft wird nicht die Luft, sondern was sie traegt: eine
    /// Sendung, die um 0,5 s zu spaet kommt (Soll +500 ms, hier
    /// +1000 ms), muss gefunden werden. Ohne den Versatz liegt sie
    /// ausserhalb des Suchbands und wird NICHT gefunden — das ist die
    /// Gegenprobe, und sie ist am 2026-10-17 gefahren worden.
    void ft4FindetEineVerspaeteteSendung_data()
    {
        QTest::addColumn<int>("versatzMs");
        QTest::addColumn<bool>("erwartet");

        // Die Soll-Sendezeit. Ging vorher auch und muss weiter gehen —
        // ein Versatz, der nur den oberen Rand kauft und den unteren
        // verkauft, waere kein Gewinn.
        QTest::newRow("Soll, +500 ms")        << 500  << true;
        // Frueh: eine Station, deren Uhr nachgeht, oder ein Fenster,
        // das zu frueh geschlossen wird. -100 ms liegt nach dem
        // Versatz noch im Band (-200 ms ist der Rand).
        QTest::newRow("frueh, -100 ms")       << -100 << true;
        // Der Fall, um den es geht: Soll plus eine halbe Sekunde
        // Laufzeit. Vorher ausserhalb des Bands (+900 ms war der
        // Rand), jetzt mittendrin.
        QTest::newRow("verspaetet, +1000 ms") << 1000 << true;
        // Und einer, der auch mit dem Versatz nicht mehr geht —
        // sonst behauptet dieser Lauf, das Band sei unendlich.
        QTest::newRow("viel zu spaet, +1500 ms") << 1500 << false;
    }

    void ft4FindetEineVerspaeteteSendung()
    {
        QFETCH(int, versatzMs);
        QFETCH(bool, erwartet);

        const QString text = QStringLiteral("CQ OE5SOS JN78");
        const QStringList wortlaute = ft4DurchDenStrom(text, versatzMs);
        qInfo().noquote() << "FT4 bei" << versatzMs << "ms ->" << wortlaute;

        if (erwartet) {
            QVERIFY2(wortlaute.contains(text),
                     qPrintable(QStringLiteral(
                         "bei %1 ms nicht gefunden (gelesen: %2) — das "
                         "Suchband von ft8_lib liegt falsch, siehe "
                         "Ft8Stream::kFt4BlockVersatzMs")
                         .arg(versatzMs)
                         .arg(wortlaute.join(QLatin1String(" | ")))));
        } else {
            QVERIFY2(!wortlaute.contains(text),
                     qPrintable(QStringLiteral(
                         "bei %1 ms GEFUNDEN — dann ist das gemessene "
                         "Suchband breiter als angenommen und die Rechnung "
                         "in Ft8Stream.h gehoert nachgezogen")
                         .arg(versatzMs)));
        }
    }
};

QTEST_MAIN(TstFt8AmTon)
#include "tst_ft8_am_ton.moc"
