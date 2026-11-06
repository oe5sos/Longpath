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
#include <QJsonDocument>
#include <QJsonObject>
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "gui/MainWindow.h"
#include "ContestFenster.h"
#include "gui/ConnectionPanel.h"
#include "gui/LayoutProfiles.h"
#include "gui/PanadapterApplet.h"
#include "gui/applets/AppletVisibilityController.h"
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

    // ── Fall 5: hinter der Verbindungsmaske ist nichts (2026-11-05) ─────
    //
    // Betreiber, mit Bild von Log und Chat hinter der Maske: "ich will bei
    // dieser connect maske nichts im hintergrund haben" -- "hatte ich dir
    // auch schon hunderte mal gesagt". Die Contest-Fenster (Panels und
    // Logbuch) gehen mit den anderen hinter die Maske und kommen mit ihr
    // wieder hervor.
    void hinterDerMaskeIstNichts()
    {
        if (auto* alt = m_mw->findChild<ConnectionPanel*>()) { alt->close(); QTest::qWait(300); }
        QAction* chatAction = m_mw->findChild<QAction*>(
            QStringLiteral("actContainersContestPanel_chat"));
        QVERIFY(chatAction);
        chatAction->setChecked(true);
        QTest::qWait(300);
        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke);
        QWidget* chat = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(chat && chat->isWindow());
        QTRY_VERIFY_WITH_TIMEOUT(chat->isVisible(), 3000);
        QString grund;
        QVERIFY2(bruecke->oeffne(&grund, m_mw), qPrintable(grund));
        QWidget* log = bruecke->fenster();
        QVERIFY(log && log->isVisible());

        const int vorher = m_mw->floatingWindowsWaitingForConnectMaskForTest();
        QVERIFY(QMetaObject::invokeMethod(m_mw, "showConnectionPanel"));
        QTRY_VERIFY_WITH_TIMEOUT(m_mw->findChild<ConnectionPanel*>() != nullptr, 20000);
        ConnectionPanel* maske = m_mw->findChild<ConnectionPanel*>();
        QTRY_VERIFY_WITH_TIMEOUT(maske->isVisible(), 5000);
        QVERIFY2(!chat->isVisible(), "das Chat-Fenster steht hinter der Maske");
        QVERIFY2(!log->isVisible(), "das Logbuch-Fenster steht hinter der Maske");
        QVERIFY(m_mw->floatingWindowsWaitingForConnectMaskForTest() >= vorher + 2);

        maske->close();
        QTRY_VERIFY_WITH_TIMEOUT(m_mw->findChild<ConnectionPanel*>() == nullptr, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(chat->isVisible(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(log->isVisible(), 5000);
        bruecke->dockePanel(QStringLiteral("chat"));
        log->close();   // versteckt nur
    }

    // ── Fall 6: Datei > Save Current Layout sichert, was man sieht ──────
    //
    // Betreiber 2026-11-05: "habe es total anders gespeichert als
    // contest" -- der Export auf der Schiene nahm das Profil des
    // angeklickten Buchstabens (C, gespeicherter Stand) statt der
    // Live-Anordnung (N). Der Menuepunkt nimmt immer das aktive Profil,
    // live erfasst: mit den Contest-Panels.
    void dasDateiMenueSichertDieLiveAnordnung()
    {
        QAction* sichern = m_mw->findChild<QAction*>(QStringLiteral("actSaveCurrentLayout"));
        QVERIFY2(sichern && sichern->isEnabled(), "Datei > Save Current Layout fehlt");

        QString grund;
        QVERIFY2(m_mw->contestFensterForTest()->zeigePanelAlsFenster(QStringLiteral("chat"), &grund, m_mw),
                 qPrintable(grund));
        QTest::qWait(300);
        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);

        const QByteArray json = m_mw->layoutprofilAlsJsonForTest(profile->current());
        QVERIFY(!json.isEmpty());
        const QJsonObject o = QJsonDocument::fromJson(json).object();
        QCOMPARE(o.value(QStringLiteral("name")).toString(), profile->current());
        const QJsonObject state = o.value(QStringLiteral("state")).toObject();
        QVERIFY2(state.value(QStringLiteral("contestPanels")).toObject().contains(QStringLiteral("chat")),
                 "die Live-Anordnung mit dem Chat-Fenster muss in der Datei stehen");
        m_mw->contestFensterForTest()->dockePanel(QStringLiteral("chat"));
    }

    // ── Fall: die Anzeigezeile gehoert dem Profil (2026-11-06) ──────────
    void dieAnzeigeZeileGehoertDemProfil()
    {
        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        PanadapterApplet* pan = m_mw->findChild<PanadapterApplet*>();
        QVERIFY(pan);
        const QString vorher = profile->current();
        QVERIFY(profile->create(QStringLiteral("MitZeile")));
        pan->setFrequencyReadoutShown(true);
        pan->setSwrReadoutShown(true);
        profile->captureIntoCurrent();
        const QVariantMap z = profile->snapshot(QStringLiteral("MitZeile"))
                                  .value(QStringLiteral("panReadouts")).toMap()
                                  .value(pan->panId()).toMap();
        QVERIFY2(z.value(QStringLiteral("frequency")).toBool() && z.value(QStringLiteral("swr")).toBool(),
                 "beide Haken stehen im Profil");

        // Ein Profil, das sie nicht nennt, zeigt sie nicht.
        QVERIFY(profile->activate(vorher));
        QTest::qWait(300);
        QVERIFY(!pan->frequencyReadoutShown() && !pan->swrReadoutShown());
        QVERIFY(profile->activate(QStringLiteral("MitZeile")));
        QTest::qWait(300);
        QVERIFY(pan->frequencyReadoutShown() && pan->swrReadoutShown());
        QVERIFY(profile->activate(vorher));
    }

    // ── Fall: zwei Combined-Felder (2026-11-06) ─────────────────────────
    //
    // Betreiber: "ich will auch ein 2tes combined fenster oeffnen!!" Zwei
    // Felder, jedes mit eigenem Inhalt im Profil. Ein Panel hat nur EINEN
    // Platz: was im einen liegt, steht in der Liste des anderen nicht.
    void zweiCombinedFelder()
    {
        KombiApplet* eins = m_mw->kombiFeldForTest(1);
        KombiApplet* zwei = m_mw->kombiFeldForTest(2);
        QVERIFY(eins && zwei && eins != zwei);
        QCOMPARE(eins->appletId(), QStringLiteral("Kombi"));
        QCOMPARE(zwei->appletId(), QStringLiteral("Kombi2"));
        QCOMPARE(zwei->appletTitle(), QStringLiteral("Combined 2"));
        QCOMPARE(eins->appletTitle(), QStringLiteral("Combined"));
        QVERIFY(KombiApplet::alle().contains(zwei));

        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);

        const QString chatEintrag = QStringLiteral("Contest · Chat");
        QVERIFY2(m_mw->kombiListeForTest(eins).contains(chatEintrag), "Chat steht zur Wahl");
        QVERIFY2(m_mw->kombiListeForTest(zwei).contains(chatEintrag), "in beiden Listen");
        QVERIFY2(!m_mw->kombiListeForTest(eins).contains(QStringLiteral("Combined 2")),
                 "ein Feld legt man nicht in ein Feld");

        m_mw->holeContestInsKombiFeldForTest(QStringLiteral("chat"), /*zweites=*/true);
        QTest::qWait(200);
        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke);
        QWidget* chat = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(zwei->seiten().contains(chat));
        QVERIFY(!eins->seiten().contains(chat));
        QVERIFY2(!m_mw->kombiListeForTest(eins).contains(chatEintrag),
                 "was im zweiten Feld liegt, steht im ersten nicht zur Wahl");

        // Es steht dort mit seinem Ort, und ein Klick holt es herueber
        // (Betreiber 2026-11-06, "sehe hier nicht den chat").
        const QString ausZwei = QStringLiteral("Chat (in Combined 2)");
        QVERIFY2(m_mw->kombiListeForTest(eins).contains(ausZwei),
                 "das Panel aus dem anderen Feld steht zur Wahl");
        QVERIFY(m_mw->waehleKombiEintragForTest(eins, ausZwei));
        QTest::qWait(100);
        QVERIFY2(eins->seiten().contains(chat), "der Klick holt es ins erste Feld");
        QVERIFY2(!zwei->seiten().contains(chat), "ein Panel hat nur einen Platz");
        QVERIFY2(chat->isVisibleTo(eins), "und es ist dort zu sehen");
        QVERIFY(m_mw->kombiListeForTest(zwei).contains(QStringLiteral("Chat (in Combined)")));
        QVERIFY(m_mw->waehleKombiEintragForTest(zwei, QStringLiteral("Chat (in Combined)")));
        QTest::qWait(100);
        QVERIFY2(zwei->seiten().contains(chat), "und wieder zurueck");
        QVERIFY(!eins->seiten().contains(chat));

        // Jedes Feld hat seinen eigenen Eintrag im Profil.
        profile->captureIntoCurrent();
        const QVariantMap erfasst = profile->snapshot(profile->current());
        QVERIFY(erfasst.value(QStringLiteral("kombiInhalt2")).toStringList()
                    .contains(QStringLiteral("Contest:chat")));
        QVERIFY(!erfasst.value(QStringLiteral("kombiInhalt")).toStringList()
                     .contains(QStringLiteral("Contest:chat")));

        // Ein Profilwechsel leert BEIDE und fuellt jedes aus seinem Eintrag.
        QVariantMap anders = erfasst;
        anders.insert(QStringLiteral("kombiInhalt"), QStringList{QStringLiteral("Contest:chat")});
        anders.insert(QStringLiteral("kombiInhalt2"), QStringList{QStringLiteral("Contest:map")});
        anders.insert(QStringLiteral("contestPanels"), QVariantMap());
        anders.insert(QStringLiteral("contestLogbook"), false);
        QVERIFY(profile->createWith(QStringLiteral("ZweiFelder"), anders));
        QVERIFY(profile->activate(QStringLiteral("ZweiFelder")));
        QTest::qWait(800);
        QWidget* karte = bruecke->panelFenster(QStringLiteral("map"));
        QVERIFY2(eins->seiten().contains(chat), "chat wechselt ins erste Feld");
        QVERIFY2(zwei->seiten().contains(karte), "die Karte ins zweite");
        QVERIFY(!zwei->seiten().contains(chat));

        // Eines ohne Eintrag fuer das zweite Feld (altes Profil): es wird leer.
        QVariantMap altes = anders;
        altes.remove(QStringLiteral("kombiInhalt2"));
        altes.insert(QStringLiteral("kombiInhalt"), QStringList());
        QVERIFY(profile->createWith(QStringLiteral("OhneZweites"), altes));
        QVERIFY(profile->activate(QStringLiteral("OhneZweites")));
        QTest::qWait(800);
        QVERIFY2(zwei->seiten().isEmpty(), "ein altes Profil laesst das zweite Feld leer");
        QVERIFY(eins->seiten().isEmpty());
    }

    // ── Fall: rueberziehen und entfernen (2026-11-06) ───────────────────
    //
    // Betreiber: "man sollte das auch wieder loeschen koennen", "oder
    // rueberziehen". Der Zug laeuft durch die echten Mausereignisse des
    // Reiters; entschieden wird beim Loslassen.
    void reiterZiehenUndEntfernen()
    {
        KombiApplet* eins = m_mw->kombiFeldForTest(1);
        KombiApplet* zwei = m_mw->kombiFeldForTest(2);
        QVERIFY(eins && zwei);
        auto* vis = m_mw->findChild<AppletVisibilityController*>();
        QVERIFY(vis);
        for (KombiApplet* f : {eins, zwei}) {
            vis->setVisible(f->appletId(), true);
            f->window()->show();
            f->show();
        }
        QTest::qWait(300);
        QVERIFY2(eins->isVisible() && zwei->isVisible(), "die Ziele muessen zu sehen sein");

        m_mw->holeContestInsKombiFeldForTest(QStringLiteral("chat"), /*zweites=*/false);
        QTest::qWait(200);
        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke);
        QWidget* chat = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(eins->seiten().contains(chat));

        const auto reiterVon = [](KombiApplet* f, const QString& text) -> QPushButton* {
            for (QPushButton* b : f->findChildren<QPushButton*>()) {
                if (b->text() == text && b->property("kombiSeite").isValid()) { return b; }
            }
            return nullptr;
        };
        const auto zieheNach = [](QPushButton* reiter, const QPoint& ziel) {
            const QPoint start = reiter->rect().center();
            const QPoint gStart = reiter->mapToGlobal(start);
            QMouseEvent druck(QEvent::MouseButtonPress, QPointF(start), QPointF(gStart),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(reiter, &druck);
            QMouseEvent zug(QEvent::MouseMove, QPointF(start), QPointF(ziel),
                            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(reiter, &zug);
            QMouseEvent los(QEvent::MouseButtonRelease, QPointF(start), QPointF(ziel),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(reiter, &los);
        };

        // Es zaehlt, was unter dem Zeiger oben liegt. Fenster aus den
        // Faellen davor duerfen die Felder nicht verdecken. Wegruecken,
        // nicht verstecken: ein verstecktes Fenster stuerzt beim Abbau
        // unter "offscreen" in QOffscreenBackingStore::clearHash() ab.
        for (QWidget* t : QApplication::topLevelWidgets()) {
            if (t != eins->window() && t != zwei->window() && t->isVisible()) {
                t->move(5000, 5000);
            }
        }
        QTest::qWait(100);

        // Im eigenen Feld losgelassen: nichts geschieht.
        QPushButton* reiter = reiterVon(eins, QStringLiteral("Chat"));
        QVERIFY(reiter);
        zieheNach(reiter, eins->mapToGlobal(QPoint(eins->width() / 2, eins->height() / 2)));
        QTest::qWait(100);
        QVERIFY2(eins->seiten().contains(chat), "im eigenen Feld bleibt es");

        // Ueber dem anderen Feld losgelassen: es wechselt.
        reiter = reiterVon(eins, QStringLiteral("Chat"));
        QVERIFY(reiter);
        zieheNach(reiter, zwei->mapToGlobal(QPoint(zwei->width() / 2, zwei->height() / 2)));
        QTest::qWait(100);
        QVERIFY2(zwei->seiten().contains(chat), "rueberziehen legt es ins andere Feld");
        QVERIFY(!eins->seiten().contains(chat));

        // Auch aus dem Menue des Reiters.
        QVERIFY(m_mw->waehleReiterEintragForTest(zwei, chat, QStringLiteral("Move to Combined")));
        QVERIFY(eins->seiten().contains(chat));

        // Remove: raus aus dem Feld, kein Fenster, wieder waehlbar.
        QVERIFY(m_mw->waehleReiterEintragForTest(eins, chat, QStringLiteral("Remove")));
        QTest::qWait(100);
        QVERIFY(!eins->seiten().contains(chat));
        QVERIFY(!bruecke->panelInFremdemGehaeuse(QStringLiteral("chat")));
        QVERIFY2(!chat->isWindow(), "es bleibt kein Fenster zurueck");
        QVERIFY2(m_mw->kombiListeForTest(eins).contains(QStringLiteral("Contest · Chat")),
                 "es steht wieder zur Wahl");

        // Ein Applet wechselt das Feld und geht auf demselben Weg.
        const QString id = QStringLiteral("Frequency");
        vis->setVisible(id, true);
        QVERIFY(m_mw->waehleKombiEintragForTest(eins, QStringLiteral("Frequency")));
        QTest::qWait(100);
        QCOMPARE(eins->seiten().size(), 1);
        QWidget* applet = eins->seiten().first();
        QVERIFY(m_mw->waehleKombiEintragForTest(zwei, QStringLiteral("Frequency (in Combined)")));
        QTest::qWait(100);
        QVERIFY2(zwei->seiten().contains(applet), "das Applet wechselt das Feld");
        QVERIFY2(applet->parentWidget() && zwei->isAncestorOf(applet),
                 "und die Spalte haelt es nicht fest");
        QVERIFY(m_mw->waehleReiterEintragForTest(zwei, applet, QStringLiteral("Remove")));
        QTest::qWait(100);
        QVERIFY(!zwei->seiten().contains(applet));
        QVERIFY2(!vis->isVisible(id), "ein entferntes Applet ist aus");
        vis->setVisible(id, true);
    }

    // ── Fall 7: "kein Contest" bleibt im Profil stehen (2026-11-06) ─────
    //
    // Betreiber 05:49: "so nicht abgespeichert" -- das Logbuch war beim
    // Start offen, obwohl das Profil "Logbuch zu" sagte. Beim Beenden
    // einer Sitzung ohne geladenes Logbuch schrieb die Erfassung die
    // Contest-Eintraege nicht mehr; das Profil verlor seine Aussage.
    void keinContestBleibtImProfilStehen()
    {
        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        QVariantMap zustand = profile->snapshot(profile->current());
        zustand.insert(QStringLiteral("contestPanels"), QVariantMap());
        zustand.insert(QStringLiteral("contestLogbook"), false);
        QVERIFY(profile->createWith(QStringLiteral("OhneContest"), zustand));
        QVERIFY(profile->activate(QStringLiteral("OhneContest")));
        QTest::qWait(500);

        // Wie eine Sitzung, in der das Logbuch nie geladen wurde.
        if (ContestFenster* b = m_mw->contestFensterForTest()) { b->schliesse(); QVERIFY(!b->istOffen()); }
        profile->captureIntoCurrent();
        const QVariantMap erfasst = profile->snapshot(QStringLiteral("OhneContest"));
        QVERIFY2(erfasst.contains(QStringLiteral("contestPanels")),
                 "die Aussage 'kein Contest' muss im Profil bleiben");
        QCOMPARE(erfasst.value(QStringLiteral("contestLogbook")).toBool(), false);

        // Ein Profil ganz ohne Contest-Eintrag (von vor dem 2026-11-05)
        // bekommt mit der Erfassung "kein Contest, Logbuch zu".
        QVariantMap alt = profile->snapshot(profile->current());
        alt.remove(QStringLiteral("contestPanels"));
        alt.remove(QStringLiteral("contestLogbook"));
        QVERIFY(profile->createWith(QStringLiteral("Altes"), alt));
        QVERIFY(profile->activate(QStringLiteral("Altes")));
        QTest::qWait(500);
        profile->captureIntoCurrent();
        const QVariantMap altErfasst = profile->snapshot(QStringLiteral("Altes"));
        QVERIFY2(altErfasst.contains(QStringLiteral("contestPanels")),
                 "auch ein altes Profil sagt nach der Erfassung 'kein Contest'");
        QCOMPARE(altErfasst.value(QStringLiteral("contestLogbook")).toBool(), false);
    }

    // ── Schliessen und sofort wieder oeffnen (2026-11-06) ────────────────
    //
    // Das Schliessen hinterlaesst einen Aufraeum-Auftrag fuer die naechste
    // Runde. Oeffnet das Logbuch vorher wieder, durfte er ihm nicht die
    // Datenbank wegnehmen. Das Signal macht den Fall scharf: der Zugriff
    // auf die tote Datenbank stuerzte nur in jedem zwoelften Lauf ab.
    void gleichWiederOeffnenBehaeltDieDatenbank()
    {
        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke);
        m_mw->holeContestInsKombiFeldForTest(QStringLiteral("map"), /*zweites=*/false);
        QTest::qWait(100);
        QSignalSpy geschlossen(bruecke, &ContestFenster::geschlossen);
        bruecke->schliesse();
        QString grund;
        QVERIFY2(bruecke->stelleAnordnungHer({}, false, &grund, m_mw), qPrintable(grund));
        QVERIFY(bruecke->istOffen());
        QTest::qWait(100);
        QCOMPARE(geschlossen.size(), 0);
        QVERIFY2(bruecke->istOffen(), "das neue Logbuch muss offen bleiben");
    }

    // ── Letzter Fall: Beenden mit dem Log im Combined (2026-11-06) ───────
    //
    // Am 2026-11-05 stuerzte Longpath drei Mal im Abbau des Logbuchs ab.
    // Liegt das Log in einem fremden Gehaeuse, darf der Abbau es nicht
    // doppelt treffen. Muss der letzte Fall bleiben: er schliesst das
    // Fenster, das alle anderen brauchen, und raeumt damit zugleich ab,
    // was sonst den Qt-eigenen Offscreen-Absturz im Abbau ausloest.
    void beendenMitDemLogImCombined()
    {
        KombiApplet* eins = m_mw->kombiFeldForTest(1);
        QVERIFY(eins);
        m_mw->holeContestInsKombiFeldForTest(QStringLiteral("unifiedlog"), /*zweites=*/false);
        QTest::qWait(300);
        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY(bruecke);
        QVERIFY(eins->seiten().contains(bruecke->panelFenster(QStringLiteral("unifiedlog"))));

        QVERIFY(m_mw->close());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::closeAllWindows();
        QTest::qWait(300);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(TestContestPanelsImContainersMenu)
#include "tst_contest_panels_im_containers_menu.moc"
