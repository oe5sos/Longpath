// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf, kein Port. Die eine
// Thetis-Nennung (display.cs:5008) steht als Herkunft der
// Torbedingung in einem Kommentar -- es wird kein Thetis-Code
// uebersetzt, sondern Longpath-Verhalten geprueft.
//
// ── Drei Befunde aus NereusSDRs PureSignal-Diagnose, bei uns nachgeprueft ──
//
// NereusSDR hat am 2026-10-12 in seinem Entwurfsblatt sechs bestaetigte
// Befunde zur Zweiton-/PureSignal-Schiene notiert. Zwei davon (doppelter
// Zuschnitt, stilles Versagen) waren bei uns am 2026-10-21 unabhaengig
// gefunden und behoben. Dieser Lauf haelt die drei fest, die am
// 2026-10-22 dazukamen:
//
//   Befund 3  Die Analyse lief IM Malvorgang. Damit hing jeder Messwert
//             und jede Protokollzeile davon ab, WANN Qt neu zeichnete --
//             bei verdecktem Fenster gar nicht. Das erklaert die
//             Messreihen vom 2026-10-21, in denen Werte mal kamen und
//             mal nicht.
//
//   Befund 4  Die drei Zustaende des Zweiton-Felds (MOX, Zweitongenerator,
//             Messhaken) hingen einmalig an activeSpectrumWidget() --
//             also an Pan 0, und zwar am OBJEKT, nicht am Ausdruck. Auf
//             jedem anderen Pan blieb das Tor fuer immer zu.
//
//   Befund 6  Im GPU-Weg liegen Marken und Messfeld in einer
//             zwischengespeicherten Textur. Die wurde nur fuer veraltet
//             erklaert, wenn Spitzenhalten, Spitzenblasen, Rauschflur
//             oder 3D-Modus liefen -- das Zweiton-Feld stand nicht in
//             der Liste. update() allein baut die Textur nicht neu.
//
// Gegenprobe (2026-10-22, gegen die Fassung von 11c2008ef):
//   * Befund 3: analyseLaeuftOhneMalvorgang faellt mit „kein Messwert",
//     weil die Analyse ohne paintEvent nie lief.
//   * Befund 6: torschlussEntwertetDieSchicht faellt, weil
//     setTestingIMD(false) nur update() rief.
//   * Befund 4: zweiterPanBekommtDasZweitonTor faellt mit
//     testingIMD=false auf Pan 1.

#include <QtTest>

#include <cmath>

#include "gui/ImdOverlay.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "core/TwoToneController.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

// Ein Zweiton, wie ihn der Generator erzeugt: zwei gleich starke Traeger
// mit Abstand, dazu die IMD3- und IMD5-Produkte -- alles als LINEARE
// Leistung, weil updateSpectrumLinear das erwartet.
//
// Die Pegel entsprechen der Messreihe vom 2026-10-21 am echten Sender
// (f0 bei -32 dBm, IMD3 rund 19 dB darunter, IMD5 rund 31 dB darunter).
QVector<float> zweitonBins(int n)
{
    QVector<float> bins(n, 0.0f);
    auto setzen = [&](int bin, float dbm) {
        if (bin >= 0 && bin < n) {
            bins[bin] = static_cast<float>(std::pow(10.0, dbm / 10.0));
        }
    };
    // Grundrauschen, damit die Spitzensuche ueberhaupt Flanken findet.
    for (int i = 0; i < n; ++i) {
        setzen(i, -110.0f);
    }
    const int mitte = n / 2;
    const int d     = n / 8;          // Tonabstand in Bins
    setzen(mitte - d / 2, -32.0f);    // f0 unten
    setzen(mitte + d / 2, -32.0f);    // f0 oben
    setzen(mitte - d / 2 - d, -51.0f);      // IMD3 unten
    setzen(mitte + d / 2 + d, -51.0f);      // IMD3 oben
    setzen(mitte - d / 2 - 2 * d, -63.0f);  // IMD5 unten
    setzen(mitte + d / 2 + 2 * d, -63.0f);  // IMD5 oben
    return bins;
}

// Das Tor aus Thetis display.cs:5008 [v2.10.3.13] von Hand aufmachen.
void torAufmachen(SpectrumWidget* sw)
{
    sw->setMoxOverlay(true);
    sw->setTestingIMD(true);
    sw->setShowIMDMeasurements(true);
}

} // namespace

class TstZweitonAusserhalbMalvorgang : public QObject
{
    Q_OBJECT

private slots:

    // ── Befund 3 ────────────────────────────────────────────────────────
    //
    // Kein show(), kein paintEvent, kein processEvents: nur ein
    // Spektrumsbild hinein. Danach MUSS ein Messwert dastehen.
    void analyseLaeuftOhneMalvorgang()
    {
        SpectrumWidget sw;
        sw.resize(900, 400);
        torAufmachen(&sw);

        QVERIFY2(!sw.imdAnalysisValidForTest(),
                 "vor dem ersten Bild darf noch kein Ergebnis stehen");

        constexpr int kBins = 2048;
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 192000.0);
        sw.updateSpectrumLinear(0, zweitonBins(kBins), 1.0, 0.0);

        QVERIFY2(sw.imdAnalysisValidForTest(),
                 "kein Messwert: die Analyse lief nicht ohne Malvorgang");

        const ImdOverlay* werk = sw.imdOverlayForTest();
        QVERIFY(werk != nullptr);
        const ImdReadout& r = werk->readout();
        QVERIFY2(r.valid, "Messfeld ungueltig, obwohl die Analyse lief");
        // Die zwei Grundtoene bei -32 dBm, mit Toleranz fuer Detektor
        // und EMA-Anlauf.
        QVERIFY2(r.dbc > -45.0f && r.dbc < -20.0f,
                 qPrintable(QStringLiteral("dbc=%1 liegt nicht bei -32 dBm")
                                .arg(r.dbc)));
        // IMD3 rund 19 dB unter dem Grundton.
        QVERIFY2(r.worstImd3DBc > 5.0f,
                 qPrintable(QStringLiteral("worstIMD3=%1, erwartet > 5 dB")
                                .arg(r.worstImd3DBc)));
    }

    // Faellt das Tor, verfaellt das Ergebnis -- sonst zeichnet der
    // Malvorgang alte Marken weiter.
    void torschlussVerwirftDasErgebnis()
    {
        SpectrumWidget sw;
        sw.resize(900, 400);
        torAufmachen(&sw);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 192000.0);
        sw.updateSpectrumLinear(0, zweitonBins(2048), 1.0, 0.0);
        QVERIFY(sw.imdAnalysisValidForTest());

        sw.setTestingIMD(false);
        sw.updateSpectrumLinear(0, zweitonBins(2048), 1.0, 0.0);
        QVERIFY2(!sw.imdAnalysisValidForTest(),
                 "nach Torschluss stand das alte Ergebnis noch da");
    }

    // ── Befund 6 ────────────────────────────────────────────────────────
    //
    // Jeder Schalter am Zweiton-Tor muss die zwischengespeicherte
    // Overlay-Schicht entwerten. Auf einem Bau ohne GPU gibt es keine
    // Textur -- dann ist nichts zu pruefen.
    void torschlussEntwertetDieSchicht()
    {
#ifndef LONGPATH_GPU_SPECTRUM
        QSKIP("Bau ohne GPU-Spektrum: keine zwischengespeicherte Schicht");
#endif
        SpectrumWidget sw;
        sw.resize(900, 400);
        torAufmachen(&sw);

        sw.clearOverlayDynamicDirtyForTest();
        QVERIFY(!sw.overlayDynamicDirtyForTest());
        sw.setTestingIMD(false);
        QVERIFY2(sw.overlayDynamicDirtyForTest(),
                 "setTestingIMD(false) entwertete die Schicht nicht");

        sw.clearOverlayDynamicDirtyForTest();
        sw.setShowIMDMeasurements(false);
        QVERIFY2(sw.overlayDynamicDirtyForTest(),
                 "setShowIMDMeasurements(false) entwertete die Schicht nicht");
    }

    // Und bei offenem Tor muss jedes Spektrumsbild die Schicht fuer
    // veraltet erklaeren, auch ohne Spitzenhalten und Rauschflur --
    // sonst bleiben die Marken auf dem Stand eines fremden Neubaus.
    void offenesTorEntwertetJedesBild()
    {
#ifndef LONGPATH_GPU_SPECTRUM
        QSKIP("Bau ohne GPU-Spektrum: keine zwischengespeicherte Schicht");
#endif
        SpectrumWidget sw;
        sw.resize(900, 400);
        torAufmachen(&sw);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 192000.0);

        sw.clearOverlayDynamicDirtyForTest();
        sw.updateSpectrumLinear(0, zweitonBins(2048), 1.0, 0.0);
        QVERIFY2(sw.overlayDynamicDirtyForTest(),
                 "das Zweiton-Feld fehlt in der Schmutzbedingung");
    }

    // ── Befund 4 ────────────────────────────────────────────────────────
    //
    // Am echten Hauptfenster: ein zweiter Pan, und der Zweitongenerator
    // muss auch dort ankommen.
    void zweiterPanBekommtDasZweitonTor()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        auto* stapel = mw->findChild<PanadapterStack*>();
        QVERIFY2(stapel != nullptr, "kein Panadapter-Stapel am Fenster");

        // Zwei Pans nebeneinander: damit entsteht pan-1.
        stapel->applyLayout(QStringLiteral("2h"),
                            {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QTest::qWait(50);

        SpectrumWidget* zweiter = stapel->spectrum(QStringLiteral("pan-1"));
        QVERIFY2(zweiter != nullptr, "pan-1 hat kein Spektrum");

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model != nullptr);
        TwoToneController* tt = model->twoToneController();
        QVERIFY2(tt != nullptr, "kein Zweitongenerator am Modell");

        // Der Generator meldet sich an. Vor dem 2026-10-22 kam das nur
        // bei Pan 0 an.
        emit tt->twoToneActiveChanged(true);
        QTest::qWait(10);

        QVERIFY2(zweiter->testingIMDForTest(),
                 "pan-1 hat den Zweitongenerator nicht mitbekommen");

        delete mw;
    }
};

QTEST_MAIN(TstZweitonAusserhalbMalvorgang)
#include "tst_zweiton_ausserhalb_malvorgang.moc"
