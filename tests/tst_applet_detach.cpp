// =================================================================
// tests/tst_applet_detach.cpp  (NereusSDR)
// =================================================================
//
// Ein Applet aus der Spalte in ein eigenes Fenster — und zurück.
//
// Drei Dinge werden hier festgehalten, und alle drei sind die Art
// Fehler, die man erst auf dem Schreibtisch bemerkt:
//
//  1. Der EINE Weg. Abgelöst wird über den Menüpunkt, sonst nirgends.
//     Ziehen bleibt Umsortieren innerhalb der Spalte — dort findet
//     kein Fensterwechsel statt. Das ist keine Geschmacksfrage:
//     AetherSDR hat für dieselbe Sache genau das getan, wegen
//     Abstürzen beim Umhängen über Top-Level-Grenzen (#2495 QRhiWidget
//     mit Aufräum-Rückruf auf freigegebenem Zustand, #4319 D3D11 beim
//     Texturneuanlegen, #4617). Die Prüfung „Ziehen löst NICHT ab" ist
//     deshalb keine Formalie, sondern der Zaun um einen bekannten
//     Absturz.
//
//  2. Dass der eine Weg auch wirklich etwas tut. Ein einziger Pfad,
//     den niemand prüft, ist ein einziger Pfad, der eines Tages
//     stillschweigend nichts mehr tut.
//
//  3. Das Eigentum. releaseApplet() muss das Applet UNBESCHÄDIGT
//     herausgeben — Elternteil weg, aber am Leben. MainWindow hält
//     rohe Zeiger auf jedes Applet; gäbe das Fenster es beim Sterben
//     mit, wäre der nächste Zugriff auf Gelöschtes, und zwar aus einer
//     ganz anderen Ecke des Programms.
//
// Was hier NICHT geprüft wird: wo ein gezeigtes Fenster landet. Das
// entscheidet der Fensterverwalter, und eine Zusicherung darüber wäre
// je nach Schreibtisch mal wahr und mal nicht.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-16 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-17 — Ziehen-zum-Ablösen wieder entfernt; die Schwellen-
//                 Prüfungen wurden zu Prüfungen, dass GAR kein Zug
//                 ablöst. Menüpfad dazu. Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QSignalSpy>

#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletKeys.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/AppletWidget.h"

using namespace NereusSDR;

namespace {

class StubApplet : public AppletWidget {
public:
    explicit StubApplet(QString id)
        : AppletWidget(nullptr), m_id(std::move(id)) {}
    QString appletId() const override { return m_id; }
    QString appletTitle() const override { return m_id.toUpper(); }
    void syncFromModel() override {}

private:
    QString m_id;
};

/// Die Titelleiste einer Hülle — der Griff, auf dem der Ereignisfilter
/// sitzt. Sie ist das direkte Kind der Hülle, das nicht das Applet ist.
QWidget* titleBarFor(AppletWidget* applet)
{
    QWidget* wrapper = applet ? applet->parentWidget() : nullptr;
    if (!wrapper) { return nullptr; }
    const QList<QWidget*> kids =
        wrapper->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* k : kids) {
        if (k != applet) { return k; }
    }
    return nullptr;
}

void sendMouse(QWidget* target, QEvent::Type type, QPointF global,
               Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent ev(type, QPointF(4, 4), global, global, button, buttons,
                   Qt::NoModifier);
    QCoreApplication::sendEvent(target, &ev);
}

} // namespace

class TestAppletDetach : public QObject
{
    Q_OBJECT

private slots:

    // ── Der Zaun: kein Zug löst ab ───────────────────────────────────
    //
    // In JEDE Richtung geprüft, nicht nur senkrecht. Eine seitliche
    // Schwelle hat es einen Tag lang gegeben; sie ist wegen der
    // AetherSDR-Abstürze wieder heraus, und dieser Test ist der Grund,
    // warum sie nicht unbemerkt zurückkommt.

    void noDragInAnyDirectionDetaches()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.addApplet(new StubApplet(QStringLiteral("tx")));

        QWidget* bar = titleBarFor(rx);
        QVERIFY2(bar, "Titelleiste nicht gefunden — Hüllenaufbau geändert?");

        QSignalSpy spy(&panel, &AppletPanelWidget::appletDetachRequested);
        const QPointF start(500, 300);

        // Weit über jede denkbare Schwelle, in acht Richtungen.
        const QList<QPointF> aways = {
            {400, 0}, {-400, 0}, {0, 400}, {0, -400},
            {300, 300}, {-300, 300}, {300, -300}, {-300, -300},
        };
        for (const QPointF& away : aways) {
            sendMouse(bar, QEvent::MouseButtonPress, start,
                      Qt::LeftButton, Qt::LeftButton);
            sendMouse(bar, QEvent::MouseMove, start + away,
                      Qt::NoButton, Qt::LeftButton);
            sendMouse(bar, QEvent::MouseButtonRelease, start + away,
                      Qt::LeftButton, Qt::NoButton);
        }

        QCOMPARE(spy.count(), 0);
    }

    // Dass das Umsortieren selbst noch geht, steht in
    // tst_applet_reorder und wird hier nicht wiederholt. Es dort
    // nachzubauen hiesse, dieselbe Zusicherung an zwei Stellen zu
    // pflegen — und eine davon vergisst man.

    // ── Der eine Weg ─────────────────────────────────────────────────

    void theMenuIsTheWayOut()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.addApplet(new StubApplet(QStringLiteral("tx")));

        QMenu* menu = panel.buildTitleBarMenuForTesting(rx);
        QVERIFY2(menu, "kein Kontextmenue fuer ein Applet der Spalte");
        QCOMPARE(menu->actions().size(), 1);
        QCOMPARE(menu->actions().first()->text(),
                 QStringLiteral("Als Fenster ablösen"));

        QSignalSpy spy(&panel, &AppletPanelWidget::appletDetachRequested);
        menu->actions().first()->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<AppletWidget*>(),
                 static_cast<AppletWidget*>(rx));
        // Die Stelle muss mit hinaus: nach dem Ausbau kann das Applet
        // nicht mehr sagen, wo es herkam.
        QCOMPARE(spy.at(0).at(1).toInt(), 0);
        delete menu;
    }

    void menuDetachPositionFollowsTheStackNotTheRegistrationOrder()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.addApplet(new StubApplet(QStringLiteral("tx")));
        panel.addApplet(new StubApplet(QStringLiteral("eq")));

        // rx ans Ende schieben, dann ablösen: die gemeldete Stelle muss
        // die neue sein, sonst kehrt es an die falsche zurück.
        QVERIFY(panel.moveApplet(rx, 2));
        QCOMPARE(panel.appletPosition(rx), 2);

        QMenu* menu = panel.buildTitleBarMenuForTesting(rx);
        QVERIFY(menu);
        QSignalSpy spy(&panel, &AppletPanelWidget::appletDetachRequested);
        menu->actions().first()->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 2);
        delete menu;
    }

    // Ein Applet, das nicht in dieser Spalte steht (schon abgelöst),
    // bekommt kein Menü — sonst liesse sich dasselbe Applet zweimal
    // herauslösen, und der zweite Zeiger zeigte auf nichts.
    void noMenuForAnAppletThatIsNotInTheColumn()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.removeApplet(rx);

        QVERIFY(panel.buildTitleBarMenuForTesting(rx) == nullptr);
        delete rx;
    }

    // ── Das Eigentum ─────────────────────────────────────────────────

    void releaseGivesTheAppletBackAlive()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        QPointer<AppletWidget> guard(rx);

        auto* win = new AppletFloatingWindow(rx, QStringLiteral("Rx"), 3);
        QCOMPARE(win->applet(), static_cast<AppletWidget*>(rx));
        // „Rx", nicht „rx": das Fenster meldet den Schluessel, unter
        // dem es gefuehrt wird. Hier stand appletId() des Applets, und
        // damit kam dockRequested unter einem Namen an, den
        // m_floatingApplets nicht kannte.
        QCOMPARE(win->appletId(), QStringLiteral("Rx"));
        QCOMPARE(win->dockIndex(), 3);
        QCOMPARE(rx->parentWidget(), static_cast<QWidget*>(win));

        AppletWidget* back = win->releaseApplet();
        QCOMPARE(back, static_cast<AppletWidget*>(rx));
        QVERIFY2(back->parentWidget() == nullptr,
                 "releaseApplet muss die Elternschaft loesen, sonst "
                 "stirbt das Applet mit dem Fenster");
        QVERIFY(win->applet() == nullptr);

        delete win;
        QVERIFY2(!guard.isNull(),
                 "das Applet hat das Fenster nicht ueberlebt");
        delete back;
    }

    void releaseTwiceIsHarmless()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* win = new AppletFloatingWindow(rx, QStringLiteral("Rx"), 0);
        AppletWidget* first = win->releaseApplet();
        QVERIFY(first);
        QVERIFY(win->releaseApplet() == nullptr);
        delete win;
        delete first;
    }

    // Die Kehrseite, damit der Vertrag in beide Richtungen steht: wer
    // NICHT freigibt, nimmt das Applet mit. Das ist kein Fehler,
    // sondern der Grund, warum dockAppletBack() erst freigibt und dann
    // das Fenster abraeumt.
    void windowWithoutReleaseTakesTheAppletWithIt()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        QPointer<AppletWidget> guard(rx);
        auto* win = new AppletFloatingWindow(rx, QStringLiteral("Rx"), 0);
        delete win;
        QVERIFY(guard.isNull());
    }


    // ── Der Fehler vom 2026-08-18: zwei Kennungen ────────────────────
    //
    // „Als Fenster abloesen verliert das Panel. Das Panel verschwindet
    // aus der Spalte, kein Fenster erscheint, und der Zustand wird
    // gespeichert." — OE5SOS.
    //
    // Ursache: jedes Applet trug zwei Namen. Die Panelkennung („Rx")
    // fuehrte den Auswaehler, die Sichtbarkeit und das Profil; die
    // Eigenkennung („rx") stand in appletId(). detachApplet fragte die
    // Sichtbarkeit unter der Eigenkennung ab, fand keinen Eintrag und
    // liess das fertig gebaute Fenster ungezeigt.
    //
    // Die drei Faelle unten pruefen die Aufloesung selbst. Sie steht
    // deshalb als freie Funktion in AppletKeys und nicht als Methode
    // auf MainWindow: MainWindow laesst sich hier nicht bauen (ein
    // blosses `MainWindow w;` startet echte UDP-Suche im Netz).

    void thePanelIdWinsOverTheAppletsOwnId()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("TX"));
        const AppletMap map{{QStringLiteral("Rx"), rx},
                            {QStringLiteral("Tx"), tx}};

        QCOMPARE(AppletKeys::panelIdFor(map, rx), QStringLiteral("Rx"));
        QCOMPARE(AppletKeys::panelIdFor(map, tx), QStringLiteral("Tx"));
        delete rx;
        delete tx;
    }

    // Aufnahmen von vor dem Update nennen Eigenkennungen. Wuerden sie
    // hier nicht mehr aufgeloest, verloere jeder beim ersten Start nach
    // diesem Update seine Anordnung — von vierzehn Eintraegen loesten
    // sich vier auf, der Rest fiel still weg.
    void bothKindsOfKeyStillFindTheApplet()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        const AppletMap map{{QStringLiteral("Rx"), rx}};

        QCOMPARE(AppletKeys::appletFor(map, QStringLiteral("Rx")),
                 static_cast<AppletWidget*>(rx));
        QCOMPARE(AppletKeys::appletFor(map, QStringLiteral("rx")),
                 static_cast<AppletWidget*>(rx));
        QCOMPARE(AppletKeys::canonical(map, QStringLiteral("rx")),
                 QStringLiteral("Rx"));

        // Eine Kennung ohne Applet bleibt, wie sie ist: eine Aufnahme
        // kann Widgets nennen, die es nicht mehr gibt, und die sollen
        // nicht stillschweigend zu etwas anderem werden.
        QCOMPARE(AppletKeys::canonical(map, QStringLiteral("weg")),
                 QStringLiteral("weg"));
        QVERIFY(AppletKeys::appletFor(map, QStringLiteral("weg")) == nullptr);
        delete rx;
    }

    // Der Fehler in einer Zeile: unter welcher Kennung die Sichtbarkeit
    // steht, und unter welcher gefragt wurde.
    void visibilityIsKeptUnderThePanelId()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        const AppletMap map{{QStringLiteral("Rx"), rx}};

        AppletVisibilityController vis;
        vis.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"),
                           /*defaultVisible=*/true);

        QVERIFY2(vis.isEffectivelyVisible(
                     AppletKeys::panelIdFor(map, rx)),
                 "die Panelkennung muss die Sichtbarkeit finden");
        QVERIFY2(!vis.isEffectivelyVisible(rx->appletId()),
                 "die Eigenkennung darf NICHT als sichtbar gelten — sonst "
                 "sagt dieser Test nichts ueber den Fehler aus");
        delete rx;
    }

    // ── Hin UND zurueck ──────────────────────────────────────────────
    //
    // „Der Weg muss in beide Richtungen laufen, sonst ist er eine
    // Falle." — OE5SOS. Geprueft wird die Eigentumsuebergabe an den
    // echten Klassen; MainWindow steuert nur die Kennung bei, und die
    // hat oben ihre eigenen Faelle.

    void detachThenDockPutsTheAppletBackInTheColumn()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("TX"));
        panel.addApplet(rx);
        panel.addApplet(tx);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        const AppletMap map{{QStringLiteral("Rx"), rx},
                            {QStringLiteral("Tx"), tx}};
        const int idx = panel.appletPosition(rx);
        QCOMPARE(idx, 0);

        // ── hinaus ───────────────────────────────────────────────────
        panel.removeApplet(rx);
        auto* win = new AppletFloatingWindow(
            rx, AppletKeys::panelIdFor(map, rx), idx);
        win->resize(260, 120);
        win->show();
        // isVisible(), nicht qWaitForWindowExposed: ob der
        // Fensterverwalter das Fenster tatsaechlich auf den Schirm
        // bringt, entscheidet er, und im Pruefstand oft gar nicht. Der
        // FEHLER war ein nie gerufenes show() — und genau das sagt
        // isVisible().
        QVERIFY(QTest::qWaitFor([win]() { return win->isVisible(); }, 2000));

        QVERIFY2(!panel.applets().contains(rx),
                 "das Applet steht noch in der Spalte");
        QCOMPARE(win->appletId(), QStringLiteral("Rx"));
        QVERIFY2(win->isVisible(), "das Fenster ist nicht sichtbar");
        QVERIFY2(rx->isVisible(),
                 "das Applet im Fenster ist unsichtbar — removeApplet "
                 "versteckt es, und das Fenster muss es wieder zeigen");
        QCOMPARE(rx->parentWidget(), static_cast<QWidget*>(win));

        // ── und zurueck ──────────────────────────────────────────────
        const int back = win->dockIndex();
        AppletWidget* a = win->releaseApplet();
        QCOMPARE(a, static_cast<AppletWidget*>(rx));
        delete win;

        panel.addApplet(a);
        panel.moveApplet(a, back);

        QVERIFY2(panel.applets().contains(rx),
                 "das Applet ist nicht in die Spalte zurueckgekehrt");
        QCOMPARE(panel.appletPosition(rx), idx);
        QVERIFY2(QTest::qWaitFor([rx]() { return rx->isVisible(); }, 2000),
                 "zurueck in der Spalte, aber unsichtbar");
    }

    void closingTheWindowAsksToDock()
    {
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* win = new AppletFloatingWindow(rx, QStringLiteral("Rx"), 1);
        QSignalSpy spy(win, &AppletFloatingWindow::dockRequested);

        win->close();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Rx"));

        AppletWidget* back = win->releaseApplet();
        delete win;
        delete back;
    }
};

QTEST_MAIN(TestAppletDetach)
#include "tst_applet_detach.moc"
