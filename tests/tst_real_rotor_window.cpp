// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_real_rotor_window.cpp  (Longpath)
// =================================================================
// Der Rotor als Fenster wie alle anderen.
//
// Der Betreiber, 2026-08-20: „rotor noch immer in keinem window
// welches man wie alle andern verschieben und vergroessern kann."
//
// Er lag nackt im Splitter unter dem Panadapter — keine Anfassmarke,
// kein Ablöseknopf, kein Schloss, kein Anfasser. Alles andere im
// Programm trug diese Leiste inzwischen; dies war das letzte Feld
// ohne.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QPushButton>
#include <QScreen>
#include "gui/MainWindow.h"
#include "gui/ToolWindow.h"
#include "gui/WindowChrome.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

class TestRealRotorWindow : public QObject
{
    Q_OBJECT
private slots:
    void theRotorGetsARealWindowLikeEverythingElse()
    {
        auto* mwp = new MainWindow();      // bewusst nicht abgeraeumt
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(500);

        // Unter den Panadapter holen — der Ort, an dem der Betreiber
        // ihn hat.
        QMetaObject::invokeMethod(mwp, "setRotorPanelBelow",
                                  Q_ARG(bool, true));
        QTest::qWait(300);

        auto* panel = mwp->findChild<RotorLogbookPanel*>();
        QVERIFY2(panel, "es muss ein Rotor-Panel geben");

        // 1. Eine Kopfleiste MUSS da sein — sonst gibt es nichts
        //    anzufassen.
        WindowTitleBar* header = nullptr;
        for (WindowTitleBar* b : mwp->findChildren<WindowTitleBar*>()) {
            if (b->isVisible()) { header = b; break; }
        }
        QVERIFY2(header,
                 "unter dem Panadapter MUSS eine Kopfleiste stehen — bis "
                 "2026-08-20 lag das Panel dort nackt im Splitter");

        // 2. Ablösen ergibt ein echtes, rahmenloses Werkzeugfenster.
        QMetaObject::invokeMethod(mwp, "detachRotorPanel");
        QTest::qWait(400);

        ToolWindow* win = mwp->findChild<ToolWindow*>();
        QVERIFY2(win, "das Abloesen MUSS ein Fenster ergeben");
        QVERIFY2(win->isVisible(), "und es muss zu sehen sein");
        QVERIFY2(win->windowFlags() & Qt::FramelessWindowHint,
                 "rahmenlos, sonst stehen zwei Leisten uebereinander");
        QVERIFY2(win->windowFlags() & Qt::Tool,
                 "Werkzeugfenster — sonst zieht es auf macOS in die "
                 "Vollbildflaeche des Hauptfensters ein");
        QVERIFY2(win->content() == static_cast<QWidget*>(panel),
                 "und es MUSS dasselbe Panel tragen, keine Kopie: zwei "
                 "Logbuecher mit eigenem Zustand faellt erst auf, wenn "
                 "zwei Eintraege auseinanderlaufen");

        // 3. Leiste, Schloss und Anfasser wie ueberall.
        auto* bar = win->findChild<WindowTitleBar*>();
        QVERIFY2(bar, "eigene Leiste zum Ziehen");
        QVERIFY2(win->findChild<ResizeGrip*>() != nullptr,
                 "Anfasser unten rechts");
        QPushButton* lock = nullptr;
        for (QPushButton* b : bar->findChildren<QPushButton*>()) {
            if (b->isCheckable()) { lock = b; break; }
        }
        QVERIFY2(lock && lock->isVisible(), "Schloss in der Leiste");

        // 4. Nicht bildschirmfuellend.
        const QSize avail = win->screen() ? win->screen()->availableSize()
                                          : QSize(1440, 900);
        QVERIFY2(win->height() <= (avail.height() * 4) / 5 + 4,
                 qPrintable(QStringLiteral("zu hoch: %1").arg(win->height())));

        // 5. Ziehen an der Leiste bewegt es — nach links UND oben.
        win->move(400, 300);
        const QPoint before = win->pos();
        auto send = [](QWidget* t, QEvent::Type ty, QPoint g,
                       Qt::MouseButtons btns) {
            QMouseEvent ev(ty, t->mapFromGlobal(g), g, Qt::LeftButton,
                           btns, Qt::NoModifier);
            QApplication::sendEvent(t, &ev);
        };
        const QPoint press(700, 320);
        send(bar, QEvent::MouseButtonPress, press, Qt::LeftButton);
        send(bar, QEvent::MouseMove, press + QPoint(-90, -60), Qt::LeftButton);
        send(bar, QEvent::MouseButtonRelease, press + QPoint(-90, -60),
             Qt::NoButton);
        QCOMPARE(win->pos(), before + QPoint(-90, -60));

        // 6. Andocken gibt das Panel LEBEND zurueck.
        QPointer<RotorLogbookPanel> alive(panel);
        QMetaObject::invokeMethod(mwp, "dockRotorPanel");
        QTest::qWait(400);
        QVERIFY2(!alive.isNull(),
                 "das Panel muss das Andocken ueberleben");
    }
};
QTEST_MAIN(TestRealRotorWindow)
#include "tst_real_rotor_window.moc"
