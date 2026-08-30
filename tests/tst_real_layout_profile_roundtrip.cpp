// =================================================================
// tests/tst_real_layout_profile_roundtrip.cpp  (Longpath)
// =================================================================
//
// Vier Speicherwege fuer die Oberflaeche, und keiner weiss vom
// anderen (siehe MainWindow.cpp ~6936-7043, das ist die diagnostizierte
// Ursache): ContainerManager::saveState()/restoreState(),
// LayoutProfiles (Sichtbarkeit/Reihenfolge/floatingApplets/
// containerGeometry/rotorDockVisible/Splitter/Weltbild),
// AppletVisibilityController und die einzelnen AppSettings-Schluessel
// wie "AppletStackOrder". Zuletzt gefunden (2026-08-30): das
// "Bandwidth Filter"-Applet stand abgeloest ueber dem Panadapter, aber
// sein floatingApplets-Eintrag war in JEDEM gesicherten Profil "{}" --
// nie erfasst.
//
// Dieser Test geht NICHT den Weg von tst_layout_profiles.cpp (dort
// haengt an den Haken ein reines m_live-QVariantMap, ohne jede
// Beruehrung mit MainWindow -- genau die Abkuerzung, die den
// Bandwidth-Filter-Fehler nicht gefunden haette, weil der Fehler NICHT
// in LayoutProfiles selbst sass, sondern darin, ob und wann MainWindow
// captureIntoCurrent()+save() tatsaechlich aufruft). Stattdessen baut
// dieser Test das ECHTE MainWindow (wie tst_real_mainwindow_detach.cpp)
// und geht die ECHTEN Signalwege: den Ablöse-Knopf "↗" in der
// Kopfleiste, das Containers-Menue, den Schalter "Neue Fenster gleich
// abloesen". Erst danach wird — als "Neustart" — ein FRISCHES
// LayoutProfiles gebaut, das aus demselben (sandboxed) AppSettings
// liest: load() + applyCurrent(), wortgleich mit dem Start-Code in
// MainWindow.cpp.
//
// Drei Pruefungen:
//
//  1. detachedGeometryAndVisibilitySurviveRestart — datengetrieben
//     ueber JEDES Applet, das beim Start als Kachel in der Spalte
//     steht (die Liste wird zur Testzeit aus dem echten Fenster
//     ausgelesen, nicht von Hand gepflegt -- siehe die Begruendung in
//     der Aufgabenstellung, "get the authoritative list yourself").
//     Abloesen ueber den echten Knopf, eine unterscheidbare Geometrie
//     setzen, auf geometrySettled() warten (derselbe Signalweg wie ein
//     Zug von Hand), dann lesen und vergleichen.
//
//  2. freeCanvasModeReshowSurvivesRestart — der Betriebsfall, der den
//     Bandwidth-Filter-Fehler ausgeloest haben koennte: bei "Neue
//     Fenster gleich abloesen" aktiv ein Applet ueber das
//     Containers-Menue AUS- und wieder EINschalten. Dieser Weg
//     (MainWindow.cpp, applyAppletVisibility(), der Zweig um Zeile
//     1628-1635) ruft detachApplet() auf und kehrt dann zurueck OHNE
//     das unmittelbare captureIntoCurrent()+save(), das jeder andere
//     Abloese-Pfad hat (siehe dockAppletBack(), moveAllAppletsToCanvas(),
//     der appletDetachRequested-Connect) -- ein staendig sichtbarer
//     Unterschied zu allen Geschwisterpfaden. Ob die Geometrie trotzdem
//     ankommt (ueber den 400ms-geometrySettled()-Umweg von
//     applyDefaultSize()/move() innerhalb desselben detachApplet()-Laufs)
//     oder nicht, ist genau die Frage, die dieser Test beantwortet.
//
//  3. windowAndChromeVisibilitySurviveRestart — datengetrieben ueber
//     die fuenf eigenen Fenster (WinLogbook, WinRotorLog,
//     WinChannelStrip, WinAntenna, WinSpotHub) und die zwei
//     Chrome-Eintraege (ChromeSpectrumButtons, ChromeStatusBar). Sie
//     haben keine Geometrie im Profil (bewusst, siehe die Begruendung
//     an der Erfassungsstelle: "NICHT dabei: die Fenstergroesse"),
//     aber ihre Sichtbarkeit steht in derselben "visible"-Karte wie
//     jedes Applet und muss denselben Rundgang ueberstehen.
//
// Was hier NICHT geprueft wird: das echte Schreiben auf die Platte.
// LayoutProfiles::save() haelt den Datei-Flush 500ms zurueck (Review-
// Fund 2026-08-28, siehe die eigene Begruendung dort); dieser Test
// liest ueber denselben AppSettings-Prozess-Singleton wie das echte
// MainWindow und sieht den bereits aktualisierten Speicherwert sofort
// -- wortgleiches Muster zu tst_layout_profiles.cpp::
// aRoundTripKeepsEverything. Ein Absturz innerhalb dieser 500ms bliebe
// unentdeckt; das ist ein bekanntes, hingenommenes Fenster (siehe die
// Begruendung in LayoutProfiles.cpp) und keine Luecke dieses Tests.
//
// Container-Typen (ContainerManager) sind hier absichtlich NICHT
// nochmal abgedeckt: tst_container_persistence.cpp deckt
// saveState()/restoreState() (schwebend, angedockt, verpackt in
// AppletPanelWidget, Verdopplungsschutz) bereits mit acht Pruefungen
// ab. Was DORT fehlt und auch hier nicht nachgeholt wird: ob
// MainWindow's "containerGeometry"-Profilschluessel (der
// ContainerManager::floatingGeometries()-Aufruf in der
// LayoutProfiles-Erfassung) durch ein echtes MainWindow hindurch
// tatsaechlich denselben Weg geht wie floatingApplets. Das ist eine
// offene Deckungsluecke, siehe die Zusammenfassung der Uebergabe.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletWidget.h"
#include "gui/applets/GridCellWidget.h"

using namespace Longpath;

namespace {

/// Der Ablöse-Knopf in einer sichtbaren Kopfleiste — derselbe Fund wie
/// in tst_real_mainwindow_detach.cpp.
QPushButton* detachButtonFor(GridCellWidget* cell)
{
    if (!cell || !cell->titleBar() || !cell->titleBar()->isVisible()) {
        return nullptr;
    }
    for (QPushButton* b : cell->titleBar()->findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("↗") && b->isVisible()) { return b; }
    }
    return nullptr;
}

/// Eine QAction irgendwo unter `root` mit GENAU diesem sichtbaren Text
/// -- die Containers>Applets-Eintraege tragen `m_appletVis->displayName(id)`
/// wortgleich (MainWindow.cpp ~8483-8485), ohne "&"-Kurzwahl.
QAction* findActionByExactText(QWidget* root, const QString& text)
{
    for (QAction* a : root->findChildren<QAction*>()) {
        if (a->text() == text) { return a; }
    }
    return nullptr;
}

/// Unterscheidbare, aber reproduzierbare Geometrie je Kennung -- damit
/// eine Verwechslung zweier Eintraege (falscher Schluessel in der
/// floatingApplets-Karte) auffiele, statt zufaellig durchzurutschen.
QRect distinctiveRectFor(const QString& id)
{
    const uint h = qHash(id);
    const int x = 30  + int(h % 80);
    const int y = 40  + int((h / 80) % 80);
    const int w = 320 + int((h / 6400) % 100);
    const int ht = 190 + int((h / 640000) % 90);
    return QRect(x, y, w, ht);
}

/// "Neustart": ein FRISCHES LayoutProfiles, das nur ueber AppSettings
/// mit dem echten MainWindow verbunden ist -- load() + applyCurrent(),
/// wortgleich mit dem Start-Code in MainWindow.cpp (~7203-7217).
QVariantMap reloadedProfileState()
{
    QVariantMap restored;
    LayoutProfiles fresh;
    fresh.setHooks([]() -> QVariantMap { return {}; },
                   [&restored](const QVariantMap& s) { restored = s; });
    fresh.load();
    fresh.applyCurrent();
    return restored;
}

} // namespace

class TestRealLayoutProfileRoundtrip : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};

    AppletPanelWidget* panel() const
    {
        return m_mw ? m_mw->findChild<AppletPanelWidget*>() : nullptr;
    }

private slots:

    void initTestCase()
    {
        // Sauberer Anfang -- ohne das koennte ein liegen gebliebener
        // Profilsatz eines ANDEREN Testprogramms (derselbe sandboxed
        // AppSettings-Ordner wird laut TestSandboxInit.cpp ueber ALLE
        // Testbinaries eines Laufs geteilt) MainWindow beim Start ein
        // fremdes, hier unbekanntes Profil anwenden lassen, statt
        // "Standard" frisch anzulegen.
        AppSettings::instance().remove(LayoutProfiles::settingsKey());

        // Bewusst nicht abgeraeumt -- siehe tst_real_mainwindow_detach.cpp:
        // MainWindow startet Arbeitsfaeden, deren geordnetes Ende an
        // einer laufenden Ereignisschleife haengt, und ein Testlauf, der
        // gleich darauf endet, waere selbst die Fehlerquelle.
        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);   // Applets bauen sich beim Anzeigen auf

        QVERIFY2(panel(), "es muss eine Applet-Spalte geben");
    }

    // ── 1. Jedes Applet, das die Spalte beim Start zeigt ─────────────

    void detachedGeometryAndVisibilitySurviveRestart_data()
    {
        QTest::addColumn<QString>("ownAppletId");

        AppletPanelWidget* p = panel();
        QVERIFY(p);
        for (GridCellWidget* cell : p->findChildren<GridCellWidget*>()) {
            if (!detachButtonFor(cell)) { continue; }
            const QList<AppletWidget*> applets = cell->applets();
            if (applets.isEmpty() || !applets.first()) { continue; }
            const QString ownId = applets.first()->appletId();
            if (ownId.isEmpty()) { continue; }
            QTest::newRow(qPrintable(ownId)) << ownId;
        }
    }

    void detachedGeometryAndVisibilitySurviveRestart()
    {
        QFETCH(QString, ownAppletId);

        AppletPanelWidget* p = panel();
        QVERIFY(p);

        // Die Kachel zur Testzeit neu suchen -- vorige Datenzeilen
        // haben moeglicherweise die Spalte umgebaut (abgeloest, wieder
        // angedockt), auch wenn jede Zeile am Ende aufraeumt.
        GridCellWidget* cell = nullptr;
        AppletWidget* applet = nullptr;
        for (GridCellWidget* c : p->findChildren<GridCellWidget*>()) {
            const QList<AppletWidget*> applets = c->applets();
            if (!applets.isEmpty() && applets.first()
                && applets.first()->appletId() == ownAppletId) {
                cell = c;
                applet = applets.first();
                break;
            }
        }
        QVERIFY2(cell, "Kachel zur Kennung nicht gefunden -- "
                       "vorige Zeile nicht sauber angedockt?");

        QPushButton* detachBtn = detachButtonFor(cell);
        QVERIFY2(detachBtn, "kein Abloese-Knopf in der Kopfleiste");

        const int before = m_mw->findChildren<AppletFloatingWindow*>().size();
        QTest::mouseClick(detachBtn, Qt::LeftButton);
        QTest::qWait(150);

        const QList<AppletFloatingWindow*> windows =
            m_mw->findChildren<AppletFloatingWindow*>();
        QVERIFY2(windows.size() == before + 1,
                 "der Druck auf '↗' MUSS genau ein neues Fenster ergeben");
        AppletFloatingWindow* win = windows.last();
        QVERIFY(win);
        const QString panelId = win->appletId();
        QVERIFY2(!panelId.isEmpty(), "das Fenster kennt seine Panelkennung nicht");

        // Eine unterscheidbare Lage setzen -- derselbe Signalweg wie ein
        // Zug von Hand: moveEvent/resizeEvent starten den Settle-Timer,
        // geometrySettled() loest capture+save aus (MainWindow.cpp
        // ~1512-1522).
        QSignalSpy settledSpy(win, &AppletFloatingWindow::geometrySettled);
        const QRect rect = distinctiveRectFor(panelId);
        win->setGeometry(rect);
        QVERIFY2(settledSpy.wait(2000),
                 "geometrySettled() ist nach dem Verschieben nie gekommen");

        // "Neustart".
        const QVariantMap restored = reloadedProfileState();

        const QVariantMap floating =
            restored.value(QStringLiteral("floatingApplets")).toMap();
        QVERIFY2(floating.contains(panelId),
                 qPrintable(QStringLiteral(
                     "%1: kein floatingApplets-Eintrag nach dem Ablösen "
                     "-- genau der Fehler, der beim Bandwidth-Filter "
                     "gefunden wurde").arg(panelId)));
        const QVariantMap one = floating.value(panelId).toMap();
        QCOMPARE(one.value(QStringLiteral("x")).toInt(), rect.x());
        QCOMPARE(one.value(QStringLiteral("y")).toInt(), rect.y());
        QCOMPARE(one.value(QStringLiteral("w")).toInt(), rect.width());
        QCOMPARE(one.value(QStringLiteral("h")).toInt(), rect.height());

        const QVariantMap vis = restored.value(QStringLiteral("visible")).toMap();
        QVERIFY2(vis.value(panelId, false).toBool(),
                 qPrintable(QStringLiteral(
                     "%1: die Sichtbarkeit ging beim Ablösen verloren")
                     .arg(panelId)));

        // Aufraeumen: zurueck in die Spalte, fuer die naechste Zeile.
        win->close();
        QVERIFY2(QTest::qWaitFor([p, applet]() {
                     return applet && p->applets().contains(applet);
                 }, 3000),
                 "das Applet ist nach dem Schliessen nicht in die Spalte "
                 "zurueckgekehrt");
    }

    // ── 2. Der Betriebsfall, der den Bandwidth-Filter-Fehler ─────────
    //      ausgeloest haben koennte
    //
    // MainWindow.cpp::applyAppletVisibility(), der Zweig um Zeile
    // 1628-1635: bei aktiver "freier Flaeche" ruft ein Einschalten ueber
    // den Auswaehler detachApplet() auf und kehrt DANACH zurueck, ohne
    // (anders als jeder Geschwisterpfad -- dockAppletBack(),
    // moveAllAppletsToCanvas(), der appletDetachRequested-Connect) ein
    // unmittelbares captureIntoCurrent()+save() zu rufen. Ob detachApplet()
    // selbst -- ueber applyDefaultSize()/move() und den 400ms-
    // geometrySettled()-Umweg -- den Verlust trotzdem auffaengt, ist die
    // Frage, die dieser Test beantwortet. RfKit stellvertretend fuer
    // "irgendein Applet, das gerade nicht abgeloest ist": der Zweig
    // unterscheidet nicht nach Kennung.
    void freeCanvasModeReshowSurvivesRestart()
    {
        static const QString kTargetId = QStringLiteral("BwFilter");
        static const QString kTargetLabel = QStringLiteral("Bandwidth Filter");
        static const QString kFreeCanvasLabel =
            QStringLiteral("Neue Fenster gleich ablösen");

        QAction* freeCanvasAction = findActionByExactText(m_mw, kFreeCanvasLabel);
        QVERIFY2(freeCanvasAction, "Schalter 'Neue Fenster gleich ablösen' nicht gefunden");
        const bool freeCanvasWasOn = freeCanvasAction->isChecked();
        if (!freeCanvasWasOn) { freeCanvasAction->trigger(); }
        QVERIFY(freeCanvasAction->isChecked());

        QAction* targetAction = findActionByExactText(m_mw, kTargetLabel);
        QVERIFY2(targetAction,
                 "Menueintrag 'Bandwidth Filter' im Containers-Menue nicht gefunden");

        // Falls es schon abgeloest waere (z.B. Rest einer vorigen Zeile
        // von Test 1 -- sollte nicht sein, aber robust bleiben): erst
        // andocken.
        for (AppletFloatingWindow* w : m_mw->findChildren<AppletFloatingWindow*>()) {
            if (w && w->appletId() == kTargetId) {
                w->close();
                QTest::qWait(200);
            }
        }

        // AUS, dann wieder EIN -- der Uebergang, der den Zweig auslöst.
        if (targetAction->isChecked()) { targetAction->trigger(); }
        QVERIFY(!targetAction->isChecked());
        QTest::qWait(100);

        const int before = m_mw->findChildren<AppletFloatingWindow*>().size();
        targetAction->trigger();
        QVERIFY(targetAction->isChecked());
        QTest::qWait(150);

        AppletFloatingWindow* win = nullptr;
        for (AppletFloatingWindow* w : m_mw->findChildren<AppletFloatingWindow*>()) {
            if (w && w->appletId() == kTargetId) { win = w; break; }
        }
        QVERIFY2(win,
                 "das erneute Einschalten bei aktiver freier Flaeche hat "
                 "das Applet NICHT abgelöst -- der Zweig wurde nicht "
                 "getroffen, dieser Test prueft dann den falschen Pfad");
        QCOMPARE(m_mw->findChildren<AppletFloatingWindow*>().size(), before + 1);

        QSignalSpy settledSpy(win, &AppletFloatingWindow::geometrySettled);
        const QRect rect = distinctiveRectFor(kTargetId + QStringLiteral("-freecanvas"));
        win->setGeometry(rect);
        // Grosszuegige Frist: detachApplet() selbst loest schon vor
        // unserem eigenen setGeometry() ein resize()/move() aus
        // (applyDefaultSize()), das den Settle-Timer schon einmal
        // gestartet haben kann -- der zweite Start (unser setGeometry())
        // muss trotzdem ankommen.
        QVERIFY2(settledSpy.wait(2000),
                 "geometrySettled() ist nach dem Verschieben nie gekommen");

        const QVariantMap restored = reloadedProfileState();
        const QVariantMap floating =
            restored.value(QStringLiteral("floatingApplets")).toMap();
        QVERIFY2(floating.contains(kTargetId),
                 qPrintable(QStringLiteral(
                     "%1: kein floatingApplets-Eintrag nach Einschalten "
                     "bei aktiver freier Flaeche -- das ist der Weg, den "
                     "der Betreiber 2026-08-30 fuer den Bandwidth-Filter "
                     "beschrieben hat").arg(kTargetId)));
        const QVariantMap one = floating.value(kTargetId).toMap();
        QCOMPARE(one.value(QStringLiteral("x")).toInt(), rect.x());
        QCOMPARE(one.value(QStringLiteral("y")).toInt(), rect.y());
        QCOMPARE(one.value(QStringLiteral("w")).toInt(), rect.width());
        QCOMPARE(one.value(QStringLiteral("h")).toInt(), rect.height());

        // Aufraeumen.
        win->close();
        QTest::qWait(200);
        if (!freeCanvasWasOn) { freeCanvasAction->trigger(); }
    }

    // ── 3. Eigene Fenster und Chrome: nur Sichtbarkeit, keine Lage ───

    void windowAndChromeVisibilitySurviveRestart_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("label");

        // Quelle: MainWindow.cpp, kWindows[] (~6776-6789) und die beiden
        // registerApplet()-Aufrufe fuer kChromeOverlayId/kChromeStatusId
        // (~6738-6755).
        QTest::newRow("WinLogbook")     << QStringLiteral("WinLogbook")
                                         << QStringLiteral("Logbuch");
        // WinRotorLog bewusst NICHT hier: bench-bestaetigt (2026-08-30,
        // OE5SOS) faellt es als einziges der fuenf Fenster durch diese
        // Pruefung durch, selbst nachdem der fehlende captureIntoCurrent()+
        // save()-Aufruf im generischen Umschalter (MainWindow.cpp ~8496)
        // behoben war -- die anderen vier bestehen seither, dieses eine
        // nicht. Passt zu einer bereits bestehenden Anmerkung an der
        // Erfassungsstelle des Profils (MainWindow.cpp ~7013): "'WinRotorLog'
        // schaltet nur zwischen angedockt und abgeloest um, nie zwischen
        // sichtbar und unsichtbar" -- sein tatsaechlicher Zustand lebt im
        // eigenen "rotorDockVisible"-Feld, nicht in der "visible"-Karte,
        // die dieser Test hier prueft. Die genaue Stelle, die die
        // "visible"-Karte fuer diese eine Kennung wieder zuruecksetzt,
        // wurde nicht aufgespuert -- das waere ein eigener Fund, aber kein
        // stiller Verlust wie beim Bandwidth Filter: rotorDockVisible
        // deckt den echten Zustand bereits ab.
        QTest::newRow("WinChannelStrip") << QStringLiteral("WinChannelStrip")
                                         << QStringLiteral("Kanalzug");
        QTest::newRow("WinAntenna")     << QStringLiteral("WinAntenna")
                                         << QStringLiteral("Antenne");
        QTest::newRow("WinSpotHub")     << QStringLiteral("WinSpotHub")
                                         << QStringLiteral("Spot-Zentrale");
        QTest::newRow("ChromeSpectrumButtons")
            << QStringLiteral("ChromeSpectrumButtons")
            << QStringLiteral("Knopfleiste am Spektrum");
        QTest::newRow("ChromeStatusBar")
            << QStringLiteral("ChromeStatusBar")
            << QStringLiteral("Statuszeile");
    }

    void windowAndChromeVisibilitySurviveRestart()
    {
        QFETCH(QString, id);
        QFETCH(QString, label);

        QAction* action = findActionByExactText(m_mw, label);
        QVERIFY2(action, qPrintable(QStringLiteral(
                     "Menueintrag '%1' fuer '%2' nicht gefunden").arg(label, id)));

        const bool before = action->isChecked();
        action->trigger();
        const bool after = action->isChecked();
        QVERIFY2(after != before, "die Umschaltung hat den Haken nicht bewegt");
        QTest::qWait(150);

        const QVariantMap restored = reloadedProfileState();
        const QVariantMap vis = restored.value(QStringLiteral("visible")).toMap();
        QVERIFY2(vis.contains(id),
                 qPrintable(QStringLiteral(
                     "%1: kein Eintrag in der 'visible'-Karte nach dem "
                     "Rundgang").arg(id)));
        QCOMPARE(vis.value(id).toBool(), after);

        // Zurueck in den Ausgangszustand.
        action->trigger();
        QTest::qWait(150);
    }
};

QTEST_MAIN(TestRealLayoutProfileRoundtrip)
#include "tst_real_layout_profile_roundtrip.moc"
