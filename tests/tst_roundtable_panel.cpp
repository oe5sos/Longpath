// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_roundtable_panel.cpp  (Longpath)
// =================================================================
//
// Roundtable Stufe 2: das Panel (RoundtablePanel im RoundtableApplet) und
// sein eigenes Fenster (RoundtableWindow), gefahren ueber die Bedienung —
// Klicks und Tasten, nicht die Methoden dahinter. Vorbild
// tst_roundtable_spothub.
//
//   vorDerAntwort            Vor der Erstantwort sagt das Panel, wo sie
//                            steht (Settings fuehrt in den Spot Hub), und
//                            nichts geht hinaus: keine Verbindung, kein
//                            Schluessel.
//   einRaumOhneVerbindung    Ein Raum aus der Raumansicht: Sprecher,
//                            Messwerte, Nachrichten; das eigene Fenster
//                            per Klick auf und wieder zurueck.
//   livePerKlick             Gegen Upstreams Referenzserver auf 127.0.0.1
//                            (wie tst_roundtable_e2e, sonst
//                            uebersprungen): Senden per Enter, "You · 1
//                            hear you" beim Senden, einen Durchgang ohne
//                            bekannten Sprecher per Klick benennen.
//
// Mit LONGPATH_BILD_DIR legt jeder Fall Bilder seiner Ansicht ab (offscreen
// fehlt das App-Stylesheet, die Flaechen des Panels malt es selbst).
//
// Modification history (Longpath):
//   2026-11-18 - Angelegt fuer Longpath (Roundtable Stufe 2) von Martin
//                Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
//                Die Objektnamen und Erwartungen an die Texte folgen
//                Upstreams tests/tst_roundtable_panel.cpp @ e56b3b44c.
// =================================================================

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableKey.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "models/RoundtableHub.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest>

#include <memory>

using namespace Longpath;

namespace {

constexpr int kStartMs = 20000;
constexpr int kSeeMs = 30000;
const QString kRoom = QStringLiteral("lsb-7088000");

void settle()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

QStringList texts(const QWidget* root, const QString& name)
{
    QStringList out;
    for (const QLabel* l : root->findChildren<QLabel*>(name)) {
        if (l->isVisibleTo(root->window())) {
            out.append(l->text());
        }
    }
    return out;
}

void bild(QWidget& w, const QString& name)
{
    const QString dir = QString::fromLocal8Bit(qgetenv("LONGPATH_BILD_DIR"));
    if (dir.isEmpty()) {
        return;
    }
    QDir().mkpath(dir);
    settle();
    w.grab().save(dir + QLatin1Char('/') + name);
}

quint16 freePort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost, 0);
    return s.serverPort();
}

QJsonObject member(const QString& ref, const QString& call, const QString& grid, double dbm,
                   qint64 t, bool you = false)
{
    QJsonObject m{{QStringLiteral("ref"), ref},      {QStringLiteral("callsign"), call},
                  {QStringLiteral("nth"), 1},        {QStringLiteral("grid"), grid},
                  {QStringLiteral("you"), you},      {QStringLiteral("away"), false},
                  {QStringLiteral("joining"), false}, {QStringLiteral("keyed"), false},
                  {QStringLiteral("hz"), 7088000},   {QStringLiteral("slice"), 0}};
    m.insert(QStringLiteral("reading"),
             dbm == 0.0 ? QJsonValue(QJsonValue::Null)
                        : QJsonValue(QJsonObject{{QStringLiteral("dbm"), dbm},
                                                 {QStringLiteral("t"), t},
                                                 {QStringLiteral("present"), true}}));
    return m;
}

} // namespace

class TstRoundtablePanel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    void init()
    {
        auto& s = AppSettings::instance();
        for (const char* k : {"Roundtable/AutoStart", "Roundtable/FirstRunAnswer", "Roundtable/Hidden",
                              "Roundtable/MuteList", "Roundtable/MentionsSeenMs",
                              "Roundtable/WindowGeometry"}) {
            s.remove(QLatin1String(k));
        }
        s.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("OE5SOS"));
        s.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("JN78"));
        // Nie zu roundtable.nereussdr.com: hier lauscht niemand.
        s.setValue(QStringLiteral("Roundtable/ServerUrl"), QStringLiteral("ws://127.0.0.1:9/v1/station"));
        RoundtableModel::setClockForTest({});
    }

    void vorDerAntwort()
    {
        QTemporaryDir dir;
        const QString keyPath = dir.filePath(QStringLiteral("roundtable-identity.pem"));
        RoundtableHub hub;
        hub.setKeyPathForTest(keyPath);
        hub.reloadSettings();
        SliceModel slice(0);
        slice.setSliceIndex(0);
        slice.setFrequency(7088100.0);
        slice.setDspMode(DSPMode::LSB);
        RoundtableApplet applet(nullptr, &hub, nullptr);
        applet.setSliceSource([&slice]() { return &slice; });
        applet.resize(300, 640);
        applet.show();
        settle();

        auto* line = applet.findChild<QLabel*>(QStringLiteral("roundtableConnectionLine"));
        QVERIFY(line);
        QVERIFY2(line->text().contains(QStringLiteral("answer in Settings")), qPrintable(line->text()));
        QCOMPARE(hub.client()->linkState(), RoundtableClient::LinkState::Off);
        QVERIFY(!QFile::exists(keyPath));
        bild(applet, QStringLiteral("panel-vor-der-antwort.png"));

        // Settings im Menue des Panels: dort fuehrt es in den Spot Hub.
        QSignalSpy settings(&applet, &RoundtableApplet::settingsRequested);
        auto* menuBtn = applet.findChild<QToolButton*>(QStringLiteral("roundtableMenuBtn"));
        QVERIFY(menuBtn);
        QTest::mouseClick(menuBtn, Qt::LeftButton);
        settle();
        QMenu* menu = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* m = qobject_cast<QMenu*>(w); m != nullptr && m->isVisible()) {
                menu = m;
            }
        }
        QVERIFY(menu);
        QAction* item = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text() == QLatin1String("Settings")) {
                item = a;
            }
        }
        QVERIFY(item);
        item->trigger();
        menu->close();
        QCOMPARE(settings.size(), 1);
        // Und noch immer nichts hinaus.
        QCOMPARE(hub.client()->linkState(), RoundtableClient::LinkState::Off);
        QVERIFY(!QFile::exists(keyPath));
    }

    void einRaumOhneVerbindung()
    {
        const qint64 now = RoundtableModel::nowMs();
        RoundtableHub hub;
        RoundtableSliceState state;
        state.id = 0;
        state.hz = 7088100;
        state.mode = QStringLiteral("LSB");
        state.filterHz = 2700;
        hub.setSlicesForTest({state});
        SliceModel slice(0);
        slice.setSliceIndex(0);
        slice.setFrequency(7088100.0);
        slice.setDspMode(DSPMode::LSB);

        emit hub.client()->roomsReceived(QJsonObject{
            {QStringLiteral("rooms"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), kRoom},
                                    {QStringLiteral("family"), QStringLiteral("lsb")},
                                    {QStringLiteral("hz"), 7088000},
                                    {QStringLiteral("members"), 3}}}}});
        QCOMPARE(hub.model()->sliceRoomId(0), kRoom);
        const QJsonObject talking{
            {QStringLiteral("id"), 41},
            {QStringLiteral("kind"), QStringLiteral("talking")},
            {QStringLiteral("talkers"),
             QJsonArray{QJsonObject{{QStringLiteral("ref"), QStringLiteral("b/0")},
                                    {QStringLiteral("callsign"), QStringLiteral("DL2XYZ")},
                                    {QStringLiteral("nth"), 1}}}},
            {QStringLiteral("start"), now - 65000},
            {QStringLiteral("end"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("ending"), false},
            {QStringLiteral("names"), QJsonArray{}}};
        const QJsonObject unknown{{QStringLiteral("id"), 40},
                                  {QStringLiteral("kind"), QStringLiteral("unknown")},
                                  {QStringLiteral("talkers"), QJsonArray{}},
                                  {QStringLiteral("start"), now - 300000},
                                  {QStringLiteral("end"), now - 280000},
                                  {QStringLiteral("names"), QJsonArray{}}};
        emit hub.client()->roomReceived(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("room")},
            {QStringLiteral("id"), kRoom},
            {QStringLiteral("members"),
             QJsonArray{member(QStringLiteral("a/0"), QStringLiteral("OE5SOS"), QStringLiteral("JN78"),
                               -73.0, now - 1000, true),
                        member(QStringLiteral("b/0"), QStringLiteral("DL2XYZ"), QStringLiteral("JN58"),
                               0.0, 0),
                        member(QStringLiteral("c/0"), QStringLiteral("OK1ABC"), QStringLiteral("JN79"),
                               -91.0, now - 2000)}},
            {QStringLiteral("now"), QJsonArray{talking}},
            {QStringLiteral("earlier"), QJsonArray{unknown}},
            {QStringLiteral("messages"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), 7},
                                    {QStringLiteral("t"), now - 30000},
                                    {QStringLiteral("callsign"), QStringLiteral("DL2XYZ")},
                                    {QStringLiteral("text"), QStringLiteral("QRV on 7088, anyone?")},
                                    {QStringLiteral("state"), QStringLiteral("live")}}}}});

        RoundtableApplet applet(nullptr, &hub, nullptr);
        applet.setSliceSource([&slice]() { return &slice; });
        applet.resize(300, 760);
        applet.show();
        settle();
        RoundtablePanel* panel = applet.panel();
        QCOMPARE(panel->roomId(), kRoom);
        QCOMPARE(texts(panel, QStringLiteral("roundtableTalkerName")), QStringList{QStringLiteral("DL2XYZ")});
        // Wie hoeren die anderen ihn: zwei Messwerte (OE5SOS, OK1ABC).
        QCOMPARE(texts(panel, QStringLiteral("roundtableReadingCall")).size(), 2);
        QVERIFY(texts(panel, QStringLiteral("roundtableMessageText")).join(QLatin1Char('|'))
                    .contains(QStringLiteral("QRV on 7088")));
        // Ohne Verbindung: kein Benennen, Senden verweigert der Hub.
        for (QPushButton* b : panel->findChildren<QPushButton*>(QStringLiteral("roundtableTurnNameBtn"))) {
            QVERIFY(!b->isEnabled() || !b->isVisible());
        }
        bild(applet, QStringLiteral("panel-raum.png"));

        // Das eigene Fenster: Klick auf den Knopf, das Panel zieht um, die
        // Spalte sagt es; "Put it back here" holt es zurueck.
        auto* pop = panel->findChild<QToolButton*>(QStringLiteral("roundtablePopBtn"));
        QVERIFY(pop);
        QTest::mouseClick(pop, Qt::LeftButton);
        settle();
        RoundtableWindow* window = applet.floatingWindow();
        QVERIFY(window);
        QVERIFY(window->isVisible());
        QVERIFY(applet.isPoppedOut());
        auto* list = window->findChild<QListWidget*>(QStringLiteral("roundtableRoomList"));
        QVERIFY(list);
        // Eine Bandzeile (40M) und der Raum.
        QCOMPARE(list->count(), 2);
        QVERIFY(list->item(1)->text().contains(QStringLiteral("7.088")));
        window->resize(RoundtableWindow::kWideWidth, RoundtableWindow::kWideHeight);
        settle();
        QCOMPARE(window->shape(), RoundtableWindow::Shape::ThreeColumns);
        bild(*window, QStringLiteral("fenster-breit.png"));
        bild(applet, QStringLiteral("spalte-panel-im-fenster.png"));
        auto* back = applet.findChild<QPushButton*>(QStringLiteral("roundtablePutBackBtn"));
        QVERIFY(back);
        QTest::mouseClick(back, Qt::LeftButton);
        settle();
        QVERIFY(!applet.isPoppedOut());
        QVERIFY(!window->isVisible());
        QCOMPARE(panel->parentWidget(), &applet);
    }

    void livePerKlick()
    {
        // ---- Der Referenzserver, wie in tst_roundtable_e2e.
        const QString root = QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR"));
        if (root.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt (ueber ctest starten)");
        }
        QString python = QString::fromLocal8Bit(qgetenv("LONGPATH_ROUNDTABLE_PYTHON"));
        if (python.isEmpty()) {
            python = QStringLiteral("python3");
        }
        QProcess probe;
        probe.start(python, {QStringLiteral("-c"),
                             QStringLiteral("import sys, websockets, cryptography; "
                                            "sys.exit(0 if sys.version_info >= (3, 9) else 1)")});
        if (!probe.waitForFinished(15000) || probe.exitStatus() != QProcess::NormalExit
            || probe.exitCode() != 0) {
            QSKIP("Kein Python mit websockets und cryptography (LONGPATH_ROUNDTABLE_PYTHON)");
        }
        QTemporaryDir dir;
        const quint16 wsPort = freePort();
        quint16 http = freePort();
        while (http == wsPort) {
            http = freePort();
        }
        QDir().mkpath(dir.filePath(QStringLiteral("static")));
        QDir().mkpath(dir.filePath(QStringLiteral("backup")));
        QFile conf(dir.filePath(QStringLiteral("roundtable.conf")));
        QVERIFY(conf.open(QIODevice::WriteOnly));
        conf.write(QStringLiteral("[roundtable]\nws_listen = 127.0.0.1:%1\nhttp_listen = 127.0.0.1:%2\n"
                                  "public_host = 127.0.0.1\ndatabase = %3\nbackup_dir = %4\n"
                                  "static_dir = %5\nlog_level = info\n")
                       .arg(wsPort)
                       .arg(http)
                       .arg(dir.filePath(QStringLiteral("roundtable.db")),
                            dir.filePath(QStringLiteral("backup")), dir.filePath(QStringLiteral("static")))
                       .toUtf8());
        conf.close();
        QProcess server;
        server.setWorkingDirectory(root + QStringLiteral("/roundtable/server"));
        server.setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
        server.setProcessEnvironment(env);
        server.start(python, {QStringLiteral("-m"), QStringLiteral("nereus_roundtable"), QStringLiteral("--config"),
                              conf.fileName(), QStringLiteral("--stop-on-stdin-eof")});
        QVERIFY(server.waitForStarted(kStartMs));
        const bool up = QTest::qWaitFor([wsPort] {
            QTcpSocket s;
            s.connectToHost(QHostAddress::LocalHost, wsPort);
            return s.waitForConnected(200);
        }, kStartMs);
        QVERIFY2(up, qPrintable(QString::fromUtf8(server.readAll())));
        const auto stopServer = qScopeGuard([&server] {
            server.closeWriteChannel();
            if (!server.waitForFinished(10000)) {
                server.kill();
                server.waitForFinished(5000);
            }
        });
        const QUrl url(QStringLiteral("ws://127.0.0.1:%1/v1/station").arg(wsPort));

        // ---- B: ein nackter Client, spricht (getastet).
        QString err;
        RoundtableKey keyB = RoundtableKey::generate(&err);
        QVERIFY2(keyB.isValid(), qPrintable(err));
        RoundtableClient b;
        b.setServerUrl(url);
        b.setIdentity(&keyB);
        b.setStationInfo(QStringLiteral("DL2XYZ"), QStringLiteral("JN58td"));
        RoundtableSliceState sb;
        sb.id = 0;
        sb.hz = 7088000;
        sb.mode = QStringLiteral("LSB");
        sb.filterHz = 2700;
        sb.keyed = true;
        b.setSlices({sb});
        b.setFirstRunAnswered(true);
        RoundtableModel seenByB;
        seenByB.attachLocal(&b, nullptr);
        // B's S-Meter: Rauschen, bis A sendet; dann A's Signal. Der Boden
        // braucht 10 s (6.5), erst dann ist ein Signal "present".
        double dbmB = -110.0;
        qint64 stampB = 1;
        QTimer meterB;
        QObject::connect(&meterB, &QTimer::timeout, &b,
                         [&b, &stampB, &dbmB]() { b.pushMeter(0, dbmB, ++stampB * 1000000); });
        meterB.start(200);
        QElapsedTimer sinceB;
        sinceB.start();
        b.start();
        QTRY_COMPARE_WITH_TIMEOUT(b.linkState(), RoundtableClient::LinkState::Live, kStartMs);

        // ---- A: Longpath, Hub und Applet. Live geantwortet.
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Roundtable/ServerUrl"), url.toString());
        // Erst "Stay hidden": A liest nur, schickt nichts ueber sich.
        s.setValue(QStringLiteral("Roundtable/FirstRunAnswer"), QStringLiteral("Hidden"));
        s.setValue(QStringLiteral("Roundtable/Hidden"), QStringLiteral("True"));
        RoundtableHub hub;
        hub.setKeyPathForTest(dir.filePath(QStringLiteral("a/roundtable-identity.pem")));
        double meterA = -73.0;
        hub.setMeterSourceForTest([&meterA](int) { return meterA; });
        RoundtableSliceState sa;
        sa.id = 0;
        sa.hz = 7088100;
        sa.mode = QStringLiteral("LSB");
        sa.filterHz = 2700;
        hub.setSlicesForTest({sa});
        hub.reloadSettings();
        SliceModel slice(0);
        slice.setSliceIndex(0);
        slice.setFrequency(7088100.0);
        slice.setDspMode(DSPMode::LSB);
        RoundtableApplet applet(nullptr, &hub, nullptr);
        applet.setSliceSource([&slice]() { return &slice; });
        applet.resize(300, 760);
        applet.show();
        RoundtablePanel* panel = applet.panel();
        QTRY_COMPARE_WITH_TIMEOUT(hub.client()->linkState(), RoundtableClient::LinkState::Hidden, kStartMs);

        // Verborgen: B spricht, A sieht ihn, und A's eigener Messwert steht
        // nur hier, als NOT SENT (vom Hub ueber sliceMeterRead). B sieht
        // A nicht.
        QTRY_COMPARE_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTalkerName")),
                                  QStringList{QStringLiteral("DL2XYZ")}, kSeeMs);
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableReadingCall"))
                                     .contains(QStringLiteral("OE5SOS")), kSeeMs);
        QVERIFY(!panel->findChildren<QLabel*>(QStringLiteral("roundtableNotSentTag")).isEmpty());
        const auto bSeesA = [&seenByB]() {
            for (const QString& id : seenByB.viewedRoomIds()) {
                for (const QJsonValue& v : seenByB.room(id).value(QStringLiteral("members")).toArray()) {
                    if (v.toObject().value(QStringLiteral("callsign")).toString() == QLatin1String("OE5SOS")) {
                        return true;
                    }
                }
            }
            return false;
        };
        QVERIFY(!bSeesA());
        bild(applet, QStringLiteral("panel-verborgen.png"));

        // "Go live" (Spot Hub): jetzt meldet sich A an, B sieht A.
        hub.setHidden(false);
        QTRY_COMPARE_WITH_TIMEOUT(hub.client()->linkState(), RoundtableClient::LinkState::Live, kStartMs);
        QTRY_VERIFY_WITH_TIMEOUT(bSeesA(), kSeeMs);

        // Das Panel zeigt B als Sprecher.
        QTRY_COMPARE_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTalkerName")),
                                  QStringList{QStringLiteral("DL2XYZ")}, kSeeMs);
        bild(applet, QStringLiteral("panel-live.png"));

        // Senden per Tastatur: Text ins Feld, Enter. Die Nachricht steht im
        // Panel und bei B.
        auto* box = panel->findChild<QPlainTextEdit*>(QStringLiteral("roundtableBox"));
        QVERIFY(box);
        box->setFocus();
        QTest::keyClicks(box, QStringLiteral("Good copy in Linz, 59"));
        QTest::keyClick(box, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableMessageText"))
                                     .join(QLatin1Char('|')).contains(QStringLiteral("Good copy in Linz")),
                                 kSeeMs);
        const auto bHas = [&seenByB](const QString& text) {
            for (const QString& id : seenByB.viewedRoomIds()) {
                for (const QJsonValue& v : seenByB.room(id).value(QStringLiteral("messages")).toArray()) {
                    if (v.toObject().value(QStringLiteral("text")).toString().contains(text)) {
                        return true;
                    }
                }
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(bHas(QStringLiteral("Good copy in Linz")), kSeeMs);
        QVERIFY(box->toPlainText().isEmpty());
        bild(applet, QStringLiteral("panel-gesendet.png"));

        // Wie hoeren mich andere: A sendet, B hoert A mit -71 dBm.
        sb.keyed = false;
        b.setSlices({sb});
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTalkerName")) != QStringList{QStringLiteral("DL2XYZ")}, kSeeMs);
        if (sinceB.elapsed() < 11000) {
            QTest::qWait(int(11000 - sinceB.elapsed()));
        }
        sa.keyed = true;
        hub.setSlicesForTest({sa});
        dbmB = -71.0;
        QTRY_COMPARE_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTxText")),
                                  QStringList{QStringLiteral("You · 1 hear you")}, kSeeMs);
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableReadingCall")).contains(QStringLiteral("DL2XYZ")),
                                 kSeeMs);
        bild(applet, QStringLiteral("panel-du-sendest.png"));
        dbmB = -110.0;

        // Sprecher benennen: A taste los; dann hoert A ein Signal weit ueber
        // seinem Boden, niemand tastet — ein Durchgang ohne Sprecher. Er
        // endet, steht unter EARLIER, und A benennt ihn per Klick.
        sa.keyed = false;
        hub.setSlicesForTest({sa});
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTxText")).isEmpty(), kSeeMs);
        QTest::qWait(4000);  // das Ende von A's Durchgang (3 s, 6.6) und mehr Boden
        meterA = -40.0;
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTalkerName")).join(QString())
                                     .contains(QStringLiteral("Unknown")), kSeeMs);
        meterA = -73.0;  // still: nach 3 s endet der Durchgang (6.6 Quiet)
        QPushButton* name = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(
            [&]() {
                for (QPushButton* p : panel->findChildren<QPushButton*>(QStringLiteral("roundtableTurnNameBtn"))) {
                    if (p->isVisible() && p->isEnabled()) {
                        name = p;
                        return true;
                    }
                }
                return false;
            }(),
            kSeeMs);
        QTest::mouseClick(name, Qt::LeftButton);
        settle();
        auto* input = panel->findChild<QLineEdit*>(QStringLiteral("roundtableNameInput"));
        QVERIFY(input);
        QTest::keyClicks(input, QStringLiteral("dl2xyz"));
        QTest::keyClick(input, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(texts(panel, QStringLiteral("roundtableTurnNames")).join(QLatin1Char('|'))
                                     .contains(QStringLiteral("DL2XYZ")),
                                 kSeeMs);
        bild(applet, QStringLiteral("panel-benannt.png"));

        meterB.stop();
        b.stop();
        hub.client()->stop();
        s.remove(QStringLiteral("Roundtable/FirstRunAnswer"));
    }
};

QTEST_MAIN(TstRoundtablePanel)
#include "tst_roundtable_panel.moc"
