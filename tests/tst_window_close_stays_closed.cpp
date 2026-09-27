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
MainWindow* startMainWindow()
{
    auto* mw = new MainWindow();        // bewusst nicht abgeraeumt
    mw->resize(1280, 800);
    mw->show();
    if (!QTest::qWaitForWindowExposed(mw)) { return nullptr; }
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
