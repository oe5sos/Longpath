// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tools/sunsdr_tx_pacer_jitter_probe.cpp  (Longpath)
// =================================================================
//
// WERKZEUG, kein Programmteil: Schritt 5 der SunSDR2-QRP-TX-Kette.
//
// ── Warum es das gibt ───────────────────────────────────────────────
//
// SunSdrTxPacer (src/core/sunsdr/SunSdrTxPacer.h) soll TX-I/Q-Pakete im
// Takt von 5,12 ms (195,3125 pps) versenden. Qt6 kennt aber keine
// gebrochenen Millisekunden fuer QTimer::setInterval() — der Pacer
// rundet auf 5 ms und setzt Qt::PreciseTimer, ohne zu behaupten, dass
// das exakt 5,12 ms trifft. Der Entwurfs-Workflow, der diese TX-Kette
// autorisiert hat, hat bewusst KEINEN eigenen OS-Thread fuer diesen
// Pacer gebaut (das waere eine Architekturentscheidung, die laut
// CLAUDE.md eine Freigabe braucht) — stattdessen wird hier zuerst
// gemessen, ob ein simpler QTimer im echten Qt-Event-Loop nah genug an
// den Zielwert kommt.
//
// Dieses Werkzeug braucht KEIN Funkgeraet. Es misst nur, wie praezise
// der Timer selbst tickt — SunSdrTxPacer sendet ohnehin auf keinen
// Socket (siehe seine eigene Kopfkommentar-Erklaerung), das gilt auch
// hier unveraendert.
//
// ── Aufruf ──────────────────────────────────────────────────────────
//
//   sunsdr_tx_pacer_jitter_probe [sekunden]     (Default: 30)
//
// Gibt am Ende eine kurze Statistik aus: Anzahl Ticks, Mittelwert,
// Standardabweichung, minimale/maximale Abweichung vom 5-ms-Zielwert,
// in Millisekunden.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-02 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork). Schritt 5
//                der freigegebenen 6-Schritte-SunSDR2-QRP-TX-Kette.
// =================================================================

#include "core/sunsdr/SunSdrTxPacer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

using Longpath::SunSdrTxPacer;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const double durationSeconds = (argc > 1) ? std::atof(argv[1]) : 30.0;

    SunSdrTxPacer pacer;

    QElapsedTimer clock;
    clock.start();
    std::vector<qint64> tickTimestampsNs;
    tickTimestampsNs.reserve(static_cast<std::size_t>(durationSeconds * 250));

    QObject::connect(&pacer, &SunSdrTxPacer::tickFiredForTest,
                      [&tickTimestampsNs, &clock]() {
                          tickTimestampsNs.push_back(clock.nsecsElapsed());
                      });

    std::printf("sunsdr_tx_pacer_jitter_probe: messe %.1f s lang, "
                "Zielintervall %d ms (angefragt fuer 5,12 ms) ...\n",
                durationSeconds, SunSdrTxPacer::kTxPaceIntervalMs);
    std::fflush(stdout);

    pacer.start();

    QTimer::singleShot(static_cast<int>(durationSeconds * 1000), &app,
                        [&app]() { app.quit(); });
    app.exec();

    pacer.stop();

    if (tickTimestampsNs.size() < 2) {
        std::printf("sunsdr_tx_pacer_jitter_probe: zu wenige Ticks (%zu) fuer "
                    "eine Auswertung -- lief der Timer ueberhaupt?\n",
                    tickTimestampsNs.size());
        return 1;
    }

    std::vector<double> intervalsMs;
    intervalsMs.reserve(tickTimestampsNs.size() - 1);
    for (std::size_t i = 1; i < tickTimestampsNs.size(); ++i) {
        intervalsMs.push_back(
            static_cast<double>(tickTimestampsNs[i] - tickTimestampsNs[i - 1]) / 1'000'000.0);
    }

    const double sum = std::accumulate(intervalsMs.begin(), intervalsMs.end(), 0.0);
    const double mean = sum / static_cast<double>(intervalsMs.size());

    double sqDiffSum = 0.0;
    for (double v : intervalsMs) {
        sqDiffSum += (v - mean) * (v - mean);
    }
    const double stddev = std::sqrt(sqDiffSum / static_cast<double>(intervalsMs.size()));

    const auto [minIt, maxIt] = std::minmax_element(intervalsMs.begin(), intervalsMs.end());
    const double minMs = *minIt;
    const double maxMs = *maxIt;

    constexpr double kTargetMs = 5.12;  // design doc's real target, not the rounded 5 ms request
    const double maxDeviationFromTargetMs =
        std::max(std::abs(maxMs - kTargetMs), std::abs(minMs - kTargetMs));

    // Perzentile: der Mittelwert allein verschleiert, WIE OFT ein Ausreisser
    // vorkommt (ein einzelner 9-ms-Tick unter 12000 sagt wenig ueber die
    // Haeufigkeit). Sortierte Kopie, damit ein Blick auf p50/p95/p99/p99.9
    // zeigt, ob grosse Abweichungen selten oder regelmaessig sind.
    std::vector<double> sorted = intervalsMs;
    std::sort(sorted.begin(), sorted.end());
    auto percentile = [&sorted](double p) -> double {
        const std::size_t idx = static_cast<std::size_t>(
            p * static_cast<double>(sorted.size() - 1));
        return sorted[idx];
    };

    int countOverOneAndAHalfTarget = 0;   // > 7.68 ms (1.5x das 5.12-ms-Ziel)
    int countOverDoubleTarget = 0;        // > 10.24 ms (2x das Ziel)
    for (double v : intervalsMs) {
        if (v > 1.5 * kTargetMs) { ++countOverOneAndAHalfTarget; }
        if (v > 2.0 * kTargetMs) { ++countOverDoubleTarget; }
    }

    std::printf("\n--- Ergebnis ---\n");
    std::printf("Ticks gesamt:            %zu\n", tickTimestampsNs.size());
    std::printf("Intervalle ausgewertet:  %zu\n", intervalsMs.size());
    std::printf("Mittelwert:              %.4f ms  (angefragt: %d ms, Ziel: %.2f ms)\n",
                mean, SunSdrTxPacer::kTxPaceIntervalMs, kTargetMs);
    std::printf("Standardabweichung:      %.4f ms\n", stddev);
    std::printf("Minimum:                 %.4f ms\n", minMs);
    std::printf("Maximum:                 %.4f ms\n", maxMs);
    std::printf("Max. Abweichung vom Ziel (%.2f ms): %.4f ms\n",
                kTargetMs, maxDeviationFromTargetMs);
    std::printf("p50 / p95 / p99 / p99.9: %.4f / %.4f / %.4f / %.4f ms\n",
                percentile(0.50), percentile(0.95), percentile(0.99), percentile(0.999));
    std::printf("Intervalle > 1.5x Ziel (%.2f ms): %d von %zu (%.3f %%)\n",
                1.5 * kTargetMs, countOverOneAndAHalfTarget, intervalsMs.size(),
                100.0 * countOverOneAndAHalfTarget / static_cast<double>(intervalsMs.size()));
    std::printf("Intervalle > 2.0x Ziel (%.2f ms): %d von %zu (%.3f %%)\n",
                2.0 * kTargetMs, countOverDoubleTarget, intervalsMs.size(),
                100.0 * countOverDoubleTarget / static_cast<double>(intervalsMs.size()));

    return 0;
}
