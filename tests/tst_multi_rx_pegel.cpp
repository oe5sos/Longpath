// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die Summenbildung selbst ist in
// src/core/audio/MasterMixer.{h,cpp} gegen WDSPs aamix.c zitiert.
//
// Wie laut wird es, wenn mehrere Empfaenger mitlaufen? (2026-10-13)
//
// Zeus hat in 2.0.46 gemeldet: "Turning on Multi RX no longer makes the
// audio jump louder. Receivers that are playing now share the speaker
// level instead of stacking up, muted receivers don't count."
//
// Also nachgemessen, statt es anzunehmen -- die Regel
// [[feedback-abwesenheit-ist-keine-aussage]]: dass im Baum kein
// `normalizeAf` steht, heisst nicht, dass der Fehler fehlt, und ebenso
// wenig, dass er da ist.
//
// DAS ERGEBNIS (dieser Prueffall selbst):
//   ein Empfaenger   0,500 hinein -> 0,500 hinaus
//   zwei Empfaenger  0,500 je     -> 1,000 hinaus     == Faktor 2,000
//   drei, einer stumm             -> 1,000 hinaus     == der Stumme zaehlt nicht
//
// Die Pegel STAPELN sich also auch hier, und die zweite Haelfte von Zeus'
// Meldung (stumme zaehlen nicht mit) stimmt bei uns schon.
//
// NICHT GEAENDERT, und das ist Absicht:
//
//   * WDSP summiert genauso. `MasterMixer::tryDrain` macht
//     `out[i] += ring[r] * gain`, und das ist aamix.c nachgebaut
//     (Warren Pratt, NR0V) -- dort wird auch glatt summiert. ZEUS weicht
//     hier von der Vorlage ab, nicht wir.
//   * Es ist eine Entscheidung ueber den HOERPEGEL, und die gehoert dem
//     Betreiber: [[feedback-messung-schlaegt-nicht-das-geraet]] --
//     richtige Zahlen, falsches Ergebnis; sein Ohr entscheidet. Eine
//     Teilung durch die Zahl der Empfaenger macht den EINEN Empfaenger
//     nicht leiser, aber jeden weiteren -- wer zwei Baender gleich laut
//     hoeren will, verliert dabei.
//
// Dieser Fall nagelt darum das HEUTIGE Verhalten fest. Wird es einmal
// geaendert, faellt er -- und dann ist die Aenderung eine Entscheidung
// und kein Versehen.

#include <QtTest/QtTest>

#include "core/audio/MasterMixer.h"

#include <vector>

using namespace Longpath;

class TstMultiRxPegel : public QObject {
    Q_OBJECT

private:
    static constexpr int kFrames = 480;
    static constexpr float kEingang = 0.5f;

    /// Ein Mischer ohne Rampe und ohne Anlauffenster: die beiden wuerden
    /// den ersten Block daempfen und die Messung verwischen. Beide sind
    /// als Prueffall-Naht vorgesehen (setRampFrames/setSlewUpFrames).
    static void nackt(MasterMixer& m, int empfaenger)
    {
        m.setRampFrames(0);
        m.setSlewUpFrames(0);
        for (int i = 0; i < empfaenger; ++i) { m.setSliceGain(i, 1.0f, 0.0f); }
    }

private slots:
    void zweiEmpfaengerSindGENAUDoppeltSoLaut()
    {
        std::vector<float> block(static_cast<size_t>(kFrames) * 2, kEingang);
        std::vector<float> aus(static_cast<size_t>(kFrames) * 2, 0.0f);

        MasterMixer einer;
        nackt(einer, 1);
        einer.accumulate(0, block.data(), kFrames, false);
        QCOMPARE(einer.tryDrain(aus.data(), kFrames), kFrames);
        const float mitEinem = aus[0];
        QVERIFY2(qFuzzyCompare(mitEinem, kEingang),
                 qPrintable(QStringLiteral("ein Empfaenger gibt %1")
                                .arg(mitEinem)));

        std::fill(aus.begin(), aus.end(), 0.0f);
        MasterMixer zwei;
        nackt(zwei, 2);
        zwei.accumulate(0, block.data(), kFrames, false);
        zwei.accumulate(1, block.data(), kFrames, false);
        QCOMPARE(zwei.tryDrain(aus.data(), kFrames), kFrames);
        const float mitZwei = aus[0];

        // Das Verhaeltnis ist die Aussage, nicht der Absolutwert.
        QCOMPARE(mitZwei / mitEinem, 2.0f);
        QVERIFY2(qFuzzyCompare(mitZwei, 1.0f),
                 qPrintable(QStringLiteral("zwei Empfaenger geben %1")
                                .arg(mitZwei)));
    }

    void einStummerEmpfaengerZaehltNICHTMit()
    {
        // Die zweite Haelfte von Zeus' Meldung -- und die stimmt bei uns
        // schon: ein stummer Empfaenger traegt nichts bei, obwohl er
        // weiter mitgefuettert wird (was er muss, sonst haelt er die
        // Sperre auf; siehe MasterMixer.h, divergence 1).
        std::vector<float> block(static_cast<size_t>(kFrames) * 2, kEingang);
        std::vector<float> aus(static_cast<size_t>(kFrames) * 2, 0.0f);

        MasterMixer m;
        nackt(m, 3);
        m.accumulate(0, block.data(), kFrames, false);
        m.accumulate(1, block.data(), kFrames, false);
        m.accumulate(2, block.data(), kFrames, /*muted=*/true);
        QCOMPARE(m.tryDrain(aus.data(), kFrames), kFrames);

        // Drei angemeldet, zwei hoerbar -> derselbe Pegel wie bei zwei.
        QVERIFY2(qFuzzyCompare(aus[0], 1.0f),
                 qPrintable(QStringLiteral("drei (einer stumm) geben %1 -- "
                                           "erwartet 1,0 wie bei zwei")
                                .arg(aus[0])));
        QCOMPARE(m.producingSliceCount(), 3);
    }
};

QTEST_MAIN(TstMultiRxPegel)
#include "tst_multi_rx_pegel.moc"
