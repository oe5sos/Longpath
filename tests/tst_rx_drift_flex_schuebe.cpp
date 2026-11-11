// no-port-check: Longpath test file; the counting rule it pins comes from
// Thetis rmatch.c xrmatchOUT/xrmatchIN [v2.10.3.15-5-g852bf0e], cited in
// PortAudioBus.cpp.
// =================================================================
// tests/tst_rx_drift_flex_schuebe.cpp  (Longpath)
// =================================================================
//
// Der RX-Driftausgleich an der FLEX-8400, am 2026-11-11 gemessen: in jedem
// Lauf zog er weg, −1700 bis −8400 ppm, und warf 0,8 % des Tons weg --
// obwohl die Flex 24 013 Rahmen/s liefert (nachgezaehlt), also nur rund
// −540 ppm Unterschied zum Geraet.
//
// Ursache: der Bus meldete als Verbrauch nur, was er WIRKLICH aus dem Ring
// geholt hatte. Kommt der Ton in Schueben (Flex-Ton laeuft ueber den
// Hauptfaden; im Protokoll ueber 300 Unterbrechungen, die laengste 650 ms),
// laeuft der 100-ms-Ring leer und danach ueber. Dann ist der gezaehlte
// Verbrauch nur noch das, was hineingeschoben wurde -- die Vorsteuerung
// misst ihr EIGENES Verhaeltnis, nicht den Takt des Geraets, und der beim
// Schub hoch gemessene Fuellstand schiebt sie Schritt fuer Schritt nach
// unten.
//
// Thetis zaehlt anders: xrmatchOUT meldet jeden Ruf mit seiner vollen
// Blocklaenge, `control (a, -(a->outsize))`, auch beim Leerlauf
// (rmatch.c:464); xrmatchIN jeden Eingang mit `a->insize`, auch beim
// Ueberlauf (rmatch.c:359). Der Verbrauch ist damit der Geraetetakt.
//
// Gegen die alte Zaehlregel faellt Fall 1 um; Fall 2 ist die Gegenprobe,
// die mit der alten Regel weglaufen MUSS.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include "core/audio/PortAudioBus.h"
#include "core/audio/RxRatenAngleich.h"

#include <QtTest>

#include <cmath>
#include <cstdint>
#include <vector>

using Longpath::PortAudioBus;
using Longpath::RxRatenAngleich;

namespace {

constexpr int    kRate        = 48000;
constexpr int    kRing        = RxRatenAngleich::ringRahmenFuer(kRate);   // 4800
constexpr int    kGeraetBlock = 512;          // ein Rueckruf des Geraets
constexpr int    kQuellBlock  = 256;          // ein Flex-Paket nach 24->48 kHz
constexpr double kQuelleHz    = 48026.0;      // 2 x 24 013, an der Station gezaehlt

// Wie der Ton an der Station stockt, nach den Protokollen vom 2026-11-11:
// FlexVitaStream zaehlt eine „Unterbrechung" ab 85 ms Luecke
// (kUnterbrechungBodenNs); es waren gut 300 in fuenf Minuten, also etwa
// eine je Sekunde, die laengste 650 ms. Hier: alle 0,9 s eine Pause von 90
// bis 140 ms, jede zehnte 273 ms, und nach einer Minute einmal 650 ms.
// Waehrend der Pause sammelt sich der Ton im Netz an und kommt danach auf
// einmal.
constexpr double kPausen[] = {0.090, 0.110, 0.095, 0.140, 0.100,
                              0.120, 0.090, 0.105, 0.130, 0.273};
constexpr int    kPausenAnzahl = 10;

enum class Regel { Bus, RingAlt };

// Was ein Rueckruf meldet. `Bus` ist die Regel, die PortAudioBus selbst
// benutzt; `RingAlt` die bis 2026-11-11: nur, was aus dem Ring kam.
std::int64_t verbrauchDiesesRufs(Regel regel, std::int64_t gewollt, std::int64_t ausDemRing)
{
    if (regel == Regel::RingAlt) { return ausDemRing; }
    return PortAudioBus::verbrauchJeRueckruf(gewollt, ausDemRing);
}

struct Ergebnis {
    double ppm{0.0};            // nach 10 Minuten
    double ppmSpaeter{0.0};     // nach 20 Minuten
    std::int64_t leerlaeufe{0};
    std::int64_t ueberlaeufe{0};
};

// Ein Lauf ueber 20 Minuten, abgelesen nach 10 und nach 20.
Ergebnis fahre(Regel regel)
{
    constexpr double sekunden = 1200.0;
    bool zehnAbgelesen = false;
    RxRatenAngleich a(kRate, 2);
    std::vector<float> block(static_cast<size_t>(kQuellBlock) * 2, 0.0f);
    for (int i = 0; i < kQuellBlock; ++i) {
        const float s = 0.25f * std::sin(2.0f * float(M_PI) * 700.0f * i / kRate);
        block[static_cast<size_t>(i) * 2] = s;
        block[static_cast<size_t>(i) * 2 + 1] = s;
    }

    std::int64_t fuellung = kRing / 2;
    std::int64_t verbraucht = 0;          // was der Bus meldet
    std::int64_t uebersprungen = 0;       // beim Ueberlauf verworfen, noch nicht gelesen
    Ergebnis e;

    const double dt = double(kGeraetBlock) / kRate;   // Abstand der Rueckrufe
    double erzeugtSoll = 0.0;     // was die Quelle bis jetzt geliefert HAT
    double ausgeliefert = 0.0;    // was davon schon bei uns angekommen ist
    double naechstePause = 0.9;
    double pauseBis = -1.0;
    int    pauseIndex = 0;

    for (double t = 0.0; t < sekunden; t += dt) {
        // Quelle: gleichmaessig im Netz, bei uns nur ausserhalb der Pausen.
        erzeugtSoll += kQuelleHz * dt;
        if (t >= naechstePause) {
            const bool dieLange = (pauseIndex == 66);   // nach ~60 s
            pauseBis = t + (dieLange ? 0.650 : kPausen[pauseIndex % kPausenAnzahl]);
            ++pauseIndex;
            naechstePause = t + 0.9;
        }
        if (t >= pauseBis) {
            while (ausgeliefert + kQuellBlock <= erzeugtSoll) {
                ausgeliefert += kQuellBlock;
                const auto aus = a.verarbeite(block.data(), kQuellBlock,
                                              fuellung, kRing, verbraucht);
                fuellung += aus.anzahl;
                if (fuellung > kRing) {
                    uebersprungen += fuellung - kRing;
                    fuellung = kRing;
                    ++e.ueberlaeufe;
                }
            }
        }

        // Geraet: holt seinen Block im eigenen Takt, ob etwas da ist oder nicht.
        const std::int64_t aus = std::min<std::int64_t>(fuellung, kGeraetBlock);
        if (aus < kGeraetBlock) { ++e.leerlaeufe; }
        fuellung -= aus;
        // Der alte Rueckruf zaehlte den Sprung ueber verworfene Rahmen mit
        // (r - rAnfang), wie PortAudioBus::paCallback.
        verbraucht += verbrauchDiesesRufs(regel, kGeraetBlock, aus + uebersprungen);
        uebersprungen = 0;

        if (!zehnAbgelesen && t >= 600.0) {
            e.ppm = (a.verhaeltnis() - 1.0) * 1e6;
            zehnAbgelesen = true;
        }
    }
    e.ppmSpaeter = (a.verhaeltnis() - 1.0) * 1e6;
    return e;
}

}  // namespace

class TstRxDriftFlexSchuebe : public QObject {
    Q_OBJECT

private slots:
    // Fall 1: mit der Zaehlregel des Busses steht der Regler, auch wenn
    // der Ton in Schueben kommt. Gemessen am 2026-11-11: -1255 ppm nach
    // 10 min, -1249 ppm nach 30 min (dieser Pruefstand liest nach 10 und 20). Der Abstand zum wahren Unterschied
    // (-541 ppm) ist der Proportionalteil: Longpath fragt den Fuellstand
    // nur beim Hineinschieben ab, also mitten im Schub, Thetis auch bei
    // jedem Geraeterueckruf (control() in xrmatchOUT, rmatch.c:464).
    // Der Regler laeuft damit nicht weg, er steht etwas daneben.
    void geraetetaktHaeltSchuebeAus()
    {
        const double soll = (kRate / kQuelleHz - 1.0) * 1e6;   // -541 ppm
        const Ergebnis e = fahre(Regel::Bus);
        QVERIFY2(e.leerlaeufe > 100,
                 qPrintable(QStringLiteral("nur %1 Leerlaeufe — die Schuebe kamen nicht an")
                                .arg(e.leerlaeufe)));
        for (double ppm : {e.ppm, e.ppmSpaeter}) {
            QVERIFY2(std::abs(ppm - soll) < 1000.0,
                     qPrintable(QStringLiteral("Verhaeltnis %1 ppm, erwartet um %2 ppm "
                                               "(%3 Leerlaeufe, %4 Ueberlaeufe)")
                                    .arg(ppm, 0, 'f', 1).arg(soll, 0, 'f', 1)
                                    .arg(e.leerlaeufe).arg(e.ueberlaeufe)));
        }
        // Und es steht: zwischen 10 und 20 Minuten bewegt es sich nicht.
        QVERIFY2(std::abs(e.ppmSpaeter - e.ppm) < 200.0,
                 qPrintable(QStringLiteral("laeuft noch: %1 ppm nach 10 min, %2 ppm nach 20 min")
                                .arg(e.ppm, 0, 'f', 1).arg(e.ppmSpaeter, 0, 'f', 1)));
    }

    // Fall 2, Gegenprobe: mit der alten Regel (nur, was aus dem Ring kam)
    // laeuft derselbe Lauf nach unten weg, wie an der Station. Gemessen:
    // -3850 ppm nach 10 min, -5443 ppm nach 30 min.
    void ringzaehlungLaeuftWeg()
    {
        const double soll = (kRate / kQuelleHz - 1.0) * 1e6;
        const Ergebnis e = fahre(Regel::RingAlt);
        QVERIFY2(e.ppm < soll - 1500.0,
                 qPrintable(QStringLiteral("erwartet: laeuft nach unten weg; gemessen %1 ppm")
                                .arg(e.ppm, 0, 'f', 1)));
        QVERIFY2(e.ppmSpaeter < e.ppm - 300.0,
                 qPrintable(QStringLiteral("erwartet: laeuft weiter; %1 ppm nach 10 min, "
                                           "%2 ppm nach 20 min")
                                .arg(e.ppm, 0, 'f', 1).arg(e.ppmSpaeter, 0, 'f', 1)));
    }

    // Fall 3: eine lange Pause der QUELLE ist kein Takt. Der Bus zaehlt
    // den Geraetetakt jetzt auch in der Stille weiter; nach 5 s ohne Ton
    // (getrennt, Geraet aus) faengt der Regler neu an, statt die Pause als
    // Abweichung zu lesen.
    void eineLangePauseDerQuelleIstKeinTakt()
    {
        RxRatenAngleich a(kRate, 2);
        const std::vector<float> block(static_cast<size_t>(kQuellBlock) * 2, 0.1f);
        std::int64_t verbraucht = 0;
        const std::int64_t ziel = kRing / 2;
        // 20 s bei genau passendem Takt.
        for (int i = 0; i < 20 * kRate / kQuellBlock; ++i) {
            verbraucht += kQuellBlock;
            a.verarbeite(block.data(), kQuellBlock, ziel, kRing, verbraucht);
        }
        QVERIFY(a.regeltSchon());
        // 5 s Pause: das Geraet laeuft weiter, die Quelle schweigt.
        verbraucht += 5LL * kRate;
        // Weiter bei passendem Takt.
        for (int i = 0; i < 20 * kRate / kQuellBlock; ++i) {
            verbraucht += kQuellBlock;
            a.verarbeite(block.data(), kQuellBlock, ziel, kRing, verbraucht);
        }
        const double ppm = (a.verhaeltnis() - 1.0) * 1e6;
        QVERIFY2(std::abs(ppm) < 300.0,
                 qPrintable(QStringLiteral("nach der Pause %1 ppm").arg(ppm, 0, 'f', 1)));
    }
};

QTEST_MAIN(TstRxDriftFlexSchuebe)
#include "tst_rx_drift_flex_schuebe.moc"
