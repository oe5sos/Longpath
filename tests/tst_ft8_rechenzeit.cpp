// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wie lange dauert ein ganzes FT8-Fenster?
//
// Die Frage ist nicht akademisch: Ft8Stream ruft decodeBlock dort, wo
// der Fensterwechsel auffaellt — und wenn das im Faden der Oberflaeche
// geschieht, steht das Programm alle fuenfzehn Sekunden fuer genau
// diese Dauer still. Der CW-Dekoder hat aus demselben Grund einen
// eigenen Arbeiterfaden.
//
// Gemessen wird das, was im Betrieb vorkommt: fuenfzehn Sekunden bei
// 12 kHz, Rauschen, EIN Signal darin — nicht der saubere Prueftonen
// von tst_ft8_dekodiert (der ist kuerzer und viel leichter).
//
// Gemessen am 2026-10-15 auf dem M-Mac: 4 ms fuer ein Fenster mit einem
// Signal (dieser Lauf im Projektbau), 17-23 ms fuer eines mit
// fuenfundzwanzig (Handmessung, eigens uebersetzt). Damit ist die
// Frage beantwortet: das darf im Faden der Oberflaeche laufen, ein
// Arbeiterfaden waere Aufwand ohne Gegenwert.
//
// Die Decke ist bewusst weit: sie faengt die Verschlechterung ab
// („ploetzlich hundertmal so lang"), nicht die Schwankung — auf einem
// langsameren Rechner, unter Last, mit kaelterem Zwischenspeicher
// sind 100 ms kein Befund. Die gemessene Zahl steht im Protokoll des
// Laufs, nicht in einer Nachricht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QElapsedTimer>

#include "core/Ft8Decoder.h"

#include <random>

using namespace Longpath;

class TstFt8Rechenzeit : public QObject
{
    Q_OBJECT

private slots:
    void einGanzesFensterBleibtUnterDerDecke()
    {
        QVector<float> ton;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ OE5SOS JN78"),
                                        1500.0f, Ft8Decoder::Protocol::Ft8,
                                        &ton));

        // Fuenfzehn Sekunden Rauschen mit einem Signal darin.
        QVector<float> block(Ft8Decoder::kSampleRate * 15, 0.0f);
        std::mt19937 rng(7);
        std::normal_distribution<float> rauschen(0.0f, 0.15f);
        for (int i = 0; i < block.size(); ++i) { block[i] = rauschen(rng); }
        for (int i = 0; i < ton.size() && i < block.size(); ++i) {
            block[i] += 0.3f * ton[i];
        }

        qint64 kuerzeste = -1;
        for (int lauf = 0; lauf < 3; ++lauf) {
            QElapsedTimer t;
            t.start();
            const auto funde = Ft8Decoder::decodeBlock(block.constData(),
                                                       block.size());
            const qint64 ms = t.elapsed();
            qInfo() << "Lauf" << lauf << ":" << ms << "ms," << funde.size()
                    << "Funde";
            if (kuerzeste < 0 || ms < kuerzeste) { kuerzeste = ms; }
        }

        // Ein Fenster ist 15 000 ms lang. Laenger als ein Drittel davon
        // und der Dekoder kommt dem naechsten Fenster ins Gehege —
        // UND die Oberflaeche stuende so lange still.
        QVERIFY2(kuerzeste < 5000,
                 qPrintable(QStringLiteral("%1 ms fuer ein Fenster")
                                .arg(kuerzeste)));
    }
};

QTEST_MAIN(TstFt8Rechenzeit)
#include "tst_ft8_rechenzeit.moc"
