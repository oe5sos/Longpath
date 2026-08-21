// SPDX-License-Identifier: GPL-3.0-or-later
//
// Nach dem Schliessen darf kein nachgereichtes Loeschen mehr offen
// sein.
//
// Aus dem Absturzbericht vom 2026-08-21, 06:47 (und 06:29, gleicher
// Ablauf). SIGSEGV im Hauptfaden, von unten nach oben gelesen:
//
//   NSApplication terminate → exit → __cxa_finalize
//   → QThreadDataDestroyer::EarlyMainThread::~EarlyMainThread
//   → sendPostedEvents → AppletFloatingWindow::~AppletFloatingWindow
//   → WindowTitleBar::~WindowTitleBar → QWidget::destroy
//   → QWindow::~QWindow → QSurface::~QSurface
//   → QOpenGLContext::currentContext() → QThreadStorageData::get()
//
// Ein deleteLater auf ein schwebendes Fenster wurde nicht mehr
// zugestellt, solange das Programm lief. Zugestellt wurde es erst beim
// Abbau der Faden-Daten — da war der Speicher, den der
// QWindow-Destruktor ueber den OpenGL-Kontext anfasst, schon weg.
//
// Der Test prueft nicht den Absturz (den kann er nicht ausloesen),
// sondern die Bedingung, unter der er entsteht: ein Fenster, das das
// Schliessen ueberlebt.

#include <QtTest>
#include <QDialog>
#include <QPointer>

#include "gui/MainWindow.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletWidget.h"
#include "gui/WindowChrome.h"

using namespace Longpath;

class TstQuitLeavesNoPendingDeletes : public QObject
{
    Q_OBJECT

private slots:
    void aFloatingAppletIsGoneWhenTheWindowCloses()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        // Ein Applet herausloesen — derselbe Weg wie ueber den Pfeil
        // in der Zellenleiste.
        QMetaObject::invokeMethod(mw, "detachRotorPanel");
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        // Nur unsere eigenen Fenster mit Leiste und Anfasser. Genau
        // die tragen ein eigenes Fenster (WA_NativeWindow, damit die
        // Leiste ueber dem Spektrum steht), und genau deren
        // Destruktor stand im Absturzbericht.
        //
        // Beim ersten Lauf fiel hier auch ein VaxFirstRunDialog auf,
        // der das Schliessen ueberlebt. Das ist ein eigener Fall
        // (Erstlauf-Dialog ohne unsere Leiste) und gehoert nicht in
        // diesen Test — festgehalten, damit er nicht verlorengeht.
        QList<QPointer<QObject>> floats;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w == mw || !w->isWindow()) { continue; }
            if (w->findChild<WindowTitleBar*>()) { floats.append(w); }
        }
        QVERIFY2(!floats.isEmpty(),
                 "Kein schwebendes Fenster entstanden — der Test prueft "
                 "sonst nichts");

        mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        // Entscheidend: KEIN sendPostedEvents hier. Genau das fehlt
        // beim Beenden, und genau deshalb muss das Schliessen selbst
        // aufgeraeumt haben.
        for (const QPointer<QObject>& w : floats) {
            QVERIFY2(w.isNull(),
                     qPrintable(QStringLiteral(
                         "'%1' hat das Schliessen ueberlebt — sein "
                         "Loeschen faellt in den Programmabbau und "
                         "stuerzt dort ab")
                         .arg(w ? QString::fromLatin1(
                                      w->metaObject()->className())
                                : QString{})));
        }

        delete mw;
    }

    /// Und kein Dialog ueberlebt das Schliessen.
    ///
    /// Beim ersten Bau dieses Tests (2026-08-21) fiel ein
    /// VaxFirstRunDialog auf, der das Schliessen ueberlebt. Er wurde
    /// damals als eigener Fall notiert und bewusst nicht mitgeprueft —
    /// hier ist er.
    ///
    /// Ein Dialog haengt zwar als Kind am Hauptfenster und traegt
    /// WA_DeleteOnClose, aber close() auf das Hauptfenster schliesst
    /// seine Dialoge NICHT mit. Sie bleiben offen, und ihr Loeschen
    /// faellt in den Programmabbau — dieselbe Familie wie der Absturz,
    /// den dieser Test bewacht.
    void noDialogSurvivesTheClose()
    {
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        // Einen modelosen Dialog aufmachen, wie es das Programm tut.
        auto* dlg = new QDialog(mw);
        dlg->setObjectName(QStringLiteral("probeDialog"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        QPointer<QObject> alive(dlg);
        QVERIFY(dlg->isVisible());

        mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        QVERIFY2(alive.isNull() || !static_cast<QWidget*>(alive.data())->isVisible(),
                 "Ein Dialog steht noch offen, nachdem das Hauptfenster zu "
                 "ist — sein Loeschen faellt in den Programmabbau");

        delete mw;
    }
};

QTEST_MAIN(TstQuitLeavesNoPendingDeletes)
#include "tst_quit_leaves_no_pending_deletes.moc"
