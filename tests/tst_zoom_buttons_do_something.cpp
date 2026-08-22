// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die vier Zoomknoepfe (S B − +) muessen die Ansicht aendern.
//
// Der Betreiber am 2026-08-22: "test auch mal, ob das plus und minus
// funktioniert, welches den pandapter vergrößert." Gemessen am
// laufenden Geraet, Frequenzskala vor und nach dem Druck: Punkt fuer
// Punkt IDENTISCH. Ursache: zoomIn/zoomOut/zoomBand/zoomSegment sind
// Signale, und es hat ihnen nie jemand zugehoert. Die Knoepfe sandten
// ins Leere.
//
// Derselbe Fehlertyp war hier schon einmal aufgefallen ("B8 Task 20:
// wire Display-flyout orphaned signals" — Gain, Schwarzwert, Schema);
// die vier Zoomsignale hat man dabei uebersehen. Deshalb prueft dieser
// Test nicht "ist verbunden", sondern die WIRKUNG: die Bandbreite muss
// sich aendern.

#include <QtTest>
#include <QPushButton>

#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpectrumOverlayPanel.h"

using namespace Longpath;

class TstZoomButtonsDoSomething : public QObject
{
    Q_OBJECT

private:
    MainWindow*          m_mw{nullptr};
    SpectrumWidget*      m_sw{nullptr};
    SpectrumOverlayPanel* m_panel{nullptr};

private slots:
    void initTestCase()
    {
        m_mw = new MainWindow();
        m_mw->resize(1200, 800);
        m_mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mw));
        for (int i = 0; i < 8; ++i) { QCoreApplication::processEvents(); }

        m_sw = m_mw->findChild<SpectrumWidget*>();
        QVERIFY2(m_sw, "Kein Spektrum im Hauptfenster");
        m_panel = m_mw->findChild<SpectrumOverlayPanel*>();
        QVERIFY2(m_panel, "Keine Overlay-Leiste im Hauptfenster");

        m_sw->setSampleRate(384000.0);
        m_sw->setFrequencyRange(7'100'000.0, 100'000.0);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
    }

    void plusNarrowsTheView()
    {
        m_sw->setFrequencyRange(7'100'000.0, 100'000.0);
        const double before = m_sw->bandwidth();
        emit m_panel->zoomIn();
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(m_sw->bandwidth() < before * 0.9,
                 qPrintable(QStringLiteral(
                     "'+' aendert nichts: %1 Hz vor, %2 Hz nach dem "
                     "Druck — genau der Befund des Betreibers")
                     .arg(before).arg(m_sw->bandwidth())));
    }

    void minusWidensTheView()
    {
        m_sw->setFrequencyRange(7'100'000.0, 100'000.0);
        const double before = m_sw->bandwidth();
        emit m_panel->zoomOut();
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(m_sw->bandwidth() > before * 1.1,
                 qPrintable(QStringLiteral("'−' aendert nichts: %1 -> %2")
                     .arg(before).arg(m_sw->bandwidth())));
    }

    void theViewStaysWithinWhatTheReceiverDelivers()
    {
        // Zwoelfmal auf '−': ohne Deckel liefe die Ansicht ins
        // Sinnlose — breiter als die Abtastrate gibt es keine Daten.
        m_sw->setFrequencyRange(7'100'000.0, 100'000.0);
        for (int i = 0; i < 20; ++i) {
            emit m_panel->zoomOut();
            QCoreApplication::processEvents();
        }
        QVERIFY2(m_sw->bandwidth() <= m_sw->sampleRate() + 1.0,
                 qPrintable(QStringLiteral(
                     "Ansicht %1 Hz breiter als die Abtastrate %2 Hz")
                     .arg(m_sw->bandwidth()).arg(m_sw->sampleRate())));

        // Und andersherum: nicht unter 2 kHz, sonst sieht man nur noch
        // die eigene Durchlasskurve.
        for (int i = 0; i < 30; ++i) {
            emit m_panel->zoomIn();
            QCoreApplication::processEvents();
        }
        QVERIFY2(m_sw->bandwidth() >= 1999.0,
                 qPrintable(QStringLiteral("Ansicht auf %1 Hz geschrumpft")
                     .arg(m_sw->bandwidth())));
    }

    void theButtonsAreActuallyClickable()
    {
        // Der eigentliche Fehler war NICHT die fehlende Verdrahtung
        // allein. Selbst danach tat sich am Geraet nichts: der
        // Zoomstreifen war als WA_NativeWindow angelegt (Notbehelf vom
        // 2026-08-20 gegen Unsichtbarkeit) — und ein natives Kind im
        // nativen Panadapter bekommt keine Mausereignisse mehr.
        // Sichtbar und taub.
        //
        // Deshalb wird hier nicht das Signal ausgeloest, sondern
        // GEKLICKT — auf demselben Weg wie der Bediener.
        // Ueber den Namen, nicht ueber die Beschriftung: es gibt
        // mehrere "+" im Fenster, und die erste Fassung dieser Pruefung
        // erwischte prompt das falsche.
        QPushButton* plus = m_mw->findChild<QPushButton*>(
            QStringLiteral("panZoomInBtn"));
        QVERIFY2(plus, "Kein panZoomInBtn im Hauptfenster gefunden");

        m_sw->setFrequencyRange(7'100'000.0, 100'000.0);
        const double before = m_sw->bandwidth();
        QTest::mouseClick(plus, Qt::LeftButton);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(m_sw->bandwidth() < before * 0.9,
                 qPrintable(QStringLiteral(
                     "Ein echter Klick auf '+' bewirkt nichts (%1 -> %2). "
                     "Steht der Streifen wieder auf WA_NativeWindow?")
                     .arg(before).arg(m_sw->bandwidth())));
    }

    void cleanupTestCase()
    {
        // Bewusst NICHT geloescht. Ein echtes Hauptfenster mit
        // GPU-Spektrum abzureissen ist ein eigenes Kapitel (siehe
        // tst_closing_takes_the_float_along); hier wuerde es nur die
        // Messung oben verdecken. Der Prozess endet gleich ohnehin.
        m_mw = nullptr;
    }
};

QTEST_MAIN(TstZoomButtonsDoSomething)
#include "tst_zoom_buttons_do_something.moc"
