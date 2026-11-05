// tests/tst_contest_panels_im_containers_menu.cpp  (Longpath)
//
// Betreiber am 2026-11-04, mit dem Containers-Menue auf dem Bild: "finde
// bei longpath jetzt nichts vom contestprogramm". Die Panels des
// Contestprogramms standen nur unter Tools > Contest logbook panels;
// hier, wo ALLE Panels mit Haken stehen, hat er sie gesucht.
//
// Dieser Pruefstand prueft am echten Hauptfenster: im Containers-Menue
// gibt es je Panel des Contestprogramms einen abhakbaren Eintrag, und
// ein Haken holt das Panel als eigenes Fenster, OHNE das Logbuch-Fenster
// zu zeigen. Alte Fassung: die Eintraege fehlen, Fall 1 faellt um.
//
// Bewusst nicht abgeraeumt (wie tst_dock_ablage_nimmt_fenster_mit).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "gui/MainWindow.h"
#include "ContestFenster.h"
#include "gui/LayoutProfiles.h"
#include "gui/applets/KombiApplet.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestContestPanelsImContainersMenu : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};
    QTemporaryDir m_ordner;

    // Wie tst_contestfenster: ein leeres Log oeffnet modal die
    // Einstellungen und greift nach rigctld und Rotoren.
    static bool belegeLogVor(const QString& ordner)
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("vorbelegung"));
        db.setDatabaseName(ordner + QStringLiteral("/contestprogramm.sqlite"));
        if (!db.open()) { return false; }
        QSqlQuery q(db);
        bool ok = q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)"));
        const QList<QPair<QString, QString>> werte = {
            {QStringLiteral("own_callsign"), QStringLiteral("OE5SOS")},
            {QStringLiteral("own_grid"), QStringLiteral("JN67UT")},
            {QStringLiteral("rigctld_host"), QString()},
            {QStringLiteral("rotor1_enabled"), QStringLiteral("0")},
            {QStringLiteral("rotor2_enabled"), QStringLiteral("0")},
            {QStringLiteral("esm_enabled"), QStringLiteral("0")},
            {QStringLiteral("spectrum_enabled"), QStringLiteral("0")},
        };
        for (const auto& kv : werte) {
            q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)"));
            q.addBindValue(kv.first);
            q.addBindValue(kv.second);
            ok = ok && q.exec();
        }
        db.close();
        QSqlDatabase::removeDatabase(QStringLiteral("vorbelegung"));
        return ok;
    }

    QMenu* containersMenu() const
    {
        for (QMenu* m : m_mw->findChildren<QMenu*>()) {
            if (m->title().remove(QLatin1Char('&')) == QStringLiteral("Containers")) {
                return m;
            }
        }
        return nullptr;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", m_ordner.path().toUtf8());
        QVERIFY(belegeLogVor(m_ordner.path()));

        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);
    }

    // ── Fall 1: je Panel ein Eintrag mit Haken im Containers-Menue ───────
    void jedesPanelStehtImContainersMenu()
    {
        QMenu* menu = containersMenu();
        QVERIFY2(menu, "das Containers-Menue muss da sein");
        const QList<ContestFenster::PanelEintrag> panels = ContestFenster::panels();
        QVERIFY(panels.size() >= 8);
        for (const ContestFenster::PanelEintrag& e : panels) {
            QAction* a = m_mw->findChild<QAction*>(
                QStringLiteral("actContainersContestPanel_%1").arg(e.id));
            QVERIFY2(a, qPrintable(QStringLiteral("Eintrag fuer '%1' fehlt im Containers-Menue").arg(e.id)));
            QVERIFY(a->isCheckable());
            QCOMPARE(a->text(), QStringLiteral("Contest · %1").arg(e.titel));
            QVERIFY2(menu->actions().contains(a), "der Eintrag muss IM Containers-Menue haengen");
        }
    }

    // ── Fall 2: der Haken holt das Panel als eigenes Fenster ─────────────
    void derHakenHoltDasPanelAlsFenster()
    {
        QAction* chat = m_mw->findChild<QAction*>(
            QStringLiteral("actContainersContestPanel_chat"));
        QVERIFY(chat);
        chat->setChecked(true);
        QTest::qWait(300);

        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY2(bruecke, "der Haken muss das Logbuch laden");
        QVERIFY(bruecke->istPanelSchwebend(QStringLiteral("chat")));
        QWidget* fenster = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(fenster && fenster->isWindow());
        QTRY_VERIFY_WITH_TIMEOUT(fenster->isVisible(), 3000);
        QVERIFY2(!bruecke->fenster()->isVisible(), "das Logbuch-Fenster bleibt zu");

        // Beim Aufklappen zieht das Menue den Haken nach.
        QMenu* menu = containersMenu();
        QVERIFY(menu);
        emit menu->aboutToShow();
        QVERIFY(chat->isChecked());

        chat->setChecked(false);
        QTest::qWait(200);
        QVERIFY(!bruecke->istPanelSchwebend(QStringLiteral("chat")));
    }

    // ── Fall 3: das Profil bringt die Panels zurueck (2026-11-05) ───────
    //
    // Betreiber nach einem Neustart: "so war mein letzter zustand
    // nicht!!!!" Ein Profil mit contestPanels/contestLogbook holt die
    // Panels als Fenster an ihre Stelle, die Erfassung schreibt sie
    // zurueck, und ein Profil ohne Contest raeumt sie weg.
    void dasProfilBringtDiePanelsZurueck()
    {
        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        const QString vorher = profile->current();
        QVERIFY(!vorher.isEmpty());

        const QRect chatSoll(128, 144, 397, 301);
        QVariantMap zustand = profile->snapshot(vorher);
        zustand.insert(QStringLiteral("contestPanels"),
                       QVariantMap{{QStringLiteral("chat"),
                                    QVariantMap{{QStringLiteral("x"), chatSoll.x()},
                                                {QStringLiteral("y"), chatSoll.y()},
                                                {QStringLiteral("w"), chatSoll.width()},
                                                {QStringLiteral("h"), chatSoll.height()}}}});
        zustand.insert(QStringLiteral("contestLogbook"), false);
        QVERIFY(profile->createWith(QStringLiteral("Contest"), zustand));
        QVERIFY(profile->activate(QStringLiteral("Contest")));
        QTest::qWait(800);   // das Herstellen laeuft im naechsten Durchlauf

        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke && bruecke->istOffen());
        QVERIFY2(bruecke->istPanelSchwebend(QStringLiteral("chat")), "chat muss ein Fenster sein");
        QWidget* chat = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(chat && chat->isWindow());
        QTRY_COMPARE_WITH_TIMEOUT(chat->frameGeometry(), chatSoll, 3000);
        // Das Hauptfenster ist das aktive Fenster -- nur es hat die
        // Menueleiste (live 2026-11-05: "Apple · Longpath", sonst nichts).
        QTRY_VERIFY2_WITH_TIMEOUT(QApplication::activeWindow() == m_mw, "das Hauptfenster muss aktiv bleiben", 3000);
        QVERIFY2(!bruecke->fenster()->isVisible(), "das Logbuch-Fenster bleibt zu");

        // Die Erfassung schreibt die Anordnung zurueck.
        profile->captureIntoCurrent();
        const QVariantMap erfasst = profile->snapshot(QStringLiteral("Contest"));
        const QVariantMap panels = erfasst.value(QStringLiteral("contestPanels")).toMap();
        QVERIFY2(panels.contains(QStringLiteral("chat")), "chat fehlt in der Erfassung");
        QCOMPARE(erfasst.value(QStringLiteral("contestLogbook")).toBool(), false);

        // Zurueck zum Profil ohne Contest: das Panel dockt zurueck.
        QVERIFY(profile->activate(vorher));
        QTest::qWait(800);
        QVERIFY(!bruecke->istPanelSchwebend(QStringLiteral("chat")));
    }

    // ── Fall 4: das Combined-Feld nimmt ein Contest-Panel (2026-11-05) ──
    //
    // Betreiber: "im combined sollten auch alle contest optionen moeglich
    // sein". Ins Feld, im Profil als "Contest:chat", nicht als Fenster;
    // herausgezogen wird es wieder ein Fenster; ein Profil mit dem Feld-
    // Inhalt holt es zurueck, eines ohne raeumt es weg.
    void dasCombinedFeldNimmtEinContestPanel()
    {
        auto* kombi = m_mw->findChild<KombiApplet*>();
        QVERIFY2(kombi, "das Combined-Feld muss da sein");
        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        const QString vorher = profile->current();

        m_mw->holeContestInsKombiFeldForTest(QStringLiteral("chat"));
        QTest::qWait(300);
        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke);
        QWidget* chat = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(chat && !chat->isWindow());
        QVERIFY2(kombi->seiten().contains(chat), "chat muss eine Seite des Feldes sein");
        QVERIFY(bruecke->panelInFremdemGehaeuse(QStringLiteral("chat")));

        profile->captureIntoCurrent();
        const QVariantMap erfasst = profile->snapshot(vorher);
        QVERIFY2(erfasst.value(QStringLiteral("kombiInhalt")).toStringList()
                     .contains(QStringLiteral("Contest:chat")),
                 "im Profil steht das Feld mit Contest:chat");
        QVERIFY2(!erfasst.value(QStringLiteral("contestPanels")).toMap().contains(QStringLiteral("chat")),
                 "und nicht als Fenster");

        // Herausgezogen: ein Fenster dort, wo losgelassen wurde.
        kombi->gibFrei(chat);
        emit kombi->herausgezogen(chat, QPoint(300, 300));
        QTest::qWait(300);
        QVERIFY2(chat->isWindow(), "nach dem Herausziehen ein Fenster");
        QVERIFY(!bruecke->panelInFremdemGehaeuse(QStringLiteral("chat")));
        QVERIFY(bruecke->istPanelSchwebend(QStringLiteral("chat")));

        // Ein Profil, das das Feld mit Contest:chat will.
        QVariantMap zustand = profile->snapshot(vorher);
        zustand.insert(QStringLiteral("kombiInhalt"), QStringList{QStringLiteral("Contest:chat")});
        zustand.insert(QStringLiteral("contestPanels"), QVariantMap());
        zustand.insert(QStringLiteral("contestLogbook"), false);
        QVERIFY(profile->createWith(QStringLiteral("Kombi"), zustand));
        QVERIFY(profile->activate(QStringLiteral("Kombi")));
        QTest::qWait(800);
        QVERIFY2(kombi->seiten().contains(chat), "das Profil holt chat ins Feld");
        QVERIFY(!chat->isWindow());

        // Und eines ohne: zurueck in die Flaeche des Logbuchs.
        QVariantMap leer = profile->snapshot(vorher);
        leer.insert(QStringLiteral("kombiInhalt"), QStringList());
        leer.insert(QStringLiteral("contestPanels"), QVariantMap());
        leer.insert(QStringLiteral("contestLogbook"), false);
        QVERIFY(profile->createWith(QStringLiteral("Leer"), leer));
        QVERIFY(profile->activate(QStringLiteral("Leer")));
        QTest::qWait(800);
        QVERIFY(!kombi->seiten().contains(chat));
        QVERIFY(!bruecke->istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY(!chat->isWindow());
    }
};

QTEST_MAIN(TestContestPanelsImContainersMenu)
#include "tst_contest_panels_im_containers_menu.moc"
