// tests/tst_contestfenster.cpp  (Longpath)
// no-port-check: Longpath-original test file.
//
// Das Contestprogramm als Fenster in Longpath (2026-11-04). Geprueft wird
// die Bruecke allein, ohne MainWindow: ein eigener Datenordner
// (CONTESTPROGRAMM_DATA_DIR), Fenster auf, zu, wieder auf -- und dass ein
// zweiter Schreiber auf demselben Log abgewiesen wird, so wie das
// eigenstaendige Programm abgewiesen wuerde, wenn das Fenster hier offen
// ist (dieselbe Einzelplatz-Wache, dasselbe Verzeichnis).

#include <QtTest>
#include <QPushButton>
#include <QPointer>
#include <QSignalSpy>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QWidget>

#include "ContestFenster.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestContestFenster : public QObject
{
    Q_OBJECT

private:
    // Ein leeres Log oeffnet beim ersten Mal modal die Einstellungen (kein
    // Rufzeichen) und greift nach rigctld und Rotoren. Hier wird das Log
    // vorbelegt wie in test_vorgabe_ueberlappt_nicht des Contestprogramms
    // -- ueber SQLite direkt, damit dieser Pruefstand keine Kopfdatei des
    // Contestprogramms braucht (siehe ContestFenster.h zu den Kollisionen).
    static bool belegeLogVor(const QString& ordner)
    {
        const QString name = QStringLiteral("vorbelegung");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(ordner + QStringLiteral("/contestprogramm.sqlite"));
            if (!db.open()) { return false; }
            QSqlQuery q(db);
            if (!q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS settings "
                                       "(key TEXT PRIMARY KEY, value TEXT)"))) {
                return false;
            }
            const QList<QPair<QString, QString>> werte = {
                {QStringLiteral("own_callsign"),     QStringLiteral("OE5SOS")},
                {QStringLiteral("own_grid"),         QStringLiteral("JN67UT")},
                {QStringLiteral("rigctld_host"),     QString()},
                {QStringLiteral("rotor1_enabled"),   QStringLiteral("0")},
                {QStringLiteral("rotor2_enabled"),   QStringLiteral("0")},
                {QStringLiteral("esm_enabled"),      QStringLiteral("0")},
                {QStringLiteral("spectrum_enabled"), QStringLiteral("0")},
            };
            for (const auto& kv : werte) {
                q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
                q.addBindValue(kv.first);
                q.addBindValue(kv.second);
                if (!q.exec()) { return false; }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        return true;
    }

private slots:
    void dasVerzeichnisFolgtDerUmgebung()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QCOMPARE(ContestFenster::datenverzeichnis(), ordner.path());
    }

    void aufZuUndWiederAuf()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        ContestFenster bruecke;
        QSignalSpy auf(&bruecke, &ContestFenster::geoeffnet);
        QSignalSpy zu(&bruecke, &ContestFenster::geschlossen);
        QVERIFY(!bruecke.istOffen());

        QString grund;
        QVERIFY2(bruecke.oeffne(&grund), qPrintable(grund));
        QVERIFY(bruecke.istOffen());
        QWidget* fenster = bruecke.fensterForTest();
        QVERIFY(fenster);
        QVERIFY(QTest::qWaitForWindowExposed(fenster, kFensterFrist));
        QVERIFY(fenster->isVisible());
        QCOMPARE(auf.count(), 1);

        // Die Datenbank liegt im eigenen Ordner, nicht beim echten Log.
        QVERIFY(QFile::exists(ordner.path() + QStringLiteral("/contestprogramm.sqlite")));

        // Ein zweiter Aufruf holt nur nach vorn, baut nichts Zweites.
        QVERIFY(bruecke.oeffne(&grund));
        QCOMPARE(bruecke.fensterForTest(), fenster);
        QCOMPARE(auf.count(), 1);

        bruecke.schliesse();
        QVERIFY(!bruecke.istOffen());
        QVERIFY(bruecke.fensterForTest() == nullptr);

        QVERIFY2(bruecke.oeffne(&grund), qPrintable(grund));
        QVERIFY(bruecke.istOffen());
        QCOMPARE(auf.count(), 2);
        bruecke.schliesse();
    }

    // ── Mit Elternfenster: dieselbe Ebene wie die Panels ─────────────
    //
    // Betreiber 2026-11-04: "wenn ich contestprogramm oeffne, soll das nach
    // anklicken im vordergrund bleiben!" Longpaths Panels sind Qt::Tool mit
    // dem Hauptfenster als Eltern; ein gewoehnliches Fenster liegt auf macOS
    // immer darunter. Mit Elternfenster wird das Logbuch ein Qt::Tool und
    // bleibt sichtbar, wenn das Programm nicht vorn ist.
    void mitElternfensterLiegtEsAufDerEbeneDerPanels()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        QWidget eltern;
        eltern.resize(800, 600);
        eltern.show();
        QVERIFY(QTest::qWaitForWindowExposed(&eltern, kFensterFrist));

        ContestFenster bruecke;
        QString grund;
        QVERIFY2(bruecke.oeffne(&grund, &eltern), qPrintable(grund));
        QWidget* fenster = bruecke.fenster();
        QVERIFY(fenster);
        QCOMPARE(fenster->parentWidget(), &eltern);
        QCOMPARE(fenster->windowFlags() & Qt::WindowType_Mask, Qt::Tool);
        QVERIFY2(fenster->testAttribute(Qt::WA_MacAlwaysShowToolWindow),
                 "Das Logbuch muss stehen bleiben, wenn Longpath nicht vorn ist");
        QVERIFY(fenster->isWindow());
        QVERIFY(QTest::qWaitForWindowExposed(fenster, kFensterFrist));
        bruecke.schliesse();

        // Ohne Elternfenster bleibt es ein gewoehnliches Fenster.
        QVERIFY2(bruecke.oeffne(&grund), qPrintable(grund));
        QCOMPARE(bruecke.fenster()->parentWidget(), nullptr);
        QVERIFY((bruecke.fenster()->windowFlags() & Qt::WindowType_Mask) != Qt::Tool);
        bruecke.schliesse();
    }

    // ── Panels einzeln, ohne das Logbuch-Fenster ──────────────────────
    //
    // Betreiber 2026-11-04: "jedes panel vom contestprogramm sollte in
    // longpath eigens hinzuzufuegen sein, das waere die beste version."
    // Das Panel kommt als eigenes Fenster, haengt ueber das (verborgene)
    // Logbuch am Elternfenster, hat eine greifbare Groesse und steht
    // neben dem Elternfenster; das Logbuch selbst bleibt zu.
    void einPanelOhneLogbuchFenster()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        QWidget eltern;
        eltern.resize(800, 600);
        eltern.show();
        QVERIFY(QTest::qWaitForWindowExposed(&eltern, kFensterFrist));

        const QList<ContestFenster::PanelEintrag> panels = ContestFenster::panels();
        QVERIFY(panels.size() >= 8);
        bool chatDabei = false;
        for (const ContestFenster::PanelEintrag& e : panels) {
            QVERIFY(!e.id.isEmpty() && !e.titel.isEmpty());
            chatDabei = chatDabei || e.id == QStringLiteral("chat");
        }
        QVERIFY(chatDabei);

        ContestFenster bruecke;
        QString grund;
        QVERIFY(!bruecke.istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("chat"), &grund, &eltern),
                 qPrintable(grund));
        QWidget* chat = bruecke.panelFenster(QStringLiteral("chat"));
        QVERIFY(chat);
        QVERIFY(bruecke.istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY(chat->isWindow());
        QCOMPARE(chat->windowFlags() & Qt::WindowType_Mask, Qt::Tool);
        QVERIFY(QTest::qWaitForWindowExposed(chat, kFensterFrist));
        QVERIFY2(chat->width() >= 160 && chat->height() >= 120,
                 qPrintable(QStringLiteral("Panel kam %1x%2 daher")
                                .arg(chat->width()).arg(chat->height())));
        // Haengt DIREKT am Elternfenster, als Geschwister von Longpaths
        // eigenen Panels (2026-11-04: "nicht alle offenen fenster lassen
        // sich durch anklicken in den vordergrund schieben" -- zwei
        // Geschwisterkreise, jetzt einer). Das Logbuch haengt ebenfalls dort.
        QWidget* log = bruecke.fenster();
        QVERIFY(log);
        QCOMPARE(chat->parentWidget(), &eltern);
        QCOMPARE(log->parentWidget(), &eltern);
        QVERIFY2(!log->isVisible(), "das Logbuch-Fenster bleibt zu");
        // Neben dem Elternfenster, nicht bei (0,0).
        const QPoint ecke = chat->frameGeometry().topLeft();
        QVERIFY2(ecke.x() >= eltern.frameGeometry().x() + 32
                     && ecke.y() >= eltern.frameGeometry().y() + 32,
                 qPrintable(QStringLiteral("Panel steht bei (%1,%2), Eltern bei (%3,%4)")
                                .arg(ecke.x()).arg(ecke.y())
                                .arg(eltern.frameGeometry().x()).arg(eltern.frameGeometry().y())));

        // Ein zweites Panel staffelt sich dahinter.
        QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("bandmap"), &grund, &eltern),
                 qPrintable(grund));
        QWidget* bandmap = bruecke.panelFenster(QStringLiteral("bandmap"));
        QVERIFY(bandmap && bandmap->isWindow());
        QVERIFY(bandmap->frameGeometry().topLeft() != chat->frameGeometry().topLeft());

        // Zurueck ins Logbuch: kein Fenster mehr.
        bruecke.dockePanel(QStringLiteral("chat"));
        QVERIFY(!bruecke.istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY(!chat->isWindow());

        // Jetzt das Logbuch zeigen: dasselbe Fenster, kein zweites.
        QVERIFY2(bruecke.oeffne(&grund, &eltern), qPrintable(grund));
        QCOMPARE(bruecke.fenster(), log);
        QVERIFY(log->isVisible());
        bruecke.schliesse();
    }

    // ── Aufs Raster wie Longpaths eigene Fenster ─────────────────────
    //
    // Betreiber 2026-11-05, mit Bild: "hier funktioniert der raster nicht
    // mit der anordnung der fenster". Ein abgeloestes Panel, das der
    // Bediener zieht (drei Lagen nacheinander, wie beim Ziehen), muss nach
    // dem Loslassen auf dem 16er-Raster stehen -- wie PanFloatingWindow.
    void einPanelRastetNachDemZugEin()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        QWidget eltern;
        eltern.resize(800, 600);
        eltern.show();
        QVERIFY(QTest::qWaitForWindowExposed(&eltern, kFensterFrist));

        ContestFenster bruecke;
        QString grund;
        QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("chat"), &grund, &eltern),
                 qPrintable(grund));
        QWidget* chat = bruecke.panelFenster(QStringLiteral("chat"));
        QVERIFY(chat && chat->isWindow());
        QVERIFY(QTest::qWaitForWindowExposed(chat, kFensterFrist));
        QTest::qWait(400);   // ein etwaiger Rasterlauf des Aufbaus ist vorbei

        // Ziehen: drei Lagen, alle neben dem Raster.
        const QPoint ziel(chat->x() + 5, chat->y() + 7);
        chat->move(ziel - QPoint(4, 4));
        chat->move(ziel - QPoint(2, 2));
        chat->move(ziel);
        QCOMPARE(chat->frameGeometry().topLeft(), ziel);
        QVERIFY(ziel.x() % 16 != 0 || ziel.y() % 16 != 0);

        // Nach der Ruhe (180 ms Daempfung) liegt der Rahmen auf dem Raster.
        QTRY_VERIFY2_WITH_TIMEOUT(chat->frameGeometry().x() % 16 == 0
                                      && chat->frameGeometry().y() % 16 == 0,
                                  qPrintable(QStringLiteral("steht bei (%1,%2)")
                                                 .arg(chat->frameGeometry().x())
                                                 .arg(chat->frameGeometry().y())),
                                  3000);
        bruecke.schliesse();
    }

    // ── Die Anordnung kommt aus dem Profil zurueck (2026-11-05) ─────
    //
    // Betreiber nach einem Neustart: "so war mein letzter zustand
    // nicht!!!!" Zwei Panels als Fenster an bekannten Stellen, Logbuch
    // zu -> Werte fuer das Profil -> alles abbauen -> aus den Werten
    // wiederherstellen: dieselben Fenster an denselben Stellen, Logbuch
    // weiter zu. Die Rahmen liegen auf dem Raster (16er-Linien, 3 Luft),
    // damit der Rasterlauf nach dem Herstellen nichts verschiebt.
    void dieAnordnungKommtAusDemProfilZurueck()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        QWidget eltern;
        eltern.resize(800, 600);
        eltern.show();
        QVERIFY(QTest::qWaitForWindowExposed(&eltern, kFensterFrist));

        const QRect chatSoll(128, 144, 397, 301);
        const QRect bandmapSoll(560, 144, 301, 237);
        QVariantMap gesichert;
        {
            ContestFenster bruecke;
            QString grund;
            QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("chat"), &grund, &eltern),
                     qPrintable(grund));
            QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("bandmap"), &grund, &eltern),
                     qPrintable(grund));
            bruecke.panelFenster(QStringLiteral("chat"))->setGeometry(chatSoll);
            bruecke.panelFenster(QStringLiteral("bandmap"))->setGeometry(bandmapSoll);
            QTest::qWait(500);
            QVERIFY(!bruecke.logbuchSichtbar());
            gesichert = bruecke.anordnungDerPanels();
            QCOMPARE(gesichert.size(), 2);
            const QVariantMap c = gesichert.value(QStringLiteral("chat")).toMap();
            QCOMPARE(QRect(c.value("x").toInt(), c.value("y").toInt(),
                           c.value("w").toInt(), c.value("h").toInt()), chatSoll);
            bruecke.schliesse();
        }

        ContestFenster neu;
        QString grund;
        QVERIFY2(neu.stelleAnordnungHer(gesichert, false, &grund, &eltern), qPrintable(grund));
        QTest::qWait(500);
        QVERIFY(neu.istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY(neu.istPanelSchwebend(QStringLiteral("bandmap")));
        QVERIFY(!neu.istPanelSchwebend(QStringLiteral("map")));
        QCOMPARE(neu.panelFenster(QStringLiteral("chat"))->frameGeometry(), chatSoll);
        QCOMPARE(neu.panelFenster(QStringLiteral("bandmap"))->frameGeometry(), bandmapSoll);
        QVERIFY2(!neu.logbuchSichtbar(), "das Logbuch-Fenster bleibt zu");

        // Ein Profil ohne Contest raeumt auf, laesst aber das Logbuch leben.
        neu.raeumeAnordnung();
        QVERIFY(!neu.istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY(neu.istOffen());

        // Und mit sichtbarem Logbuch kommt es sichtbar zurueck.
        QVERIFY2(neu.stelleAnordnungHer(QVariantMap(), true, &grund, &eltern), qPrintable(grund));
        QVERIFY(neu.logbuchSichtbar());
        neu.schliesse();
    }

    // ── Das ✕ am eingebetteten Logbuch versteckt nur ────────────────
    //
    // Vorher: WA_DeleteOnClose -> Controller fort, die schwebenden Panels
    // (Geschwister am Hauptfenster) blieben mit totem Controller stehen,
    // und ihre Anordnung sicherte niemand mehr.
    void dasKreuzAmLogbuchVerstecktNur()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        QWidget eltern;
        eltern.resize(800, 600);
        eltern.show();
        QVERIFY(QTest::qWaitForWindowExposed(&eltern, kFensterFrist));

        ContestFenster bruecke;
        QString grund;
        QVERIFY2(bruecke.oeffne(&grund, &eltern), qPrintable(grund));
        QWidget* log = bruecke.fenster();
        QVERIFY(log && log->isVisible());
        QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("chat"), &grund, &eltern),
                 qPrintable(grund));
        QSignalSpy geaendert(&bruecke, &ContestFenster::anordnungGeaendert);

        QVERIFY(!log->close());          // abgefangen: nur verstecken
        QVERIFY(bruecke.istOffen());
        QCOMPARE(bruecke.fenster(), log);
        QVERIFY(!log->isVisible());
        QVERIFY(geaendert.count() >= 1);
        // Die Panels leben weiter und lassen sich bedienen.
        QVERIFY(bruecke.istPanelSchwebend(QStringLiteral("chat")));
        bruecke.dockePanel(QStringLiteral("chat"));
        QVERIFY(!bruecke.istPanelSchwebend(QStringLiteral("chat")));
        // Tools > Contest Logbook: dieselbe Ansicht wieder.
        QVERIFY2(bruecke.oeffne(&grund, &eltern), qPrintable(grund));
        QCOMPARE(bruecke.fenster(), log);
        QVERIFY(log->isVisible());
        // schliesse() baut trotz Filter wirklich ab.
        bruecke.schliesse();
        QVERIFY(!bruecke.istOffen());
    }

    // ── ✕ am Panel: zurueck ins Logbuch und ausgeblendet ─────────────
    //
    // Betreiber 2026-11-04: "alle fenster muessen sich mit X schliessen
    // lassen!" Das ✕ (rahmenlos, in Longpath) dockt das Panel zurueck UND
    // blendet es aus; der Haken holt es wieder. Und schliesse() mit
    // schwebenden Panels darf nichts Totes zuruecklassen.
    void dasKreuzBlendetAusUndDerHakenHoltEsWieder()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));
        QWidget eltern;
        eltern.resize(800, 600);
        eltern.show();
        QVERIFY(QTest::qWaitForWindowExposed(&eltern, kFensterFrist));

        ContestFenster bruecke;
        QString grund;
        QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("chat"), &grund, &eltern),
                 qPrintable(grund));
        QWidget* chat = bruecke.panelFenster(QStringLiteral("chat"));
        QVERIFY(chat && chat->isWindow());
        QPushButton* kreuz = nullptr;
        for (QPushButton* b : chat->findChildren<QPushButton*>()) {
            if (b->objectName() == QStringLiteral("panelHeaderCloseButton")) { kreuz = b; }
        }
        QVERIFY2(kreuz && kreuz->isVisible(), "das ✕ muss in der Kopfzeile stehen");
        kreuz->click();
        QVERIFY(!bruecke.istPanelSchwebend(QStringLiteral("chat")));
        QVERIFY2(!chat->isVisible(), "nach ✕ ist das Panel weg");

        // Der Haken (zeigePanelAlsFenster) holt es wieder.
        QVERIFY2(bruecke.zeigePanelAlsFenster(QStringLiteral("chat"), &grund, &eltern),
                 qPrintable(grund));
        QVERIFY(chat->isWindow());
        QTRY_VERIFY_WITH_TIMEOUT(chat->isVisible(), 3000);

        // Abbau mit schwebendem Panel: kein Absturz, nichts bleibt stehen.
        QPointer<QWidget> chatZeiger(chat);
        bruecke.schliesse();
        QVERIFY2(chatZeiger.isNull(), "das Panel muss mit dem Logbuch gehen");
        QVERIFY(!bruecke.istOffen());
    }

    void einZweiterSchreiberWirdAbgewiesen()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        ContestFenster erste;
        QString grund;
        QVERIFY2(erste.oeffne(&grund), qPrintable(grund));

        // Dieselbe Wache, dasselbe Verzeichnis: so saehe es aus, wenn das
        // eigenstaendige Contestprogramm schon liefe.
        ContestFenster zweite;
        QVERIFY2(!zweite.oeffne(&grund),
                 "Zwei Schreiber auf demselben Log -- genau das darf nicht sein");
        QVERIFY2(grund.contains(QStringLiteral("already running")), qPrintable(grund));
        QVERIFY(!zweite.istOffen());

        erste.schliesse();
        // Nach dem Schliessen ist der Platz frei.
        QVERIFY2(zweite.oeffne(&grund), qPrintable(grund));
        zweite.schliesse();
    }
};

QTEST_MAIN(TestContestFenster)
#include "tst_contestfenster.moc"
