// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_window_close_stays_closed.cpp  (Longpath)
// =================================================================
// Ein Fenster, das der Betreiber selbst zumacht, bleibt zu.
//
// Der Betreiber, 2026-09-27: „es ist channel strip, logbook usw.
// immer wieder erneut offen". Kanalzug, Logbuch und Spot-Zentrale
// meldeten ihr Schliessen nirgends: der Haken in der Widget-Auswahl
// blieb an, das Profil sicherte „sichtbar", und der naechste Start
// oeffnete sie wieder. Dieselbe Luecke wie beim Antennenfenster am
// 2026-09-01 (AntennaWindow::closed).
//
// Geprueft:
//   - roter Knopf (close()) -> Haken aus, Profil sagt „zu";
//   - bloss verstecken (Profilwechsel, Verbinden-Maske) -> Haken bleibt;
//   - Beenden mit offenem Fenster, auch wenn Qt (Cmd+Q) den Dialog vor
//     dem Hauptfenster schliesst -> Haken bleibt, es geht wieder auf.
//
// Modification history (Longpath):
//   2026-09-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QDialog>

#include "core/AppSettings.h"
#include "gui/ConnectionPanel.h"
#include "gui/LayoutProfiles.h"
#include "gui/LogbookWindow.h"
#include "gui/MainWindow.h"
#include "gui/SpotHubDialog.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/StripWindow.h"

using namespace Longpath;

namespace {

// Das Hauptfenster wie im echten Start, dann die Verbinden-Maske zu:
// sie versteckt sonst jedes schwebende Fenster (Muster aus
// tst_closing_takes_the_float_along.cpp).
// Das zuletzt gebaute Fenster — cleanup() raeumt es nach jedem Fall ab.
//
// ── Warum das noetig wurde (2026-10-16) ─────────────────────────────
//
// Hier stand „bewusst nicht abgeraeumt", und jeder der sieben Faelle
// liess sein MainWindow offen stehen. Gemessen: jedes braucht rund
// 5 Sekunden bis `qWaitForWindowExposed`, und mit jedem weiteren
// offenen Fenster (Ton, Taktgeber, Suche, Netz) wird es langsamer.
// Beim FUENFTEN riss die Vorgabefrist von 5000 ms:
//
//     Fenster 1: exposed 4983 ms
//     Fenster 2: exposed 4951 ms
//     Fenster 3: exposed 5016 ms
//     Fenster 4: exposed 5431 ms
//     Fenster 5: exposed scheiterte nach 5078 ms
//
// Der Lauf fiel darum in 5 von 6 Durchgaengen um, an wechselnden
// Stellen, und brauchte 30 Sekunden. Beides hat dieselbe Ursache.
//
// Abgeraeumt werden kann seit dem 2026-10-15: die vier Wachen gegen
// die Abstuerze beim Beenden stehen (siehe
// docs/architecture/… bzw. tst_abbau_ueberlebt).
MainWindow* g_zuletzt = nullptr;

MainWindow* startMainWindow()
{
    auto* mw = new MainWindow();
    g_zuletzt = mw;
    mw->resize(1280, 800);
    mw->show();
    // Grosszuegiger als die Vorgabe (5 s): unter Last braucht das
    // Fenster laenger, und die Frist laeuft nur im Fehlerfall ab.
    if (!QTest::qWaitForWindowExposed(mw, 20000)) { return nullptr; }
    if (!QTest::qWaitFor([mw]() { return mw->findChild<ConnectionPanel*>() != nullptr; },
                         15000)) {
        return nullptr;
    }
    mw->findChild<ConnectionPanel*>()->close();
    if (!QTest::qWaitFor([mw]() { return mw->findChild<ConnectionPanel*>() == nullptr; },
                         5000)) {
        return nullptr;
    }
    return mw;
}

bool profileSaysVisible(LayoutProfiles* p, const QString& id)
{
    return p->snapshot(p->current()).value(QStringLiteral("visible")).toMap()
        .value(id).toBool();
}

QDialog* windowFor(MainWindow* mw, const QString& id)
{
    if (id == QLatin1String("WinChannelStrip")) { return mw->findChild<StripWindow*>(); }
    if (id == QLatin1String("WinSpotHub"))      { return mw->findChild<SpotHubDialog*>(); }
    if (id == QLatin1String("WinLogbook"))      { return mw->findChild<LogbookWindow*>(); }
    return nullptr;
}

} // namespace

class TstWindowCloseStaysClosed : public QObject
{
    Q_OBJECT
private slots:
    /// Nach JEDEM Fall das Fenster abraeumen — sonst stapeln sich
    /// sieben davon und der naechste Fall wartet auf einen Schirm,
    /// den sechs andere blockieren.
    void cleanup()
    {
        if (!g_zuletzt) { return; }
        g_zuletzt->close();
        g_zuletzt->deleteLater();
        g_zuletzt = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(50);
    }

    void initTestCase()
    {
        // Das Logbuch liest RotorLogbookPanel::logbookPath(). Eine gesetzte
        // Sandbox-Variable lenkte das auf einen echten Ordner -- hier nie.
        qunsetenv("LONGPATH_CONFIG_DIR");
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    }

    void theRedButtonClosesForGood_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("Kanalzug")      << QStringLiteral("WinChannelStrip");
        QTest::newRow("Spot-Zentrale") << QStringLiteral("WinSpotHub");
        QTest::newRow("Logbuch")       << QStringLiteral("WinLogbook");
    }

    void theRedButtonClosesForGood()
    {
        QFETCH(QString, id);
        MainWindow* mw = startMainWindow();
        QVERIFY(mw);
        auto* vis = mw->findChild<AppletVisibilityController*>();
        auto* profiles = mw->findChild<LayoutProfiles*>();
        QVERIFY(vis && profiles);

        vis->setVisible(id, true);
        QTRY_VERIFY_WITH_TIMEOUT(windowFor(mw, id) && windowFor(mw, id)->isVisible(), 5000);
        const QString name = QStringLiteral("Probe-%1").arg(id);
        QVERIFY(profiles->create(name));
        QVERIFY(profileSaysVisible(profiles, id));

        windowFor(mw, id)->close();     // was der rote Knopf tut

        QTRY_VERIFY_WITH_TIMEOUT(!vis->isVisible(id), 2000);
        QVERIFY2(!profileSaysVisible(profiles, id),
                 "das Profil muss zu sagen, sonst oeffnet der naechste "
                 "Start das Fenster wieder");
        // AppletVisibilityController schreibt "True"/"False".
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Applet%1Visible").arg(id)).toString(),
                 QStringLiteral("False"));
    }

    // Profilwechsel und Verbinden-Maske verstecken nur (hide()): das ist
    // kein Wunsch des Betreibers, der Haken bleibt.
    void merelyHidingKeepsTheTick()
    {
        MainWindow* mw = startMainWindow();
        QVERIFY(mw);
        auto* vis = mw->findChild<AppletVisibilityController*>();
        QVERIFY(vis);
        vis->setVisible(QStringLiteral("WinChannelStrip"), true);
        QTRY_VERIFY_WITH_TIMEOUT(mw->findChild<StripWindow*>()
                                 && mw->findChild<StripWindow*>()->isVisible(), 5000);

        mw->findChild<StripWindow*>()->hide();
        QTest::qWait(100);
        QVERIFY(vis->isVisible(QStringLiteral("WinChannelStrip")));
    }

    // Beenden mit offenem Kanalzug: er soll beim naechsten Start wieder
    // da sein. Cmd+Q schliesst die Fenster in beliebiger Reihenfolge --
    // hier der ungünstige Fall, der Dialog zuerst, im selben Durchlauf.
    void quittingWithTheWindowOpenKeepsIt()
    {
        MainWindow* mw = startMainWindow();
        QVERIFY(mw);
        auto* vis = mw->findChild<AppletVisibilityController*>();
        auto* profiles = mw->findChild<LayoutProfiles*>();
        QVERIFY(vis && profiles);
        vis->setVisible(QStringLiteral("WinChannelStrip"), true);
        QTRY_VERIFY_WITH_TIMEOUT(mw->findChild<StripWindow*>()
                                 && mw->findChild<StripWindow*>()->isVisible(), 5000);
        QVERIFY(profiles->create(QStringLiteral("Probe-Beenden")));

        mw->findChild<StripWindow*>()->close();
        mw->close();
        QTest::qWait(200);

        QVERIFY2(vis->isVisible(QStringLiteral("WinChannelStrip")),
                 "Schliessen beim Beenden ist kein Wunsch des Betreibers");
        QVERIFY(profileSaysVisible(profiles, QStringLiteral("WinChannelStrip")));
    }
};

QTEST_MAIN(TstWindowCloseStaysClosed)
#include "tst_window_close_stays_closed.moc"
