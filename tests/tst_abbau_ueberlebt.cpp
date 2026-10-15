// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Fenster muss sich auch wieder abbauen lassen.
//
// Am 2026-10-15 sind binnen zwei Stunden ZWEI Abstuerze derselben Art
// aufgefallen, beide beim Beenden, beide mit demselben Bild im
// Absturzbericht:
//
//   ~MainWindow -> ~ConnectionPanel -> destroyed()
//     -> restoreFloatingWindowsHiddenBehindConnectMask()
//     -> QPointer<QWidget>::operator bool()     EXC_BAD_ACCESS @ 0x5
//
//   ~MainWindow -> ~StatusToast -> destroyed()
//     -> QPointer<StatusToast>::isNull()        EXC_BAD_ACCESS @ 0x5
//
// Die Falle ist immer dieselbe: ein Kind des Fensters meldet sein
// Ende, und der angeschlossene Rumpf greift in das Fenster — dessen
// Destruktor aber schon durch ist. Qt loest die Verbindungen erst in
// ~QObject, also NACH ~QWidget, das die Kinder abraeumt.
//
// Vier solche Stellen gibt es in MainWindow: Verbindungsmaske,
// Hinweiszettel, Unterstuetzungsfenster und die FFT-Maschinen. Alle
// vier fragen jetzt m_shuttingDown. Dieser Lauf oeffnet, was sich
// oeffnen laesst, und baut das Fenster danach wirklich ab — der
// Exitcode IST die Zusicherung.
//
// Gegenprobe (2026-10-15): ohne die Wachen endet derselbe Lauf mit
// SIGSEGV (Exitcode 139).

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "gui/MainWindow.h"

using namespace Longpath;

class TstAbbauUeberlebt : public QObject
{
    Q_OBJECT

private:
    static QAction* suche(MainWindow* mw, const QString& wortteil)
    {
        for (QMenuBar* bar : mw->findChildren<QMenuBar*>()) {
            for (QAction* a : bar->actions()) {
                if (!a->menu()) { continue; }
                for (QAction* e : a->menu()->actions()) {
                    if (e->text().contains(wortteil)) { return e; }
                    if (!e->menu()) { continue; }
                    for (QAction* f : e->menu()->actions()) {
                        if (f->text().contains(wortteil)) { return f; }
                    }
                }
            }
        }
        return nullptr;
    }

private slots:
    void mitOffenenFensternUndEinemHinweis()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        // Die Verbindungsmaske: sie steht beim ersten Start ohnehin,
        // aber der Aufruf macht es unabhaengig davon.
        QVERIFY(QMetaObject::invokeMethod(mw, "showConnectionPanel",
                                          Qt::DirectConnection));
        for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); }

        // Das Unterstuetzungsfenster.
        QVERIFY(QMetaObject::invokeMethod(mw, "showSupportDialog",
                                          Qt::DirectConnection));
        for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); }

        // Und ein Hinweiszettel, der beim Abbau noch steht: der
        // Feldbetrieb-Eintrag meldet sich mit einem.
        if (QAction* feld = suche(mw, QStringLiteral("Field layout"))) {
            feld->trigger();
            for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); }
        }

        qInfo() << "Fenster offen, Hinweis steht — jetzt abbauen";

        // DAS ist die Pruefung. Stuerzt es hier, meldet der Lauf
        // SIGSEGV, und kein QVERIFY der Welt kommt mehr dazu.
        delete mw;

        QVERIFY2(true, "bis hierher gekommen");
    }
};

QTEST_MAIN(TstAbbauUeberlebt)
#include "tst_abbau_ueberlebt.moc"
