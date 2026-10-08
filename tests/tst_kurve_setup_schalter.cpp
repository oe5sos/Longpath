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
// Dazu die Vorgabe selbst: BEIDE AUS. Sie ist eine Zusage an den
// Betreiber (wie die Kurve aussieht, entscheidet er), und eine Vorgabe
// ohne Pruefung haelt keinen Umbau aus.

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

    // Ab Werk aus, am Widget UND am Kaestchen.
    void abWerkSindBeideAus()
    {
        Stand s;
        QVERIFY(!s.kurve.traceSoftEdge());
        QVERIFY(!s.kurve.traceHalo());

        SpectrumDefaultsPage seite(&s.modell);
        QVERIFY(!kaestchen(&seite, QStringLiteral("Soft trace edge"))->isChecked());
        QVERIFY(!kaestchen(&seite, QStringLiteral("Trace halo"))->isChecked());
    }

    // Hinweg: Klick im Setup erreicht den Malweg.
    void dasKaestchenErreichtDieKurve()
    {
        Stand s;
        SpectrumDefaultsPage seite(&s.modell);

        QCheckBox* weich = kaestchen(&seite, QStringLiteral("Soft trace edge"));
        QCheckBox* hof   = kaestchen(&seite, QStringLiteral("Trace halo"));

        weich->setChecked(true);
        QVERIFY(s.kurve.traceSoftEdge());
        QVERIFY(!s.kurve.traceHalo());   // der eine schaltet nicht den anderen

        hof->setChecked(true);
        QVERIFY(s.kurve.traceHalo());

        weich->setChecked(false);
        QVERIFY(!s.kurve.traceSoftEdge());
        QVERIFY(s.kurve.traceHalo());    // und nimmt ihn auch nicht mit
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
        sw.setTraceSoftEdge(false);   // war schon aus
        QVERIFY(!sw.traceSoftEdge());
        sw.setTraceSoftEdge(true);
        sw.setTraceSoftEdge(true);    // zweimal dasselbe
        QVERIFY(sw.traceSoftEdge());
    }
};

QTEST_MAIN(TestKurveSetupSchalter)
#include "tst_kurve_setup_schalter.moc"
