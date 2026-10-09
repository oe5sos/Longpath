// Der Tonsquelch, gemessen statt behauptet.
//
// SliceModel::fmCtcssMode kennt seit jeher "Decode" (2) und
// "Encode+Decode" (3), aber empfangsseitig geschah bisher nichts: in
// Thetis gibt es dafuer auch nichts, dort filtert `wdsp/fmd.c` den
// Subton nur aus dem Hoerbaren heraus (`snotch`), ohne ihn zu erkennen.
// `CtcssDetector` schliesst die Luecke.
//
// Drei Eingriffe am Detektor sind hier belegt, und zwar so, dass die
// Fassung OHNE sie an einem Prueffall faellt:
//   1. Der Tiefpass vor der Dezimation. Ohne ihn klappte ein Stoerton
//      bei 900 Hz auf 100 Hz zurueck und oeffnete die auf 100 Hz
//      gestellte Sperre mit einem Verhaeltnis von 0,996
//      (`derTiefpassHaeltAliasingDraussen`).
//   2. Der Nachbarvergleich. Die engsten Normtoene liegen 2,3 Hz
//      auseinander (67,0 und 69,3); das 250-ms-Fenster loest 4 Hz auf.
//      Ueber das Verhaeltnis allein ging es sich gerade noch aus -- der
//      Durchgriff lag bei 0,29 unter der Schwelle von 0,30. Diese 3,5 %
//      Reserve verschwinden aber, sobald der Sender innerhalb der von
//      der Norm erlaubten +-1 % danebenliegt: ein Sender, der 69,3
//      senden will und bei 69,0 landet, riss die Sperre ohne den
//      Nachbarvergleich auf (`jederNormtonWeistSeineNachbarnAb`).
//   3. Die Schliess-Schwelle bei 0,10 statt 0,18. Mit 0,18 schlug die
//      Sperre an der Grenze viermal in 40 Bloecken um
//      (`sieFlattertNichtAnDerSchwelle`).
//
// Alle Signale sind synthetisch -- kein Funkgeraet noetig. Die Sprache
// ist als Glottisimpulsfolge mit Jitter modelliert, nicht als
// frequenzmodulierter Ton. Dieser Unterschied ist nicht kosmetisch: ein
// Ton, der ueber den Soll-Subton hinwegstreicht, erzeugt dort echte
// Dauertonleistung und misst damit nicht Sprache, sondern einen
// Suchlauf. Die erste Fassung dieses Pruefstands machte genau diesen
// Fehler und behauptete darauf zwei Detektorfehler, die es nicht gab --
// ein getraegter Bezugswert und eine Haltezeit waren schon eingebaut,
// bevor sich das Modell als falsch erwies. Mit einer Stimme, die wie
// eine Stimme gebaut ist, aenderten beide an keinem Prueffall etwas und
// sind wieder heraus.

#include <QtTest>

#include <cmath>
#include <random>
#include <vector>

#include "core/CtcssDetector.h"

using namespace Longpath;

namespace {

constexpr double kRate = 48000.0;

/// Ein Block Sinus mit `hz`, Amplitude `amp`, fortlaufende Phase.
std::vector<float> ton(double hz, double seconds, double amp = 0.5, double* phase = nullptr)
{
    const int n = static_cast<int>(kRate * seconds);
    std::vector<float> out(static_cast<size_t>(n));
    double ph = phase ? *phase : 0.0;
    const double step = 2.0 * M_PI * hz / kRate;
    for (int i = 0; i < n; ++i) {
        out[static_cast<size_t>(i)] = static_cast<float>(amp * std::sin(ph));
        ph += step;
    }
    if (phase) { *phase = ph; }
    return out;
}

/// Rauschen auf einen Block legen.
void rauschenDazu(std::vector<float>& buf, double amp, unsigned seed = 1)
{
    std::mt19937 gen(seed);
    std::normal_distribution<double> d(0.0, amp);
    for (float& s : buf) { s = static_cast<float>(s + d(gen)); }
}

/// Eine Stimme, wie eine Stimme wirklich aussieht: Glottisimpulse im
/// Abstand 1/f0, von Periode zu Periode um `jitterProzent` schwankend,
/// durch zwei Formantresonatoren geschickt, mit Silbenhuellkurve.
///
/// Wichtig fuer diesen Pruefstand: die Energie einer solchen Anregung
/// verteilt sich auf ALLE Harmonischen, ein Subton ist dagegen eine
/// einzelne Linie. Genau dieser Unterschied ist es, der den Detektor
/// tragen laesst -- auch bei einem Sprecher, dessen Grundton genau auf
/// dem Subton sitzt.
struct Stimme {
    double rate;
    double f0;
    double jitterProzent;
    std::mt19937 gen;
    double bisNaechsterImpuls{0.0};
    double r1z1{0.0}, r1z2{0.0}, r2z1{0.0}, r2z2{0.0};
    double f1{500.0}, f2{1500.0};

    Stimme(double r, double grund, double jit = 1.0, unsigned seed = 42)
        : rate(r), f0(grund), jitterProzent(jit), gen(seed) {}

    static double resonator(double x, double& z1, double& z2, double f, double q, double rate)
    {
        const double w  = 2.0 * M_PI * f / rate;
        const double r  = std::exp(-w / (2.0 * q));
        const double a1 = -2.0 * r * std::cos(w);
        const double a2 = r * r;
        const double y  = x - a1 * z1 - a2 * z2;
        z2 = z1;
        z1 = y;
        return y * (1.0 - r);
    }

    std::vector<float> block(double seconds, double amp, double huellPhase = 0.0)
    {
        const int n = static_cast<int>(rate * seconds);
        std::vector<float> out(static_cast<size_t>(n));
        std::normal_distribution<double> jit(0.0, jitterProzent / 100.0);
        for (int i = 0; i < n; ++i) {
            double anregung = 0.0;
            if (bisNaechsterImpuls <= 0.0) {
                anregung = 1.0;
                bisNaechsterImpuls = (rate / f0) * (1.0 + jit(gen));
            }
            bisNaechsterImpuls -= 1.0;
            double y = resonator(anregung, r1z1, r1z2, f1, 8.0, rate);
            y += 0.5 * resonator(anregung, r2z1, r2z2, f2, 10.0, rate);
            const double t = (huellPhase + i) / rate;
            const double huell = 0.35 + 0.65 * std::fabs(std::sin(2.0 * M_PI * 0.9 * t));
            out[static_cast<size_t>(i)] = static_cast<float>(amp * y * huell);
        }
        return out;
    }
};

/// Wieviel Prozent der Fenster die Sperre offen war, waehrend `sekunden`
/// Stimme mit `f0` anlag -- mit oder ohne Subton darunter.
int anteilOffenProzent(double f0, double jitterProzent, double subtonAmp, double stimmAmp,
                       double sekunden = 4.0, double sollTonHz = 100.0)
{
    CtcssDetector det(kRate, sollTonHz);
    Stimme stimme(kRate, f0, jitterProzent, 42);
    double phase = 0.0;
    double huellPhase = 0.0;
    int offen = 0;
    int gezaehlt = 0;
    const int bloecke = static_cast<int>(sekunden / 0.1);
    for (int k = 0; k < bloecke; ++k) {
        std::vector<float> buf = stimme.block(0.1, stimmAmp, huellPhase);
        huellPhase += kRate * 0.1;
        if (subtonAmp > 0.0) {
            const std::vector<float> sub = ton(sollTonHz, 0.1, subtonAmp, &phase);
            for (size_t i = 0; i < buf.size(); ++i) { buf[i] += sub[i]; }
        }
        rauschenDazu(buf, 0.03, static_cast<unsigned>(k + 1));
        det.process(buf.data(), static_cast<int>(buf.size()));
        if (k >= 3) {                    // die ersten Fenster anlaufen lassen
            if (det.detected()) { ++offen; }
            ++gezaehlt;
        }
    }
    return gezaehlt > 0 ? (100 * offen) / gezaehlt : 0;
}

} // namespace

class TstCtcssDetector : public QObject
{
    Q_OBJECT

private slots:
    void dieTonlisteEntsprichtDerNorm();
    void jederNormtonWirdAufSichSelbstErkannt();
    void jederNormtonWeistSeineNachbarnAb();
    void derTonWirdInUnterEinerHalbenSekundeErkannt();
    void derPegelGehtNichtEin();
    void einVerstimmterSenderWirdNochErkannt();
    void derTiefpassHaeltAliasingDraussen();
    void spracheAlleinOeffnetNie();
    void derSubtonUnterSpracheHaeltOffen();
    void derDurchgangHaeltDreissigSekunden();
    void nachDemTonendeSchliesstSieWieder();
    void sieFlattertNichtAnDerSchwelle();
};

void TstCtcssDetector::dieTonlisteEntsprichtDerNorm()
{
    const QVector<double>& toene = ctcssStandardTonesHz();
    QCOMPARE(toene.size(), 49);
    QCOMPARE(toene.first(), 67.0);
    QCOMPARE(toene.last(), 254.1);
    for (int i = 1; i < toene.size(); ++i) {
        QVERIFY2(toene.at(i) > toene.at(i - 1), "Die Tonliste ist nicht aufsteigend");
    }
    QCOMPARE(nearestCtcssToneHz(100.0), 100.0);
    QCOMPARE(nearestCtcssToneHz(100.4), 100.0);
    // Mehr als 1 Hz daneben ist kein Normton.
    QCOMPARE(nearestCtcssToneHz(105.0), 0.0);
}

void TstCtcssDetector::jederNormtonWirdAufSichSelbstErkannt()
{
    // Alle 49, keiner darf durchfallen -- auch nicht 67,0 und 69,3, die
    // nur 2,3 Hz auseinanderliegen.
    for (double t : ctcssStandardTonesHz()) {
        CtcssDetector det(kRate, t);
        const std::vector<float> buf = ton(t, 0.6);
        det.process(buf.data(), static_cast<int>(buf.size()));
        QVERIFY2(det.detected(),
                 qPrintable(QStringLiteral("Normton %1 Hz nicht erkannt (Verhaeltnis %2)")
                                .arg(t).arg(det.lastRatio())));
    }
}

void TstCtcssDetector::jederNormtonWeistSeineNachbarnAb()
{
    // Der Grund, warum es den Nachbarvergleich gibt: das 250-ms-Fenster
    // loest 4 Hz auf, 67,0 und 69,3 liegen aber nur 2,3 Hz auseinander.
    // Ein Sender auf 69,3 erzeugt im 67,0-Zweig noch ein Verhaeltnis von
    // 0,29 -- praktisch dasselbe wie ein echter Subton unter Sprache.
    // Ueber das Verhaeltnis allein ist das nicht zu trennen.
    const QVector<double>& T = ctcssStandardTonesHz();
    int geprueft = 0;
    for (int i = 0; i < T.size(); ++i) {
        for (int j : {i - 1, i + 1}) {
            if (j < 0 || j >= T.size()) { continue; }
            CtcssDetector det(kRate, T.at(i));
            const std::vector<float> buf = ton(T.at(j), 0.6);
            det.process(buf.data(), static_cast<int>(buf.size()));
            QVERIFY2(!det.detected(),
                     qPrintable(QStringLiteral("Soll %1 Hz oeffnete bei Nachbar %2 Hz "
                                               "(Verhaeltnis %3, Nachbarabstand %4)")
                                    .arg(T.at(i)).arg(T.at(j))
                                    .arg(det.lastRatio()).arg(det.lastNeighbourMargin())));
            ++geprueft;
        }
    }
    QCOMPARE(geprueft, 96);

    // Und der Fall, an dem die Fassung ohne Nachbarvergleich wirklich
    // fiel: der Nachbarsender liegt innerhalb der von der Norm erlaubten
    // +-1 % daneben und rueckt dem Soll-Zweig damit noch naeher. Ohne
    // den Vergleich oeffnete die Sperre hier in vier von fuenf Faellen.
    for (double ist : {69.3, 69.0, 68.6, 68.3, 68.0}) {
        CtcssDetector det(kRate, 67.0);
        const std::vector<float> buf = ton(ist, 0.8);
        det.process(buf.data(), static_cast<int>(buf.size()));
        QVERIFY2(!det.detected(),
                 qPrintable(QStringLiteral("Soll 67,0 Hz oeffnete bei einem Sender auf %1 Hz "
                                           "(Verhaeltnis %2, Nachbarabstand %3)")
                                .arg(ist).arg(det.lastRatio()).arg(det.lastNeighbourMargin())));
    }
}

void TstCtcssDetector::derTonWirdInUnterEinerHalbenSekundeErkannt()
{
    // TIA-603 verlangt die Erkennung in unter 500 ms. Blockweise
    // fuettern, wie der Audioweg es tut, und zaehlen.
    CtcssDetector det(kRate, 123.0);
    double phase = 0.0;
    double verstrichen = 0.0;
    constexpr double kBlock = 0.02;              // 20 ms, wie ein Audioblock
    while (verstrichen < 1.0 && !det.detected()) {
        const std::vector<float> buf = ton(123.0, kBlock, 0.4, &phase);
        det.process(buf.data(), static_cast<int>(buf.size()));
        verstrichen += kBlock;
    }
    QVERIFY2(det.detected(), "Der Ton wurde innerhalb einer Sekunde nicht erkannt");
    QVERIFY2(verstrichen <= 0.5,
             qPrintable(QStringLiteral("brauchte %1 s, die Norm verlangt unter 0,5")
                            .arg(verstrichen)));
}

void TstCtcssDetector::derPegelGehtNichtEin()
{
    // Der Hub des Subtons schwankt von Gegenstelle zu Gegenstelle. Das
    // Verhaeltnis darf davon nicht abhaengen -- darum wird es gegen die
    // Bandleistung gebildet und nicht gegen einen festen Pegel.
    for (double amp : {0.50, 0.30, 0.20, 0.15, 0.10, 0.05}) {
        CtcssDetector det(kRate, 100.0);
        std::vector<float> buf = ton(100.0, 0.6, amp);
        rauschenDazu(buf, 0.2);
        det.process(buf.data(), static_cast<int>(buf.size()));
        QVERIFY2(det.detected(),
                 qPrintable(QStringLiteral("Hub %1 nicht erkannt (Verhaeltnis %2)")
                                .arg(amp).arg(det.lastRatio())));
    }
}

void TstCtcssDetector::einVerstimmterSenderWirdNochErkannt()
{
    // Die Norm erlaubt dem Sender +-1 %. Bei 100 Hz ist das +-1 Hz, und
    // das muss durchgehen -- sonst oeffnet die Sperre bei einer
    // Gegenstelle am Rand der Toleranz nicht.
    for (double ist : {99.0, 99.5, 100.0, 100.5, 101.0}) {
        CtcssDetector det(kRate, 100.0);
        const std::vector<float> buf = ton(ist, 0.6);
        det.process(buf.data(), static_cast<int>(buf.size()));
        QVERIFY2(det.detected(),
                 qPrintable(QStringLiteral("Sender auf %1 Hz nicht erkannt (Verhaeltnis %2)")
                                .arg(ist).arg(det.lastRatio())));
    }
}

void TstCtcssDetector::derTiefpassHaeltAliasingDraussen()
{
    // Ohne den Tiefpass vor der Dezimation klappen Anteile bei 550 und
    // 660 Hz auf 450 und 340 Hz zurueck, mitten ins Messband. Ein Ton
    // bei 550 Hz darf die auf 100 Hz gestellte Sperre nicht beruehren.
    for (double stoerHz : {550.0, 660.0, 900.0, 1450.0}) {
        CtcssDetector det(kRate, 100.0);
        std::vector<float> buf = ton(stoerHz, 0.8, 0.6);
        rauschenDazu(buf, 0.02);
        det.process(buf.data(), static_cast<int>(buf.size()));
        QVERIFY2(!det.detected(),
                 qPrintable(QStringLiteral("Stoerton %1 Hz oeffnete die Sperre "
                                           "(Verhaeltnis %2) -- klappt er ins Messband?")
                                .arg(stoerHz).arg(det.lastRatio())));
    }
}

void TstCtcssDetector::spracheAlleinOeffnetNie()
{
    // Fehloeffnen ist der Fehler, der im Betrieb weh tut: eine fremde
    // Station ohne Subton reisst die Sperre auf. Darum ueber die ganze
    // Spanne menschlicher Grundfrequenzen pruefen -- inklusive der
    // Faelle, in denen der Grundton GENAU auf dem Subton sitzt, und
    // inklusive einer sehr gleichmaessigen Stimme (wenig Jitter), die
    // einem Dauerton am naechsten kommt.
    for (double f0 : {85.0, 98.0, 100.0, 110.0, 125.0, 145.0, 180.0, 220.0}) {
        const int anteil = anteilOffenProzent(f0, 1.0, 0.0, 0.6);
        QVERIFY2(anteil == 0,
                 qPrintable(QStringLiteral("Sprache mit f0 %1 Hz oeffnete in %2 %% der Zeit")
                                .arg(f0).arg(anteil)));
    }
    for (double f0 : {99.0, 100.0, 101.0}) {
        const int anteil = anteilOffenProzent(f0, 0.2, 0.0, 0.6);
        QVERIFY2(anteil == 0,
                 qPrintable(QStringLiteral("gleichmaessige Stimme mit f0 %1 Hz oeffnete in "
                                           "%2 %% der Zeit").arg(f0).arg(anteil)));
    }
    // Und laut gesprochen genauso nicht.
    const int lautAnteil = anteilOffenProzent(110.0, 1.0, 0.0, 1.4);
    QVERIFY2(lautAnteil == 0,
             qPrintable(QStringLiteral("laute Sprache ohne Subton oeffnete in %1 %% der Zeit")
                            .arg(lautAnteil)));
}

void TstCtcssDetector::derSubtonUnterSpracheHaeltOffen()
{
    // So sieht es am Relais aus: Subton mit rund 15 % Hub unter Sprache.
    // Auch bei einem Sprecher, dessen Grundton im Subtonband liegt --
    // Longpath sendet FM mit [-3000, +3000] Hz, es gibt also keinen
    // 300-Hz-Hochpass, der die tiefe Stimme vom Subtonband fernhielte
    // (`TxChannel.cpp:965`).
    for (double f0 : {85.0, 98.0, 110.0, 145.0, 180.0}) {
        const int anteil = anteilOffenProzent(f0, 1.0, 0.15, 0.6);
        QVERIFY2(anteil >= 95,
                 qPrintable(QStringLiteral("Subton unter Sprache (f0 %1 Hz) hielt nur in "
                                           "%2 %% der Zeit offen").arg(f0).arg(anteil)));
    }
    // Und unter einer lauten Stimme auch.
    for (double f0 : {98.0, 110.0}) {
        const int anteil = anteilOffenProzent(f0, 1.0, 0.15, 1.2);
        QVERIFY2(anteil >= 95,
                 qPrintable(QStringLiteral("Subton unter LAUTER Sprache (f0 %1 Hz) hielt nur "
                                           "in %2 %% der Zeit offen").arg(f0).arg(anteil)));
    }
}

void TstCtcssDetector::derDurchgangHaeltDreissigSekunden()
{
    // Ein ganzer Durchgang, wie er wirklich ablaeuft: erst der Subton
    // allein (PTT gedrueckt, noch nicht gesprochen), dann 30 Sekunden
    // laute Sprache darueber. Die Sperre darf dabei kein einziges Mal
    // zufallen -- der Subton liegt ja durchgehend an.
    //
    // Dieser Fall ist auch der Grund, warum der Nachbarvergleich NUR
    // beim Oeffnen gilt und nicht beim Halten: eine laute Stimme fuellt
    // einen Nachbarzweig zeitweise staerker als den Soll-Zweig, und mit
    // dem Vergleich auch beim Halten fiel die Sperre mitten im
    // Durchgang zu.
    CtcssDetector det(kRate, 100.0);
    double phase = 0.0;
    double huellPhase = 0.0;
    {
        std::vector<float> buf = ton(100.0, 0.3, 0.15, &phase);
        rauschenDazu(buf, 0.03);
        det.process(buf.data(), static_cast<int>(buf.size()));
    }
    QVERIFY2(det.detected(), "Die Sperre oeffnete am Durchgangsbeginn nicht");

    Stimme stimme(kRate, 110.0, 1.0, 11);
    int zugefallen = 0;
    constexpr int kBloecke = 100;                 // 100 x 0,3 s = 30 s
    for (int k = 0; k < kBloecke; ++k) {
        std::vector<float> buf = stimme.block(0.3, 0.9, huellPhase);
        huellPhase += kRate * 0.3;
        const std::vector<float> sub = ton(100.0, 0.3, 0.15, &phase);
        for (size_t i = 0; i < buf.size(); ++i) { buf[i] += sub[i]; }
        rauschenDazu(buf, 0.05, static_cast<unsigned>(k + 200));
        det.process(buf.data(), static_cast<int>(buf.size()));
        if (!det.detected()) { ++zugefallen; }
    }
    QVERIFY2(zugefallen == 0,
             qPrintable(QStringLiteral("Die Sperre fiel in %1 von %2 Fenstern zu, obwohl der "
                                       "Subton durchgehend anlag").arg(zugefallen).arg(kBloecke)));
}

void TstCtcssDetector::nachDemTonendeSchliesstSieWieder()
{
    // Wenn der Subton weg ist, muss die Sperre zu. Ein Messfenster ist
    // 0,25 s, also darf es ein bis zwei Fenster dauern; unter 1,5 s muss
    // es durch sein.
    CtcssDetector det(kRate, 100.0);
    double phase = 0.0;
    {
        const std::vector<float> buf = ton(100.0, 0.6, 0.3, &phase);
        det.process(buf.data(), static_cast<int>(buf.size()));
    }
    QVERIFY2(det.detected(), "Die Sperre war vor dem Abschalten nicht offen");

    double verstrichen = 0.0;
    while (verstrichen < 2.0 && det.detected()) {
        std::vector<float> buf(static_cast<size_t>(kRate * 0.02), 0.0f);
        rauschenDazu(buf, 0.2, static_cast<unsigned>(verstrichen * 1000) + 1);
        det.process(buf.data(), static_cast<int>(buf.size()));
        verstrichen += 0.02;
    }
    QVERIFY2(!det.detected(), "Die Sperre blieb nach dem Tonende offen");
    QVERIFY2(verstrichen <= 1.5,
             qPrintable(QStringLiteral("brauchte %1 s zum Schliessen").arg(verstrichen)));
}

void TstCtcssDetector::sieFlattertNichtAnDerSchwelle()
{
    // Ein Ton knapp an der Grenze darf die Sperre nicht im Takt der
    // Fenster auf- und zuschlagen lassen. Dafuer ist die Hysterese da --
    // und dieser Fall ist es, der die Schliess-Schwelle auf 0,10
    // festlegt: mit 0,18 schlug sie hier viermal um.
    CtcssDetector det(kRate, 100.0);
    int wechsel = 0;
    double phase = 0.0;
    for (int block = 0; block < 40; ++block) {
        std::vector<float> buf = ton(100.0, 0.1, 0.09, &phase);
        rauschenDazu(buf, 1.0, static_cast<unsigned>(block + 1));
        if (det.process(buf.data(), static_cast<int>(buf.size()))) { ++wechsel; }
    }
    QVERIFY2(wechsel <= 2,
             qPrintable(QStringLiteral("Die Sperre flattert: %1 Wechsel in 40 Bloecken")
                            .arg(wechsel)));
}

QTEST_MAIN(TstCtcssDetector)
#include "tst_ctcss_detector.moc"
