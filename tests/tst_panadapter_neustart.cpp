// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Panadapter-Einstellungen ueberleben das Beenden (2026-11-26) ─────────
//
// Betreiber: „und die einstellungen im panadapter sollten vom letzten
// beenden gespeichert sein!" Gemessen wird der Weg, den das Programm
// geht: MainWindow::closeEvent ruft saveSettings() jedes Panadapters,
// der Start baut ihn neu und ruft loadSettings() (configureSpectrumForPan).
// Verglichen wird nicht nur, was gespeichert ist, sondern was WIRKT --
// beim Wasserfall-Tempo stand der gespeicherte Wert im Menue, der Takt
// lief aber mit der Vorgabe von 30 ms weiter.
//
// Gegenprobe gefahren (2026-11-26): ohne das Nachziehen des Taktes in
// loadSettings() faellt `dasWasserfallTempoWirktNachDemNeustart`.

#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestPanadapterNeustart : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    // Alles aus der Liste des Betreibers, so weit es dem Panadapter gehoert.
    void alleEinstellungenUeberlebenDenNeustart()
    {
        QTemporaryDir dir;
        const QString bild = dir.filePath(QStringLiteral("hintergrund.png"));
        QImage(8, 8, QImage::Format_RGB32).save(bild);
        {
            SpectrumWidget w;
            w.loadSettings();                       // wie beim Start
            w.setSpectrumScheme(PanColourScheme::Brass);
            w.setWaterfallScheme(PanColourScheme::Night);
            w.setWfUpdatePeriodMs(135);
            w.setSpectrumRenderMode(SpectrumRenderMode::Mode3D);
            w.setPassbandMagnifier(0);
            w.setFrequencyRange(7188000.0, 24000.0);
            w.setDbmRange(-130.0f, -60.0f);
            w.setHoldReceivePictureOnTx(false);
            w.setTxPassbandLikeRx(false);
            w.setPassbandLabelShown(false);
            w.setSignalBracketsShown(false);
            w.setGapFinderShown(false);
            w.setSUnitsScaleShown(false);
            w.setBackgroundImage(bild);
            w.saveSettings();                       // wie closeEvent
        }
        SpectrumWidget n;
        n.loadSettings();
        QCOMPARE(n.spectrumScheme(), PanColourScheme::Brass);
        QCOMPARE(n.waterfallScheme(), PanColourScheme::Night);
        QCOMPARE(n.wfUpdatePeriodMs(), 135);
        QCOMPARE(n.spectrumRenderMode(), SpectrumRenderMode::Mode3D);
        QCOMPARE(n.passbandMagnifier(), 0);
        QCOMPARE(n.bandwidth(), 24000.0);
        QCOMPARE(n.refLevel(), -60.0f);
        QCOMPARE(n.dynamicRange(), 70.0f);
        QCOMPARE(n.holdReceivePictureOnTx(), false);
        QCOMPARE(n.txPassbandLikeRx(), false);
        QCOMPARE(n.passbandLabelShown(), false);
        QCOMPARE(n.signalBracketsShown(), false);
        QCOMPARE(n.gapFinderShown(), false);
        QCOMPARE(n.sUnitsScaleShown(), false);
        QCOMPARE(n.backgroundImagePath(), bild);
    }

    // Der Fehler, der wirklich da war: gespeichert, angezeigt -- aber der
    // Takt lief nach dem Start mit 30 ms.
    void dasWasserfallTempoWirktNachDemNeustart()
    {
        {
            SpectrumWidget w;
            w.loadSettings();
            w.setWfUpdatePeriodMs(135);
            QCOMPARE(w.waterfallTickerPeriodMsForTest(), 135);
            w.saveSettings();
        }
        SpectrumWidget n;
        n.loadSettings();
        QCOMPARE(n.wfUpdatePeriodMs(), 135);
        QCOMPARE(n.waterfallTickerPeriodMsForTest(), 135);
    }
};

QTEST_MAIN(TestPanadapterNeustart)
#include "tst_panadapter_neustart.moc"
