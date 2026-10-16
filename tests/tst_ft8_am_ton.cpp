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
};

QTEST_MAIN(TstFt8AmTon)
#include "tst_ft8_am_ton.moc"
