// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_real_pan_float.cpp  (Longpath)
// =================================================================
// Der Panadapter, abgeloest im ECHTEN Hauptfenster.
//
// Der Betreiber, 2026-08-20, auf die Frage welches Fenster: „alle,
// mache eine nach der anderen, testen wir mal den padapter" — und was
// passiert: „Es geht bildschirmfuellend auf".
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QScreen>
#include "gui/MainWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/WindowChrome.h"
#include <QPushButton>

using namespace Longpath;

class TestRealPanFloat : public QObject
{
    Q_OBJECT
private slots:
    void floatingThePanDoesNotFillTheScreen()
    {
        auto* mwp = new MainWindow();      // bewusst nicht abgeraeumt
        mwp->resize(1280, 800);
        // Genau die Lage des Betreibers nachstellen: sein
        // MainWindowGeometry sagt Rahmen (0,33)-(1469,955), also
        // bildschirmfuellend, und der Vollbild-Vermerk ist gesetzt.
        // Auf macOS oeffnet ein neues Fenster, waehrend das
        // Elternfenster im Vollbild steht, in DERSELBEN Vollbildflaeche
        // — und wirkt dort bildschirmfuellend, egal welche Groesse
        // Qt meldet.
        mwp->showFullScreen();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(400);

        auto* stack = mwp->findChild<PanadapterStack*>();
        QVERIFY2(stack, "es muss einen Panadapter-Stapel geben");

        auto* pan = mwp->findChild<PanadapterApplet*>();
        QVERIFY2(pan, "und mindestens einen Panadapter");
        qDebug() << "Panadapter minimumSizeHint:" << pan->minimumSizeHint()
                 << "minimumSize:" << pan->minimumSize()
                 << "sizeHint:" << pan->sizeHint();

        stack->floatPanadapter(pan->panId());
        QTest::qWait(500);

        auto* win = mwp->findChild<PanFloatingWindow*>();
        if (!win) {
            const QList<QWidget*> tops = QApplication::topLevelWidgets();
            for (QWidget* w : tops) {
                if (auto* p = qobject_cast<PanFloatingWindow*>(w)) { win = p; }
            }
        }
        QVERIFY2(win, "das abgeloeste Panadapterfenster muss existieren");

        qDebug() << "Fenster:" << win->size()
                 << "minimumSizeHint:" << win->minimumSizeHint();

        const QSize avail = win->screen() ? win->screen()->availableSize()
                                          : QSize(1440, 900);
        QVERIFY2(win->height() <= (avail.height() * 3) / 4,
                 qPrintable(QStringLiteral("zu hoch: %1 von %2")
                     .arg(win->height()).arg(avail.height())));
        QVERIFY2(win->width() <= (avail.width() * 3) / 4,
                 qPrintable(QStringLiteral("zu breit: %1 von %2")
                     .arg(win->width()).arg(avail.width())));
        QVERIFY2(win->minimumSizeHint().height() <= 300,
                 qPrintable(QStringLiteral(
                     "die Untergrenze ist zu hoch (%1) — dann laesst sich "
                     "das Fenster nicht kleiner ziehen")
                     .arg(win->minimumSizeHint().height())));
        QVERIFY2(win->minimumSizeHint().width() <= 500,
                 qPrintable(QStringLiteral("Untergrenze zu breit: %1")
                     .arg(win->minimumSizeHint().width())));
    }

    // ── Das Schloss ─────────────────────────────────────────────────
    //
    // „das schloss fehlt dann zum fixieren" (2026-08-20). Ein
    // festgestelltes Fenster bleibt, wo es steht — und ein Schloss,
    // das man nur sieht, aber das nichts haelt, waere schlimmer als
    // keines.
    void lockingTheWindowStopsMovingAndResizing()
    {
        auto* mwp = new MainWindow();
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(400);

        auto* stack = mwp->findChild<PanadapterStack*>();
        auto* pan   = mwp->findChild<PanadapterApplet*>();
        QVERIFY(stack && pan);
        stack->floatPanadapter(pan->panId());
        QTest::qWait(500);

        PanFloatingWindow* win = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<PanFloatingWindow*>(w)) { win = p; }
        }
        QVERIFY(win);

        auto* bar = win->findChild<WindowTitleBar*>();
        QVERIFY2(bar, "eigene Leiste");
        auto* grip = win->findChild<ResizeGrip*>();
        QVERIFY2(grip, "Anfasser unten rechts");

        // Ein Schloss muss in der Leiste ANKLICKBAR sein, nicht nur im
        // Quelltext stehen — die Lehre aus dem Ablöseknopf.
        QPushButton* lock = nullptr;
        for (QPushButton* b : bar->findChildren<QPushButton*>()) {
            if (b->isCheckable()) { lock = b; break; }
        }
        QVERIFY2(lock, "in der Leiste MUSS ein Schloss stehen");
        QVERIFY2(lock->isVisible(), "und zu sehen sein");

        win->move(300, 300);
        const QPoint before = win->pos();

        QTest::mouseClick(lock, Qt::LeftButton);
        QVERIFY2(bar->isLocked(), "der Klick muss feststellen");
        QVERIFY2(win->property("longpathWindowLocked").toBool(),
                 "und das FENSTER markieren — daran haengt der Resizer");

        // Ziehen an der Leiste: darf nichts bewegen.
        auto sendTo = [](QWidget* t, QEvent::Type ty, QPoint g,
                         Qt::MouseButtons btns) {
            QMouseEvent ev(ty, t->mapFromGlobal(g), g, Qt::LeftButton,
                           btns, Qt::NoModifier);
            QApplication::sendEvent(t, &ev);
        };
        const QPoint press(600, 320);
        sendTo(bar, QEvent::MouseButtonPress, press, Qt::LeftButton);
        sendTo(bar, QEvent::MouseMove, press + QPoint(120, 90), Qt::LeftButton);
        sendTo(bar, QEvent::MouseButtonRelease, press + QPoint(120, 90),
               Qt::NoButton);
        QCOMPARE(win->pos(), before);

        // Und der Anfasser: darf nichts vergroessern.
        const QSize sizeBefore = win->size();
        const QPoint gp = grip->mapToGlobal(grip->rect().center());
        sendTo(grip, QEvent::MouseButtonPress, gp, Qt::LeftButton);
        sendTo(grip, QEvent::MouseMove, gp + QPoint(80, 60), Qt::LeftButton);
        sendTo(grip, QEvent::MouseButtonRelease, gp + QPoint(80, 60),
               Qt::NoButton);
        QCOMPARE(win->size(), sizeBefore);

        // Wieder loesen: alles geht wieder.
        QTest::mouseClick(lock, Qt::LeftButton);
        QVERIFY2(!bar->isLocked(), "der zweite Klick muss loesen");
        sendTo(bar, QEvent::MouseButtonPress, press, Qt::LeftButton);
        sendTo(bar, QEvent::MouseMove, press + QPoint(-70, -40), Qt::LeftButton);
        sendTo(bar, QEvent::MouseButtonRelease, press + QPoint(-70, -40),
               Qt::NoButton);
        QCOMPARE(win->pos(), before + QPoint(-70, -40));
    }
};
QTEST_MAIN(TestRealPanFloat)
#include "tst_real_pan_float.moc"
