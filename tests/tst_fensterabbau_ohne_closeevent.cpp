// no-port-check: Longpath-eigener Pruefstand, nichts portiert.
// =================================================================
// tests/tst_fensterabbau_ohne_closeevent.cpp  (Longpath)
// =================================================================
//
// Ein MainWindow, das OHNE closeEvent() zerstoert wird, muss das
// ueberleben.
//
// WARUM ES DAS GIBT. Die Abbau-Fahne m_shuttingDown wird an drei
// Stellen gesetzt -- in closeEvent(), im Signal-Weg (SIGTERM) und beim
// Beenden --, aber bis zum 2026-10-09 NICHT im Destruktor. Wer ein
// MainWindow direkt loescht (Pruefstand, Ausnahmepfad), lief damit mit
// falscher Fahne durch den ganzen Abbau, und alle 26 Waechter, die auf
// sie hoeren, griffen ins Leere.
//
// Sichtbar wurde es als Absturz, der aussah wie ein fremdes Problem:
//
//   MainWindow::restoreFloatingWindowsHiddenBehindConnectMask()
//     <- doActivate <- QObject::destroyed(QObject*)
//     <- QWidget::~QWidget <- ConnectionPanel::~ConnectionPanel
//     <- QObjectPrivate::deleteChildren <- QWidget::~QWidget
//     <- MainWindow::~MainWindow
//
// ~MainWindow baut seine Kinder ab, das ConnectionPanel meldet
// destroyed(), der Handler ruft
// restoreFloatingWindowsHiddenBehindConnectMask(), deren eigener
// Waechter laesst durch (Fahne stand ja nicht), und der Rumpf greift in
// ein Fenster, dessen Mitglieder schon weg sind. SIGSEGV, in zwei von
// vierzehn Laeufen.
//
// Gefunden nicht als Fehler gesucht, sondern beim Bauen eines ANDEREN
// Pruefstands (tst_spe_anbindung), der zufaellig die ConnectMaske
// geoeffnet hatte und das Fenster am Ende loeschte.
//
// WAS ER BELEGT -- UND WAS NICHT. Ehrlich gesagt: er ist ein
// Rauchmelder, kein Tor.
//
// Der Absturz ist nicht abgesprochen, sondern unbestimmtes Verhalten:
// `m_floatingContainersHiddenPreConnect` ist ein Mitglied und damit
// beim Rueckruf laengst abgebaut (Mitglieder sterben in Schritt 2 des
// Abbaus, die Kinder erst in Schritt 3). Der Rumpf laeuft also ueber
// eine zerstoerte Liste. OB das knallt, haengt daran, wie der
// freigegebene Speicher gerade aussieht -- sieht er leer aus, laeuft
// die Schleife nullmal durch und nichts passiert. Darum zwei von
// vierzehn und nicht vierzehn von vierzehn.
//
// Dieser Fall laesst sich entsprechend NICHT so bauen, dass er ohne die
// Behebung zuverlaessig faellt: 14 Laeufe ohne Fahne gaben hier 14 mal
// gruen. Belegt ist die Behebung durch anderes --
//
//   * das entschluesselte Absturzbild oben (aus
//     ~/Library/Logs/DiagnosticReports), das den Weg Zeile fuer Zeile
//     nennt, und
//   * die Absturzrate von tst_spe_anbindung: 2 von 14 vorher,
//     0 von 20 nachher.
//
// Deterministisch faellt es nur im Debug-Bau mit AddressSanitizer --
// dort ist der Zugriff auf die zerstoerte Liste ein
// heap-use-after-free und bricht immer ab. Wer diesen Fall schaerfen
// will, fuehrt ihn dort; der normale Bau kann das nicht.
//
// Was dieser Fall hier dennoch tut: er faehrt den Weg ab (Maske
// oeffnen, Fenster ohne closeEvent loeschen) und schlaegt an, wenn der
// Abbau wieder knallt -- ein Absturz beendet das Programm, und ctest
// meldet den Fall rot. Und er schreibt den Weg auf, damit der naechste
// nicht wieder eine Stunde im falschen Modul sucht.

// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu. Martin Fischer, AI-assisted via Anthropic
//                 Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <QPointer>

#include "gui/ConnectionPanel.h"
#include "gui/MainWindow.h"

using namespace Longpath;

class TstFensterabbauOhneCloseEvent : public QObject
{
    Q_OBJECT

private slots:
    void dasFensterUeberlebtSeinenEigenenAbbau();
};

void TstFensterabbauOhneCloseEvent::dasFensterUeberlebtSeinenEigenenAbbau()
{
    auto* mw = new MainWindow();
    mw->resize(1200, 800);
    mw->show();
    QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

    // Die ConnectMaske muss da sein -- sie ist das Kind, dessen
    // destroyed() den Weg oeffnet. Ohne sie prueft der Fall nichts.
    // showConnectionPanel() ist ein privater Slot -- ueber das
    // Meta-Objekt gerufen, damit der Pruefstand keine Sichtbarkeit
    // aufbohren muss, die der Anwendung nichts bringt.
    QVERIFY2(QMetaObject::invokeMethod(mw, "showConnectionPanel"),
             "showConnectionPanel() liess sich nicht rufen -- umbenannt?");
    QTest::qWait(100);
    ConnectionPanel* maske = mw->findChild<ConnectionPanel*>();
    QVERIFY2(maske, "Keine ConnectMaske -- dann laeuft der geprüfte Weg nicht");
    QPointer<ConnectionPanel> wache(maske);

    // Und zwar GEOEFFNET: der Handler kehrt frueh zurueck, solange die
    // Maske sichtbar ist. Beim Abbau ist sie das nicht mehr, genau
    // darum greift er dann.
    QVERIFY2(maske->isVisible(), "Die Maske ist nicht offen");

    // Kein close(), kein closeEvent -- direkt loeschen. Das ist der Weg,
    // auf dem die Abbau-Fahne nie gesetzt wurde.
    delete mw;

    // Erreicht werden ist die Aussage. Waere die Fahne nicht gesetzt,
    // waere das Programm im Abbau mit SIGSEGV gestorben und diese Zeile
    // nie gelaufen.
    QVERIFY2(wache.isNull(),
             "Die Maske lebt noch -- dann wurde nicht wirklich abgebaut");
}

QTEST_MAIN(TstFensterabbauOhneCloseEvent)
#include "tst_fensterabbau_ohne_closeevent.moc"
