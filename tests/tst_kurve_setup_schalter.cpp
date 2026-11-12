// no-port-check: Longpath-original test, keine Thetis-Logik. „Soft trace
// edge" und „Trace halo" haben kein Vorbild in Thetis -- sie kommen aus
// der Richtung „Glas & Tiefe" (2026-09-17).
//
// ── Die zwei Schalter in Setup › Display › Render (2026-10-08) ─────────
//
// Der haeufigste Fehler bei einem neuen Schalter ist nicht, dass er
// falsch rechnet, sondern dass er NICHTS tut: Kaestchen gebaut, Signal
// vergessen -- oder er tut etwas, liest aber beim Oeffnen der Seite
// seinen eigenen Zustand nicht zurueck und steht dann falsch da.
// Beides wird hier gefahren, in beide Richtungen.
//
// Dazu die Vorgabe selbst: weiche Kante AN, Hof AUS -- so hat der
// Betreiber am 2026-10-08 nach den drei Blaettern entschieden. Eine
// Vorgabe ohne Pruefung haelt keinen Umbau aus, und eine, die die Seite
// anders anzeigt als sie wirkt, waere schlimmer als gar keine.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QStandardPaths>

#include "core/FFTEngine.h"
#include "gui/SpectrumWidget.h"
#include "gui/setup/DisplaySetupPages.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Findet ein Kaestchen an seinem sichtbaren Text. Die Seite vergibt
// keine objectNames; der Text ist das, was der Betreiber liest, und
// damit das Richtige zum Festnageln.
QCheckBox* kaestchen(QWidget* seite, const QString& text)
{
    const auto alle = seite->findChildren<QCheckBox*>();
    for (QCheckBox* c : alle) {
        if (c->text() == text) { return c; }
    }
    return nullptr;
}

// Die Seite braucht BEIDE Haken am Modell: `loadFromRenderer()` steigt
// in einer Zeile aus, wenn die Kurve ODER die FFT-Maschine fehlt. Im
// Programm setzt MainWindow beide; hier also auch, sonst prueft der
// Rueckweg nichts.
struct Stand {
    SpectrumWidget kurve;
    FFTEngine fft{0};
    RadioModel modell;

    Stand()
    {
        modell.setSpectrumWidget(&kurve);
        modell.setFftEngine(&fft);
    }
};

}  // namespace

class TestKurveSetupSchalter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void dieBeidenSchalterStehenNebenDemVerlauf()
    {
        Stand s;
        SpectrumDefaultsPage seite(&s.modell);

        QCheckBox* weich = kaestchen(&seite, QStringLiteral("Soft trace edge"));
        QCheckBox* hof   = kaestchen(&seite, QStringLiteral("Trace halo"));
        QVERIFY2(weich != nullptr, "Kaestchen 'Soft trace edge' fehlt");
        QVERIFY2(hof != nullptr,   "Kaestchen 'Trace halo' fehlt");
        // Jeder Schalter sagt, was er tut -- sonst probiert man ihn aus.
        QVERIFY(!weich->toolTip().isEmpty());
        QVERIFY(!hof->toolTip().isEmpty());
    }

    // Die Vorgabe des Betreibers (2026-10-08): weiche Kante AN, Hof AUS.
    // Am Widget UND am Kaestchen -- eine Vorgabe, die die Seite anders
    // anzeigt als sie wirkt, waere schlimmer als gar keine.
    void dieVorgabeIstWeicheKanteOhneHof()
    {
        Stand s;
        QVERIFY(s.kurve.traceSoftEdge());
        QVERIFY(!s.kurve.traceHalo());

        SpectrumDefaultsPage seite(&s.modell);
        QVERIFY(kaestchen(&seite, QStringLiteral("Soft trace edge"))->isChecked());
        QVERIFY(!kaestchen(&seite, QStringLiteral("Trace halo"))->isChecked());
    }

    // Hinweg: Klick im Setup erreicht den Malweg -- in beide Richtungen,
    // also auch das ABschalten der Vorgabe. Wer die alte Kurve zurueck
    // will, muss sie zurueckbekommen.
    void dasKaestchenErreichtDieKurve()
    {
        Stand s;
        SpectrumDefaultsPage seite(&s.modell);

        QCheckBox* weich = kaestchen(&seite, QStringLiteral("Soft trace edge"));
        QCheckBox* hof   = kaestchen(&seite, QStringLiteral("Trace halo"));

        weich->setChecked(false);
        QVERIFY(!s.kurve.traceSoftEdge());
        QVERIFY(!s.kurve.traceHalo());   // der eine schaltet nicht den anderen

        hof->setChecked(true);
        QVERIFY(s.kurve.traceHalo());
        QVERIFY(!s.kurve.traceSoftEdge());

        weich->setChecked(true);
        QVERIFY(s.kurve.traceSoftEdge());
        QVERIFY(s.kurve.traceHalo());    // und nimmt ihn auch nicht mit
    }

    // Schaerfe (2026-11-08): drei Kaestchen neben weicher Kante und Hof.
    // Ab Werk an (Betreiber: „Alles zusammen"), Klick erreicht das Widget,
    // und eine Seite auf einem abgeschalteten Widget steht richtig da.
    void dieSchaerfeSchalterStehenDaUndWirken()
    {
        Stand s;
        {
            SpectrumDefaultsPage seite(&s.modell);
            QCheckBox* wf    = kaestchen(&seite, QStringLiteral("Waterfall at full resolution"));
            QCheckBox* haar  = kaestchen(&seite, QStringLiteral("Hairline grid"));
            QCheckBox* kante = kaestchen(&seite, QStringLiteral("Short trace edge"));
            QVERIFY2(wf && haar && kante, "ein Schaerfe-Kaestchen fehlt");
            QVERIFY(!wf->toolTip().isEmpty() && !haar->toolTip().isEmpty()
                    && !kante->toolTip().isEmpty());
            QVERIFY(wf->isChecked() && haar->isChecked() && kante->isChecked());

            wf->setChecked(false);
            QVERIFY(!s.kurve.waterfallDevicePixels());
            haar->setChecked(false);
            QVERIFY(!s.kurve.gridHairline());
            kante->setChecked(false);
            QCOMPARE(s.kurve.traceEdgePx(), 1.0f);
            kante->setChecked(true);
            QCOMPARE(s.kurve.traceEdgePx(), 0.5f);
        }
        SpectrumDefaultsPage seite2(&s.modell);
        QVERIFY(!kaestchen(&seite2, QStringLiteral("Waterfall at full resolution"))->isChecked());
        QVERIFY(!kaestchen(&seite2, QStringLiteral("Hairline grid"))->isChecked());
        QVERIFY(kaestchen(&seite2, QStringLiteral("Short trace edge"))->isChecked());
    }

    // Rueckweg: eine Seite, die auf ein bereits eingeschaltetes Widget
    // gebaut wird, steht richtig da.
    void dieSeiteLiestDenZustandZurueck()
    {
        Stand s;
        s.kurve.setTraceSoftEdge(true);
        s.kurve.setTraceHalo(true);

        SpectrumDefaultsPage seite(&s.modell);
        QVERIFY(kaestchen(&seite, QStringLiteral("Soft trace edge"))->isChecked());
        QVERIFY(kaestchen(&seite, QStringLiteral("Trace halo"))->isChecked());
    }

    // Der Setter tut nichts, wenn sich nichts aendert -- sonst schriebe
    // jedes Oeffnen der Seite die Einstellungen neu.
    void derSetterKenntDenLeerlauf()
    {
        SpectrumWidget sw;
        sw.setTraceSoftEdge(true);    // war schon an (Vorgabe)
        QVERIFY(sw.traceSoftEdge());
        sw.setTraceSoftEdge(false);
        sw.setTraceSoftEdge(false);   // zweimal dasselbe
        QVERIFY(!sw.traceSoftEdge());
    }
};

QTEST_MAIN(TestKurveSetupSchalter)
#include "tst_kurve_setup_schalter.moc"
