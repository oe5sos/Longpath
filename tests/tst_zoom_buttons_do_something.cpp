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
#include <QMouseEvent>
#include <QSignalSpy>

#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
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

    void theArrowKeysReachThePanadapterInTheRealWindow()
    {
        // Der Betreiber am 2026-08-22: "mit dem cursor auf der
        // tastatur kann ich auch nicht nach rechts und links fahren."
        //
        // Im NACKTEN Widget geht es (tst_mouse_follows_aether). Also
        // muss der Unterschied im vollen Fenster liegen — dort gibt es
        // Eingabefelder, Ereignisfilter und eine Tabulator-Reihenfolge,
        // die dem Panadapter den Fokus wegnehmen koennen.
        m_sw->setStepSize(100);
        m_sw->setConnectionState(ConnectionState::Connected);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        const QPoint p(m_sw->width() / 2, m_sw->height() / 4);
        QMouseEvent press(QEvent::MouseButtonPress, p, m_sw->mapToGlobal(p),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(m_sw, &press);
        QMouseEvent rel(QEvent::MouseButtonRelease, p, m_sw->mapToGlobal(p),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(m_sw, &rel);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        // focusWidget() DES FENSTERS, nicht hasFocus(): letzteres
        // verlangt ein aktives Fenster, und im Pruefstand ist keins
        // aktiv. Qt merkt sich den Fokus trotzdem je Fenster — genau
        // das wird hier gelesen. (Erste Fassung fragte hasFocus() und
        // meldete "niemand", obwohl der Fokus richtig gesetzt war.)
        QWidget* f = m_mw->focusWidget();
        qInfo() << "Fokus nach dem Klick:"
                << (f ? f->metaObject()->className() : "keiner");
        QVERIFY2(f == m_sw,
                 qPrintable(QStringLiteral(
                     "Nach einem Klick ins Spektrum hat '%1' den Fokus, "
                     "nicht der Panadapter — dann kommt keine "
                     "Pfeiltaste an")
                     .arg(f ? QString::fromLatin1(f->metaObject()->className())
                            : QStringLiteral("niemand"))));

        QSignalSpy tuned(m_sw, &SpectrumWidget::frequencyClicked);
        QTest::keyClick(m_sw, Qt::Key_Right);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(tuned.count() > 0, "Die Pfeiltaste kommt nicht an");
    }

    void changingTheWidthShowsUpInTheDiagram()
    {
        // Der Betreiber am 2026-08-22: "auch wenn ich die bandbreite
        // oben ändere sehe ich keine änderung im diagramm."
        //
        // setFilterOffset() wurde nur beim Anlegen einer Scheibe und
        // beim Wechsel der aktiven gerufen — bei einer Aenderung der
        // BREITE nirgends. Der Wert im Modell stimmte, die DSP bekam
        // ihn, nur der tuerkise Balken blieb stehen.
        RadioModel* model = m_mw->findChild<RadioModel*>();
        if (!model) { QSKIP("Kein RadioModel im Fenster"); }
        if (model->slices().isEmpty()) {
            model->addSlice();
            for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        }
        if (model->slices().isEmpty()) { QSKIP("Keine Scheibe"); }
        SliceModel* s = model->slices().first();

        s->setFilterByHand(-2900, -100);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        const int lowBefore  = m_sw->filterLowHz();
        const int highBefore = m_sw->filterHighHz();

        s->setFilterByHand(-3800, -100);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        QVERIFY2(m_sw->filterLowHz() != lowBefore
                     || m_sw->filterHighHz() != highBefore,
                 qPrintable(QStringLiteral(
                     "Der Panadapter zeigt weiter %1..%2 Hz, obwohl die "
                     "Scheibe auf -3800..-100 steht")
                     .arg(lowBefore).arg(highBefore)));
        QCOMPARE(m_sw->filterLowHz(), -3800);
    }

    void aClickInTheSpectrumMovesTheBar()
    {
        // Der Betreiber am 2026-08-22: "balken spring nicht dort hin."
        //
        // Im nackten Widget kommt das Signal an (tst_mouse_follows_
        // aether). Geprueft wird deshalb hier: kommt es bis zur
        // SCHEIBE — also bis zu dem, was der Balken anzeigt.
        RadioModel* model = m_mw->findChild<RadioModel*>();
        if (!model) { QSKIP("Kein RadioModel"); }
        if (model->slices().isEmpty()) {
            model->addSlice();
            for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        }
        if (model->slices().isEmpty()) { QSKIP("Keine Scheibe"); }
        SliceModel* s = model->slices().first();

        m_sw->setConnectionState(ConnectionState::Connected);
        m_sw->setSampleRate(192'000.0);
        m_sw->setDdcCenterFrequency(7'100'000.0);
        m_sw->setFrequencyRange(7'100'000.0, 100'000.0);
        s->setFrequency(7'100'000.0);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        const double before = s->frequency();
        const QPoint p(m_sw->width() * 3 / 4, m_sw->height() / 4);
        QMouseEvent press(QEvent::MouseButtonPress, p, m_sw->mapToGlobal(p),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(m_sw, &press);
        QMouseEvent rel(QEvent::MouseButtonRelease, p, m_sw->mapToGlobal(p),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(m_sw, &rel);
        for (int i = 0; i < 8; ++i) { QCoreApplication::processEvents(); }

        qInfo() << "Scheibe vorher:" << before << "nachher:" << s->frequency()
                ;
        QVERIFY2(!qFuzzyCompare(s->frequency(), before),
                 qPrintable(QStringLiteral(
                     "Der Klick bewegt die Scheibe nicht: %1 -> %2")
                     .arg(before).arg(s->frequency())));
    }

    void theArrowKeysReallyChangeTheFrequency()
    {
        // Der Betreiber am 2026-08-22: "cursor tastatur sollte auch
        // die frequenz ändern."
        //
        // Der bestehende Fall prueft, dass das SIGNAL ausgeht. Das ist
        // die halbe Kette. Hier wird das andere Ende gemessen: die
        // Frequenz der Scheibe — das, was der Bediener sieht und hoert.
        RadioModel* model = m_mw->findChild<RadioModel*>();
        if (!model) { QSKIP("Kein RadioModel"); }
        if (model->slices().isEmpty()) {
            model->addSlice();
            for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        }
        if (model->slices().isEmpty()) { QSKIP("Keine Scheibe"); }
        SliceModel* s = model->slices().first();

        m_sw->setStepSize(100);
        s->setFrequency(7'100'000.0);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        QTest::keyClick(m_sw, Qt::Key_Right);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QCOMPARE(s->frequency(), 7'100'100.0);

        QTest::keyClick(m_sw, Qt::Key_Left);
        QTest::keyClick(m_sw, Qt::Key_Left);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QCOMPARE(s->frequency(), 7'099'900.0);

        // Mit Umschalt zehnfach — damit man ueber ein Band kommt, ohne
        // die Taste festzuhalten.
        QTest::keyClick(m_sw, Qt::Key_Right, Qt::ShiftModifier);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QCOMPARE(s->frequency(), 7'100'900.0);
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
