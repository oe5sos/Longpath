// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_iambic_keyer.cpp  (Longpath)
// =================================================================
//
// Longpath-original test fuer den aus AetherSDR uebernommenen
// Jambus-Geber (src/core/cw/IambicKeyer). Ein Lauf in Echtzeit: Punkt,
// Strich, und das Zusammendruecken in Art A und B.
//
// ── Warum 5 WpM, und warum drei Versuche ─────────────────────────────
//
// Die erste Fassung lief mit 30 WpM (Punkt 40 ms) und fiel auf der
// macOS-CI sofort um (2026-11-17): ein Punkt mass 88 ms, ein Strich 280.
// Nicht der Geber war falsch, sondern die Annahme, der Laeufer wecke
// einen Faden puenktlich. Der Geber plant auf einem Raster und gibt
// jeder Kante ihren GEPLANTEN Zeitpunkt mit -- aber wacht er um mehr als
// eine Punktlaenge zu spaet auf, rueckt er das Raster nach, und die
// Kante traegt die Verspaetung. Auf einem Laeufer, der sechshundert
// Pruefstaende nebeneinander faehrt, sind 50 bis 160 ms Stillstand
// gewoehnlich.
//
// Darum zweierlei:
//
//   * 5 WpM, das langsamste Tempo: ein Punkt ist 240 ms. Erst ein
//     Stillstand ueber 240 ms verschiebt das Raster, und der Pruefstand
//     hat fuer jedes Druecken und Loslassen Hunderte Millisekunden Luft.
//   * Was vom Takt des Laeufers NICHT abhaengt, wird hart geprueft:
//     Zahl und Reihenfolge der Kanten, und dass kein Zeichen KUERZER ist
//     als geplant. Die genaue Dauer darf bis zu dreimal versucht werden
//     -- die Logik ist dieselbe in jedem Versuch; ein echter Fehler
//     faellt dreimal, ein Stillstand des Laeufers nicht.
//
// Der Lauf faehrt ausserdem fuer sich (RUN_SERIAL in tests/CMakeLists.txt).
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test; the keyer itself is registered
// in AETHERSDR-PORTS.md.

#include <QtTest>

#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "core/cw/IambicKeyer.h"

using namespace Longpath;
using Uhr = std::chrono::steady_clock;

namespace {

constexpr int kWpm = 5;
constexpr double kPunktMs = 1200.0 / kWpm;   // 240 ms
constexpr double kGenauMs = 3.0;
constexpr int kVersuche = 3;
constexpr int kFristMs = 8000;

struct Kante {
    bool unten;
    Uhr::time_point wann;
};

double ms(Uhr::time_point a, Uhr::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Ein Geber samt Mitschrift seiner Kanten.
struct Stand {
    IambicKeyer geber;
    std::mutex mu;
    std::vector<Kante> kanten;

    explicit Stand(IambicKeyer::Mode art, bool tausch = false)
    {
        geber.setOnKeyDownChange([this](bool unten, Uhr::time_point wann) {
            std::lock_guard<std::mutex> lk(mu);
            kanten.push_back({unten, wann});
        });
        geber.setMode(art);
        geber.setWpm(kWpm);
        geber.setSwapPaddles(tausch);
        geber.start();
    }
    ~Stand() { geber.stop(); }

    std::vector<Kante> lies()
    {
        std::lock_guard<std::mutex> lk(mu);
        return kanten;
    }
    int zahl() { return int(lies().size()); }

    // Warten, bis so viele Kanten da sind.
    bool warteAuf(int n)
    {
        const Uhr::time_point ende = Uhr::now() + std::chrono::milliseconds(kFristMs);
        while (zahl() < n) {
            if (Uhr::now() > ende) { return false; }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return true;
    }
    // Nach dem letzten Zeichen: es kommt nichts mehr nach.
    void ausklingen()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(int(2.5 * kPunktMs)));
    }
};

// Die Dauern der Zeichen (unten -> oben) in Millisekunden.
QList<double> dauern(const std::vector<Kante>& k)
{
    QList<double> d;
    for (size_t i = 0; i + 1 < k.size(); i += 2) { d.append(ms(k[i].wann, k[i + 1].wann)); }
    return d;
}

// Was vom Takt des Laeufers nicht abhaengt: abwechselnd unten/oben,
// beginnend mit unten, und kein Zeichen kuerzer als geplant.
bool ordentlich(const std::vector<Kante>& k, const QList<int>& punkte, QString* warum)
{
    if (int(k.size()) != 2 * punkte.size()) {
        *warum = QStringLiteral("%1 Kanten statt %2").arg(k.size()).arg(2 * punkte.size());
        return false;
    }
    for (size_t i = 0; i < k.size(); ++i) {
        if (k[i].unten != (i % 2 == 0)) {
            *warum = QStringLiteral("Kante %1 hat die falsche Richtung").arg(i);
            return false;
        }
    }
    const QList<double> d = dauern(k);
    for (int i = 0; i < d.size(); ++i) {
        if (d[i] < punkte[i] * kPunktMs - kGenauMs) {
            *warum = QStringLiteral("Zeichen %1 ist mit %2 ms kuerzer als geplant")
                         .arg(i).arg(d[i]);
            return false;
        }
    }
    return true;
}

// Auf dem Raster: jede Dauer und jede Pause genau.
bool genau(const std::vector<Kante>& k, const QList<int>& punkte)
{
    const QList<double> d = dauern(k);
    for (int i = 0; i < d.size(); ++i) {
        if (std::abs(d[i] - punkte[i] * kPunktMs) > kGenauMs) { return false; }
    }
    for (size_t i = 1; i + 1 < k.size(); i += 2) {
        if (std::abs(ms(k[i].wann, k[i + 1].wann) - kPunktMs) > kGenauMs) { return false; }
    }
    return true;
}

} // namespace

class TstIambicKeyer : public QObject {
    Q_OBJECT

    // Ein Szenario bis zu dreimal fahren. Die Ordnung muss JEDES Mal
    // stimmen; die genaue Zeit einmal.
    static void fahre(const std::function<std::vector<Kante>()>& szenario,
                      const QList<int>& punkte)
    {
        QString letzte;
        for (int versuch = 1; versuch <= kVersuche; ++versuch) {
            const std::vector<Kante> k = szenario();
            QString warum;
            QVERIFY2(ordentlich(k, punkte, &warum), qPrintable(warum));
            if (genau(k, punkte)) { return; }
            QStringList text;
            for (double d : dauern(k)) { text << QString::number(d, 'f', 1); }
            letzte = text.join(QStringLiteral(" / "));
            qInfo().noquote() << "Versuch" << versuch << "nicht auf dem Raster:" << letzte << "ms";
        }
        QFAIL(qPrintable(QStringLiteral("in %1 Versuchen nie auf dem Raster, zuletzt %2 ms")
                             .arg(kVersuche).arg(letzte)));
    }

private slots:
    void einPunkt()
    {
        fahre([]() {
            Stand s(IambicKeyer::Mode::IambicB);
            s.geber.setPaddleState(true, false);
            if (s.warteAuf(1)) { s.geber.setPaddleState(false, false); }
            s.warteAuf(2);
            s.ausklingen();
            return s.lies();
        }, {1});
    }

    void einStrichIstDreiPunkte()
    {
        fahre([]() {
            Stand s(IambicKeyer::Mode::IambicB);
            s.geber.setPaddleState(false, true);
            if (s.warteAuf(1)) { s.geber.setPaddleState(false, false); }
            s.warteAuf(2);
            s.ausklingen();
            return s.lies();
        }, {3});
    }

    // Punkt halten, Strich dazu, beide im Strich loslassen. Art A hoert
    // mit dem Strich auf: Punkt, Strich.
    void zusammendrueckenArtA()
    {
        fahre([]() {
            Stand s(IambicKeyer::Mode::IambicA);
            s.geber.setPaddleState(true, false);
            if (s.warteAuf(1)) { s.geber.setPaddleState(true, true); }
            if (s.warteAuf(3)) { s.geber.setPaddleState(false, false); }   // der Strich laeuft
            s.warteAuf(4);
            s.ausklingen();
            return s.lies();
        }, {1, 3});
    }

    // Dasselbe in Art B: der beim Strich noch gehaltene Punkt ist
    // gemerkt und kommt nach dem Loslassen noch -- Punkt, Strich, Punkt.
    void zusammendrueckenArtB()
    {
        fahre([]() {
            Stand s(IambicKeyer::Mode::IambicB);
            s.geber.setPaddleState(true, false);
            if (s.warteAuf(1)) { s.geber.setPaddleState(true, true); }
            if (s.warteAuf(3)) { s.geber.setPaddleState(false, false); }
            s.warteAuf(6);
            s.ausklingen();
            return s.lies();
        }, {1, 3, 1});
    }

    void paddlesTauschen()
    {
        fahre([]() {
            Stand s(IambicKeyer::Mode::IambicB, /*tausch=*/true);
            s.geber.setPaddleState(true, false);   // der „Punkt"-Kontakt gibt jetzt den Strich
            if (s.warteAuf(1)) { s.geber.setPaddleState(false, false); }
            s.warteAuf(2);
            s.ausklingen();
            return s.lies();
        }, {3});
    }

    // Anhalten mitten im Zeichen: die Taste geht hoch, nicht erst am
    // Ende des Strichs (720 ms).
    void anhaltenNimmtDieTasteHoch()
    {
        Stand s(IambicKeyer::Mode::IambicB);
        s.geber.setPaddleState(false, true);
        QVERIFY(s.warteAuf(1));
        s.geber.stop();
        const auto k = s.lies();
        QCOMPARE(int(k.size()), 2);
        QVERIFY(!k[1].unten);
    }
};

QTEST_GUILESS_MAIN(TstIambicKeyer)
#include "tst_iambic_keyer.moc"
