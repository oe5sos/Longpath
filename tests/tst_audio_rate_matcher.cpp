// Das Regelgesetz gegen die Uhrendrift — und die Gegenprobe, dass es
// ueberhaupt gebraucht wird.
//
// Gemessen am 2026-09-27 (SunSDR2 QRP + MacBook Air): der Fuellstand des
// Ausgangsrings steigt um rund 0,23 ms je Minute, also etwa 4 ppm. Zwei
// Quarze, die nie genau uebereinstimmen. Mit dem 100-ms-Ring laeuft er
// nach etwa fuenf Stunden ueber; laeuft das Geraet andersherum, laeuft er
// nach derselben Zeit leer. Beides knackt.
//
// Der Stand faehrt eine virtuelle Uhr: Erzeuger bei 48000*(1+ppm),
// Verbraucher bei 48000, acht Stunden simuliert, ohne Tongeraet. Das ist
// die einzige Art, acht Stunden Drift in Millisekunden zu pruefen.
//
// DIE GEGENPROBE IST DER KERN: dieselbe Simulation einmal OHNE Regelung
// (Verhaeltnis fest auf 1,0). Dort MUSS der Ring ueberlaufen -- sonst
// belegt das gruene Ergebnis mit Regelung gar nichts, sondern nur, dass
// die Simulation zu gutmuetig ist.
//
// Was hier NICHT geprueft wird, weil es hier nicht steht: der Resampler
// und die Verdrahtung in den Tonweg. Die kommen getrennt zur Durchsicht
// (docs/architecture/2026-09-27-rx-audio-rate-match-design.md, §4).

#include "core/audio/AudioRateMatcher.h"

#include <QtTest>

#include <cmath>

using Longpath::AudioRateMatcher;

namespace {

constexpr int kRate      = 48000;
constexpr int kRing      = 4800;   // 100 ms
constexpr int kZiel      = kRing / 2;
constexpr int kBlock     = 480;    // 10 ms

struct Ergebnis {
    double hubMs{0};
    double endeFuellMs{0};
    long   ueberlauf{0};
    long   leerlauf{0};
    double varEnde{1.0};
};

/// Faehrt die Drift auf einer virtuellen Uhr.
///
/// Statt zweier Uhren mit Zeitstempeln das gleichwertige, einfachere Bild:
/// der Erzeuger legt je Runde `block * (1 + ppm)` Rahmen hin (Bruchteile
/// werden mitgeschleppt, sonst ueberdeckte das Runden die 4 ppm um
/// Groessenordnungen), der Verbraucher nimmt genau `block`.
Ergebnis fahre(double ppm, long bloecke, double verstaerkung,
               int block = kBlock, int ring = kRing)
{
    AudioRateMatcher::Einstellungen e;
    e.nennRateEin = kRate;
    e.nennRateAus = kRate;
    e.propVerstaerkung = verstaerkung;
    AudioRateMatcher m(e);

    const double erzeuger = 1.0 + ppm * 1e-6;
    double fuellung = ring / 2.0;
    double schuld = 0.0;
    double minF = 1e9, maxF = -1e9;
    Ergebnis r;

    for (long i = 0; i < bloecke; ++i) {
        schuld += block * erzeuger;
        const int ein = static_cast<int>(schuld);
        schuld -= ein;

        m.melde(ein, static_cast<std::int64_t>(fuellung + 0.5), ring);
        fuellung += ein * m.verhaeltnis();
        if (fuellung > ring) { fuellung = ring; r.ueberlauf++; }

        if (fuellung < block) { fuellung = 0; r.leerlauf++; }
        else                  { fuellung -= block; }
        m.melde(-block, static_cast<std::int64_t>(fuellung + 0.5), ring);

        // Erst nach dem Einschwingen messen: das erste Zehntel gehoert dem
        // Anlauf, und ein Anlauf ist keine Drift.
        if (m.regeltSchon() && i > bloecke / 10) {
            minF = std::min(minF, fuellung);
            maxF = std::max(maxF, fuellung);
        }
    }
    r.hubMs = (maxF - minF) / kRate * 1000.0;
    r.endeFuellMs = fuellung / kRate * 1000.0;
    r.varEnde = m.verhaeltnis();
    return r;
}

}  // namespace

class TstAudioRateMatcher : public QObject
{
    Q_OBJECT

private slots:
    void vorDemAnlaufWirdNichtGeregelt();
    void dieGrenzenHalten();
    void dieThetisVerstaerkungDivergiertHier();     // Gegenprobe 1
    void ohneRegelungLaeuftDerRingUeber();          // Gegenprobe 2
    void driftWirdAusgeglichen();
    void dasVerhaeltnisTrifftDenSollwert();
    void grobeBloeckeSindEineGroessenfrage();
};

void TstAudioRateMatcher::vorDemAnlaufWirdNichtGeregelt()
{
    // Thetis setzt `control_flag` erst nach drei Sekunden. Ein Regler, der
    // auf die ersten Bloecke nach dem Start anspringt, regelt auf den
    // Anlauf und nicht auf die Drift.
    AudioRateMatcher::Einstellungen e;
    e.nennRateEin = kRate; e.nennRateAus = kRate; e.anlaufSekunden = 3.0;
    AudioRateMatcher m(e);

    // Zwei Sekunden lang ein voellig schiefer Fuellstand -- es darf
    // trotzdem nichts geregelt werden.
    for (int i = 0; i < 2 * kRate / kBlock; ++i) {
        m.melde(kBlock, 0, kRing);
        m.melde(-kBlock, 0, kRing);
    }
    QVERIFY(!m.regeltSchon());
    QCOMPARE(m.verhaeltnis(), 1.0);

    for (int i = 0; i < 2 * kRate / kBlock; ++i) { m.melde(kBlock, kRing / 2, kRing); }
    QVERIFY(m.regeltSchon());
}

void TstAudioRateMatcher::dieGrenzenHalten()
{
    // rmatch.c:269-270. Ein Verhaeltnis ausserhalb 0,96...1,04 waere kein
    // Driftausgleich mehr, sondern eine hoerbare Tonhoehenaenderung.
    AudioRateMatcher::Einstellungen e;
    e.nennRateEin = kRate; e.nennRateAus = kRate; e.anlaufSekunden = 0.0;
    AudioRateMatcher m(e);
    for (int i = 0; i < 200000; ++i) { m.melde(kBlock, 0, kRing); }
    QVERIFY2(m.verhaeltnis() <= 1.04 + 1e-12,
             qPrintable(QString::number(m.verhaeltnis(), 'f', 9)));
    QVERIFY(m.verhaeltnis() >= 0.96);

    AudioRateMatcher m2(e);
    for (int i = 0; i < 200000; ++i) { m2.melde(kBlock, kRing, kRing); }
    QVERIFY2(m2.verhaeltnis() >= 0.96 - 1e-12,
             qPrintable(QString::number(m2.verhaeltnis(), 'f', 9)));
    QVERIFY(m2.verhaeltnis() <= 1.04);
}

void TstAudioRateMatcher::dieThetisVerstaerkungDivergiertHier()
{
    // DER BEFUND DIESES STANDES. Thetis' 4,0e-6 (rmatch.c:521) schwingt bei
    // Longpaths Aufrufrate ueber den GANZEN Ring -- und zwar auch bei
    // 0 ppm, wo es nichts zu regeln gibt. Haette der Entwurf die Zahl
    // ungeprueft uebernommen, waere der Ausgleich schlimmer gewesen als die
    // Drift, gegen die er gebaut wurde.
    const Ergebnis r = fahre(0.0, 1000000, 4.0e-6);
    qInfo("Thetis-Verstaerkung 4,0e-6 bei 0 ppm: %.1f ms Hub, Ueber %ld, Leer %ld",
          r.hubMs, r.ueberlauf, r.leerlauf);
    QVERIFY2(r.ueberlauf > 0,
             "Mit 4,0e-6 MUSS der Ring ueberlaufen. Laeuft er nicht ueber, "
             "ist die Simulation zu gutmuetig und die Vorgabe 4,0e-7 "
             "nicht mehr begruendet.");
}

void TstAudioRateMatcher::ohneRegelungLaeuftDerRingUeber()
{
    // Zweite Gegenprobe: ganz ohne Regelung. Ohne sie belegt jedes gruene
    // Ergebnis unten nichts.
    AudioRateMatcher::Einstellungen e;
    e.nennRateEin = kRate; e.nennRateAus = kRate;
    AudioRateMatcher m(e);
    const double erzeuger = 1.0 + 4.0 * 1e-6;
    double fuellung = kRing / 2.0, schuld = 0.0;
    long ueber = 0;
    // 1,5 Mio Bloecke sind 4,2 Stunden. Gemessen laeuft der Ring nach 3,5 h
    // ueber -- mit 1 Mio Bloecken (2,8 h) waere die Gegenprobe gruen, ohne
    // dass sie etwas belegt. Genau dieser Fehler war hier drin.
    for (long i = 0; i < 1500000; ++i) {
        schuld += kBlock * erzeuger;
        const int ein = static_cast<int>(schuld); schuld -= ein;
        fuellung += ein;                       // Verhaeltnis fest auf 1,0
        if (fuellung > kRing) { fuellung = kRing; ueber++; }
        fuellung = std::max(0.0, fuellung - kBlock);
    }
    qInfo("ohne Regelung, +4 ppm: Ueberlaeufe %ld", ueber);
    QVERIFY2(ueber > 0, "Ohne Regelung muesste der Ring ueberlaufen.");
}

void TstAudioRateMatcher::driftWirdAusgeglichen()
{
    // Die gemessene Drift (4 ppm) und das Fuenfundzwanzigfache davon, wie es
    // billige USB-Tonkarten liefern. Je Lauf rund 2,8 Stunden.
    for (double ppm : {0.0, 4.0, -4.0, 100.0, -100.0}) {
        const Ergebnis r = fahre(ppm, 1000000, 4.0e-7);
        qInfo("%+7.1f ppm: %.2f ms Hub, Ende %.2f ms (Ziel 50), var %.9f, "
              "Ueber %ld Leer %ld",
              ppm, r.hubMs, r.endeFuellMs, r.varEnde, r.ueberlauf, r.leerlauf);
        QCOMPARE(r.ueberlauf, 0L);
        QCOMPARE(r.leerlauf, 0L);
        QVERIFY2(r.hubMs < 2.0, qPrintable(QString::number(r.hubMs, 'f', 2)));
        QVERIFY2(std::abs(r.endeFuellMs - 50.0) < 2.0,
                 qPrintable(QString::number(r.endeFuellMs, 'f', 2)));
    }
}

void TstAudioRateMatcher::dasVerhaeltnisTrifftDenSollwert()
{
    // Nicht nur "stabil", sondern RICHTIG: bei +4 ppm Drift muss das
    // Verhaeltnis gegen 1/(1+4e-6) laufen. Eine Regelung, die den Ring
    // haelt, aber die falsche Rate faehrt, wuerde beides tun -- halten und
    // langsam die Tonhoehe verschieben.
    for (double ppm : {4.0, -4.0, 100.0, -100.0}) {
        const Ergebnis r = fahre(ppm, 1000000, 4.0e-7);
        const double soll = 1.0 / (1.0 + ppm * 1e-6);
        QVERIFY2(std::abs(r.varEnde - soll) < 1e-7,
                 qPrintable(QStringLiteral("%1 ppm: %2 statt %3")
                     .arg(ppm).arg(r.varEnde, 0, 'f', 9).arg(soll, 0, 'f', 9)));
    }
}

void TstAudioRateMatcher::grobeBloeckeSindEineGroessenfrage()
{
    // Zweiter gemessener Befund: ueber die Stabilitaet entscheidet das
    // PRODUKT aus Verstaerkung und Blockgroesse, nicht die Verstaerkung
    // allein. Gemessen (2026-10-05, 4 ppm, Ring 100 ms):
    //
    //     4,0e-6 x  480 = 1,9e-3   -> divergiert (Thetis' Zahl)
    //     4,0e-7 x 2048 = 8,2e-4   -> divergiert
    //     4,0e-7 x 1024 = 4,1e-4   -> stabil, 3,6 ms Hub
    //     1,0e-7 x 2048 = 2,0e-4   -> stabil, 4,9 ms Hub
    //     4,0e-7 x  480 = 1,9e-4   -> stabil, 0,3 ms Hub
    //
    // Die Vorgabe 4,0e-7 gilt also fuer Bloecke bis rund 1024 Rahmen. Wer
    // die Verdrahtung mit groesseren Bloecken baut, muss die Verstaerkung
    // mitfuehren -- und erfaehrt es hier, statt es am Geraet zu hoeren.
    //
    // Ein groesserer Ring hilft NICHT: mit Block 2048 und 4,0e-7 wird es im
    // 16384er Ring sogar schlimmer (298 ms Hub). Das stand hier zuerst als
    // Abhilfe und war behauptet, nicht gemessen.
    const Ergebnis fein = fahre(4.0, 300000, 4.0e-7, 1024, kRing);
    qInfo("Block 1024, 4,0e-7 (Produkt 4,1e-4): %.1f ms Hub, Ueber %ld",
          fein.hubMs, fein.ueberlauf);
    QCOMPARE(fein.ueberlauf, 0L);

    const Ergebnis grob = fahre(4.0, 300000, 4.0e-7, 2048, kRing);
    qInfo("Block 2048, 4,0e-7 (Produkt 8,2e-4): %.1f ms Hub, Ueber %ld",
          grob.hubMs, grob.ueberlauf);
    QVERIFY2(grob.ueberlauf > 0,
             "Produkt 8,2e-4 MUSS ueberlaufen -- sonst stimmt die Grenze im "
             "Kopf der Klasse nicht mehr.");

    // Und die Abhilfe, diesmal gemessen: die Verstaerkung mitfuehren.
    const Ergebnis gezogen = fahre(4.0, 300000, 1.0e-7, 2048, kRing);
    qInfo("Block 2048, 1,0e-7 (Produkt 2,0e-4): %.1f ms Hub, Ueber %ld",
          gezogen.hubMs, gezogen.ueberlauf);
    QCOMPARE(gezogen.ueberlauf, 0L);
    QCOMPARE(gezogen.leerlauf, 0L);
}

QTEST_MAIN(TstAudioRateMatcher)
#include "tst_audio_rate_matcher.moc"
