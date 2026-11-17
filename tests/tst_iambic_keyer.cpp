// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_iambic_keyer.cpp  (Longpath)
// =================================================================
//
// Longpath-original test fuer den aus AetherSDR uebernommenen
// Jambus-Geber (src/core/cw/IambicKeyer). Ein kurzer Lauf in Echtzeit:
// Punkt, Strich, und das Zusammendruecken in Art A und B.
//
// Gemessen wird am GEPLANTEN Zeitpunkt jeder Kante (dem zweiten Argument
// des Rueckrufs), nicht am Aufwachen des Fadens -- das Raster ist genau,
// auch auf einem belasteten CI-Laeufer. Tempo 30 WpM: ein Punkt ist
// 40 ms, das laesst dem Druecken und Loslassen aus dem Pruefstand genug
// Luft.
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test; the keyer itself is registered
// in AETHERSDR-PORTS.md.

#include <QtTest>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "core/cw/IambicKeyer.h"

using namespace Longpath;
using Uhr = std::chrono::steady_clock;

namespace {

constexpr int kWpm = 30;
constexpr double kPunktMs = 1200.0 / kWpm;   // 40 ms

struct Kante {
    bool unten;
    Uhr::time_point wann;
};

struct Mitschrift {
    std::mutex mu;
    std::vector<Kante> kanten;

    void haenge(IambicKeyer& geber)
    {
        geber.setOnKeyDownChange([this](bool unten, Uhr::time_point wann) {
            std::lock_guard<std::mutex> lk(mu);
            kanten.push_back({unten, wann});
        });
    }
    std::vector<Kante> lies()
    {
        std::lock_guard<std::mutex> lk(mu);
        return kanten;
    }
    int zahl() { return int(lies().size()); }
};

double ms(Uhr::time_point a, Uhr::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

void warte(int millis)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

// Die Dauern der Zeichen (unten -> oben) in Punktlaengen.
QList<int> elemente(const std::vector<Kante>& k)
{
    QList<int> laengen;
    for (size_t i = 0; i + 1 < k.size(); i += 2) {
        laengen.append(qRound(ms(k[i].wann, k[i + 1].wann) / kPunktMs));
    }
    return laengen;
}

} // namespace

class TstIambicKeyer : public QObject {
    Q_OBJECT

private slots:
    void einPunkt()
    {
        IambicKeyer geber;
        Mitschrift m;
        m.haenge(geber);
        geber.setWpm(kWpm);
        geber.start();
        geber.setPaddleState(true, false);
        warte(20);
        geber.setPaddleState(false, false);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 2, 2000);
        warte(150);   // es kommt nichts nach
        const auto k = m.lies();
        geber.stop();
        QCOMPARE(int(k.size()), 2);
        QVERIFY(k[0].unten);
        QVERIFY(!k[1].unten);
        const double dauer = ms(k[0].wann, k[1].wann);
        QVERIFY2(std::abs(dauer - kPunktMs) < 2.0, qPrintable(QString::number(dauer)));
    }

    void einStrichIstDreiPunkte()
    {
        IambicKeyer geber;
        Mitschrift m;
        m.haenge(geber);
        geber.setWpm(kWpm);
        geber.start();
        geber.setPaddleState(false, true);
        warte(20);
        geber.setPaddleState(false, false);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 2, 2000);
        const auto k = m.lies();
        geber.stop();
        const double dauer = ms(k[0].wann, k[1].wann);
        QVERIFY2(std::abs(dauer - 3.0 * kPunktMs) < 2.0, qPrintable(QString::number(dauer)));
    }

    // Punkt halten, Strich dazu, beide im Strich loslassen. Art A hoert
    // mit dem Strich auf: Punkt, Strich. Die Pause dazwischen ist ein
    // Punkt, auf dem Raster.
    void zusammendrueckenArtA()
    {
        IambicKeyer geber;
        Mitschrift m;
        m.haenge(geber);
        geber.setMode(IambicKeyer::Mode::IambicA);
        geber.setWpm(kWpm);
        geber.start();
        geber.setPaddleState(true, false);
        warte(10);
        geber.setPaddleState(true, true);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 3, 2000);   // der Strich hat begonnen
        warte(30);
        geber.setPaddleState(false, false);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 4, 2000);
        warte(250);
        const auto k = m.lies();
        geber.stop();
        QCOMPARE(elemente(k), (QList<int>{1, 3}));
        const double pause = ms(k[1].wann, k[2].wann);
        QVERIFY2(std::abs(pause - kPunktMs) < 2.0, qPrintable(QString::number(pause)));
    }

    // Dasselbe in Art B: der beim Strich noch gehaltene Punkt ist
    // gemerkt und kommt nach dem Loslassen noch -- Punkt, Strich, Punkt.
    void zusammendrueckenArtB()
    {
        IambicKeyer geber;
        Mitschrift m;
        m.haenge(geber);
        geber.setMode(IambicKeyer::Mode::IambicB);
        geber.setWpm(kWpm);
        geber.start();
        geber.setPaddleState(true, false);
        warte(10);
        geber.setPaddleState(true, true);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 3, 2000);
        warte(30);
        geber.setPaddleState(false, false);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 6, 2000);
        warte(250);
        const auto k = m.lies();
        geber.stop();
        QCOMPARE(elemente(k), (QList<int>{1, 3, 1}));
    }

    void paddlesTauschen()
    {
        IambicKeyer geber;
        Mitschrift m;
        m.haenge(geber);
        geber.setWpm(kWpm);
        geber.setSwapPaddles(true);
        geber.start();
        geber.setPaddleState(true, false);   // der „Punkt"-Kontakt gibt jetzt den Strich
        warte(20);
        geber.setPaddleState(false, false);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 2, 2000);
        const auto k = m.lies();
        geber.stop();
        QCOMPARE(elemente(k), (QList<int>{3}));
    }

    // Anhalten mitten im Zeichen: die Taste geht hoch, nicht erst am
    // Ende des Strichs.
    void anhaltenNimmtDieTasteHoch()
    {
        IambicKeyer geber;
        Mitschrift m;
        m.haenge(geber);
        geber.setWpm(5);   // ein Strich dauert 720 ms
        geber.start();
        geber.setPaddleState(false, true);
        QTRY_COMPARE_WITH_TIMEOUT(m.zahl(), 1, 2000);
        geber.stop();
        const auto k = m.lies();
        QCOMPARE(int(k.size()), 2);
        QVERIFY(!k[1].unten);
    }
};

QTEST_GUILESS_MAIN(TstIambicKeyer)
#include "tst_iambic_keyer.moc"
