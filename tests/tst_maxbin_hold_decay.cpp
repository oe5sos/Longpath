// Der MaxBin-Haltewert blieb nach einem sehr starken Signal oben kleben.
//
// Thetis rechnet (wdsp/analyzer.c:815-818 [@501e3f5], bei uns bis zum
// 2026-10-09 Zeile fuer Zeile uebernommen):
//
//     held -= fabs((1 - decay) * held);
//
// Der Abzug haengt am BETRAG des Haltewerts. Oberhalb 0 dBm schrumpft
// er mit dem Wert selbst, der Haltewert naehert sich 0 dBm asymptotisch
// und unterschreitet sie nie -- die Anzeige klebt (Thetis #2681). Die
// Vorlage hat denselben Fehler im Oktober 2026 behoben; hier steht eine
// eigene Fassung: Abfall gegen einen Boden bei -160 dBm statt gegen 0.
//
// Dieser Pruefstand faellt gegen die alte Fassung -- das ist sein Zweck.

#include <QtTest>

#include <cmath>

#include "core/WdspEngine.h"

using namespace Longpath;

namespace {

// Die Rechnung, wie sie bis zum 2026-10-09 im Baum stand. Nur hier, als
// Gegenprobe: so sieht man, was der Fehler wirklich tat.
double altThetisHold(double heldDbm, double liveDbm, double decay)
{
    double next = heldDbm - std::abs((1.0 - decay) * heldDbm);
    return liveDbm > next ? liveDbm : next;
}

// tau = 0.5 s bei 20 Bildern/s -- die Werte, mit denen MainWindow den
// Detektor fuer das S-Meter einrichtet.
constexpr double kDecay = 0.90483741803595952;  // exp(-1/(0.5*20))

} // namespace

class TstMaxBinHoldDecay : public QObject
{
    Q_OBJECT

private slots:
    void theOldFormulaStickedAboveZeroDbm();
    void theHoldNowFallsFromAStrongSignal();
    void itStillRidesAStrongerLiveReadingAtOnce();
    void theWorkingRangeDecaysAsBefore();
    void itNeverFallsBelowTheFloorAndKeepsTheSentinel();
};

// Erst der Beweis, dass es den Fehler wirklich gab: mit der alten
// Rechnung steht der Haltewert nach einer vollen Minute (1200 Bilder)
// immer noch ueber 0 dBm.
void TstMaxBinHoldDecay::theOldFormulaStickedAboveZeroDbm()
{
    double held = 10.0;
    for (int frame = 0; frame < 1200; ++frame) {
        held = altThetisHold(held, -400.0, kDecay);
    }
    qInfo() << "alte Rechnung, +10 dBm nach 1200 Bildern:" << held;
    QVERIFY2(held > 0.0, "Der Fehler ist in der Gegenprobe nicht mehr da");
}

void TstMaxBinHoldDecay::theHoldNowFallsFromAStrongSignal()
{
    double held = 10.0;
    int frames = 0;
    // Zwanzig Sekunden bei 20 Bildern/s muessen weit mehr als genug sein.
    for (; frames < 400; ++frames) {
        held = WdspEngine::maxBinHoldNextDbm(held, -400.0, kDecay);
        if (held < -100.0) { break; }
    }
    qInfo() << "neue Rechnung: +10 dBm unter -100 dBm nach" << frames << "Bildern =" << held;
    QVERIFY2(held < -100.0, "Der Haltewert klebt immer noch oben");
    // Und zwar zuegig: in unter zwei Sekunden (40 Bilder bei 20/s).
    // Gemessen sind es 33 -- der Abzug betraegt im oberen Bereich den
    // Mindestbezug von 20 dB mal (1 - decay) und waechst erst, wenn der
    // Betrag des Haltewerts darueber hinausgeht.
    QVERIFY2(frames < 40, qPrintable(QStringLiteral("brauchte %1 Bilder").arg(frames)));
}

void TstMaxBinHoldDecay::itStillRidesAStrongerLiveReadingAtOnce()
{
    // Peak-Attack unveraendert: ein staerkerer Messwert gilt sofort.
    QCOMPARE(WdspEngine::maxBinHoldNextDbm(-120.0, -42.0, kDecay), -42.0);
    // Ein schwaecherer zieht den Haltewert nicht schneller herunter als
    // bisher -- hier faellt die Fassung mit dem Boden bei -160 dBm
    // durch, die bei -42 dBm dreimal so schnell abfiel.
    const double neu = WdspEngine::maxBinHoldNextDbm(-42.0, -120.0, kDecay);
    const double alt = altThetisHold(-42.0, -120.0, kDecay);
    qInfo() << "bei -42 dBm: alt" << alt << "neu" << neu;
    QVERIFY2(std::abs(alt - neu) < 0.01, "Der Abfall im starken Bereich hat sich geaendert");
}

void TstMaxBinHoldDecay::theWorkingRangeDecaysAsBefore()
{
    // Bei -80 dBm -- dem Bereich, in dem dieses Instrument wirklich
    // arbeitet -- muss die neue Rechnung dieselbe sein wie die alte,
    // sonst waere es keine Fehlerbehebung, sondern eine Umstellung.
    const double alt  = altThetisHold(-80.0, -400.0, kDecay);
    const double neu  = WdspEngine::maxBinHoldNextDbm(-80.0, -400.0, kDecay);
    qInfo() << "bei -80 dBm: alt" << alt << "neu" << neu;
    QVERIFY2(std::abs(alt - neu) < 0.01,
             qPrintable(QStringLiteral("Arbeitsbereich weicht ab: %1 statt %2").arg(neu).arg(alt)));
}

void TstMaxBinHoldDecay::itNeverFallsBelowTheFloorAndKeepsTheSentinel()
{
    // Ohne Signal laeuft der Haltewert nach unten, aber nicht ueber den
    // Sentinel aus Init_DetectMaxBin hinaus.
    double held = -150.0;
    for (int frame = 0; frame < 2000; ++frame) {
        held = WdspEngine::maxBinHoldNextDbm(held, -400.0, kDecay);
    }
    qInfo() << "nach 2000 Bildern ohne Signal:" << held;
    QCOMPARE(held, -400.0);
    QCOMPARE(WdspEngine::maxBinHoldNextDbm(-400.0, -400.0, kDecay), -400.0);
}

QTEST_MAIN(TstMaxBinHoldDecay)
#include "tst_maxbin_hold_decay.moc"
