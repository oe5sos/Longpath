// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_eq_peak_hold_stands_still.cpp  (Longpath)
// =================================================================
// Haltet heisst haltet.
//
// Der Betreiber am 2026-10-09, nachdem die Entzerrerkurve zum ersten
// Mal seine eigene Stimme zeigte: „die eigene kurve sollte auch
// laengere zeit mit hold stehen bleiben und standhaft bleiben, so
// kann man danach ohne stress diese aendern."
//
// Die portierte Fassung tat das NICHT. „Eingefroren" hielt dort nur
// den Abfall an; das Maximum je Fach wurde weiter nachgezogen. Jedes
// Geraeusch im Raum hob die eingefrorene Linie also weiter an -- und
// weil sie nie wieder faellt, war sie nach einer Minute nicht mehr
// die Stimme, sondern die Hoechstmarke von allem, was seither im
// Zimmer passiert ist. Ein Bezug, der sich unter der Hand aendert,
// ist schlimmer als keiner: man legt eine Kurve dagegen und weiss
// hinterher nicht, wogegen.
//
// Zwei Faelle, beide fallen gegen die alte Fassung durch.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "gui/applets/eq/ClientEqCurveWidget.h"

#include <vector>

using namespace Longpath;

namespace {

std::vector<float> flat(float db, size_t bins = 256)
{
    return std::vector<float>(bins, db);
}

} // namespace

class TstEqPeakHoldStandsStill : public QObject
{
    Q_OBJECT
private slots:

    // Eingefroren bewegt sich nichts -- auch dann nicht, wenn es
    // danach deutlich lauter wird.
    void frozenMeansFrozenEvenWhenItGetsLouder()
    {
        ClientEqCurveWidget w;
        w.setFftBinsDb(flat(-40.0f), 48000.0);
        w.setPeakHoldFrozen(true);
        const std::vector<float> held = w.peakHoldDbForTest();
        QVERIFY(!held.empty());

        // Dreissig Bilder lang zwanzig Dezibel lauter. Die alte
        // Fassung zog hier auf -20 nach.
        for (int i = 0; i < 30; ++i) {
            w.setFftBinsDb(flat(-20.0f), 48000.0);
        }
        QCOMPARE(w.peakHoldDbForTest(), held);
    }

    // Und frei laufend bleibt die Spitze lange genug stehen, dass man
    // einen Satz sprechen und DANN hinsehen kann. Upstream fiel mit
    // 12,5 dB/s -- nach einer Sekunde war nichts mehr da.
    void anUnfrozenPeakSurvivesLongEnoughToLookAt()
    {
        ClientEqCurveWidget w;
        w.setFftBinsDb(flat(-20.0f), 48000.0);
        const float peak = w.peakHoldDbForTest().at(0);
        QVERIFY(std::abs(peak - (-20.0f)) < 0.01f);

        // Eine Sekunde Stille bei 25 Bildern.
        for (int i = 0; i < 25; ++i) {
            w.setFftBinsDb(flat(-60.0f), 48000.0);
        }
        QVERIFY2(w.peakHoldDbForTest().at(0) > -21.0f,
                 qPrintable(QStringLiteral("nach einer Sekunde nur noch %1 dB")
                     .arg(double(w.peakHoldDbForTest().at(0)))));

        // Nach fuenf Sekunden darf sie gefallen sein -- stehen
        // bleiben soll sie ja nicht fuer immer.
        for (int i = 0; i < 125; ++i) {
            w.setFftBinsDb(flat(-60.0f), 48000.0);
        }
        QVERIFY(w.peakHoldDbForTest().at(0) < -21.0f);
    }
};

QTEST_MAIN(TstEqPeakHoldStandsStill)
#include "tst_eq_peak_hold_stands_still.moc"
