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
//   fitsTheRealAppletColumn, aTenCharacterFrequencyFits
//                            Upstreams Passproben (@ e56b3b44c): in 260,
//                            280, 298 und 340 px ragt nichts ueber den
//                            Rand, nichts wird abgeschnitten — nach der
//                            Umstellung der Schrift auf die Leiter des
//                            Hausstils (12 -> 13 px) mitgefahren.
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
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
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

// ---- Upstream's fit cases (tests/tst_roundtable_panel.cpp @ e56b3b44c).
// Longpath's Posed: the hub owns the model; the link state is set through
// RoundtableHub::setLinkStateForTest (Upstream: RoundtableState::
// applyStationValue), the slice's room from the hub's placement.
constexpr qint64 kFitNow = 1'800'000'000'000LL;
const QString kFitRoom = QStringLiteral("lsb-7185000");
const QString kFitOther = QStringLiteral("lsb-7249000");
qint64 g_now = kFitNow;

QJsonObject fitReading(double dbm, qint64 ageMs)
{
    return QJsonObject{{QStringLiteral("dbm"), dbm}, {QStringLiteral("t"), g_now - ageMs},
                       {QStringLiteral("present"), true}};
}

QJsonObject fitMember(const QString& ref, const QString& callsign, double km, const QJsonValue& rd,
                   bool you = false, bool away = false)
{
    QJsonObject m{{QStringLiteral("ref"), ref},          {QStringLiteral("callsign"), callsign},
                  {QStringLiteral("nth"), 1},            {QStringLiteral("grid"), QStringLiteral("EM73")},
                  {QStringLiteral("offsetHz"), 0},       {QStringLiteral("away"), away},
                  {QStringLiteral("keyed"), false},      {QStringLiteral("you"), you},
                  {QStringLiteral("reading"), rd}};
    if (km >= 0) {
        m.insert(QStringLiteral("km"), km);
    }
    return m;
}

QJsonObject fitTalker(const QString& ref, const QString& callsign)
{
    return QJsonObject{{QStringLiteral("ref"), ref}, {QStringLiteral("callsign"), callsign},
                       {QStringLiteral("nth"), 1}};
}

QJsonObject fitTurn(qint64 id, const QString& kind, const QJsonArray& talkers, qint64 start,
                 const QJsonValue& end, bool mine = false)
{
    return QJsonObject{{QStringLiteral("id"), id},       {QStringLiteral("kind"), kind},
                       {QStringLiteral("talkers"), talkers}, {QStringLiteral("start"), start},
                       {QStringLiteral("end"), end},     {QStringLiteral("names"), QJsonArray{}},
                       {QStringLiteral("mine"), mine}};
}

QJsonObject fitMessage(qint64 id, const QString& ref, const QString& callsign, const QString& text,
                    const QString& state = QStringLiteral("shown"))
{
    return QJsonObject{{QStringLiteral("id"), id},     {QStringLiteral("ref"), ref},
                       {QStringLiteral("callsign"), callsign}, {QStringLiteral("nth"), 1},
                       {QStringLiteral("t"), g_now - 60000 + id}, {QStringLiteral("state"), state},
                       {QStringLiteral("text"), text}};
}

QJsonArray fitMembers()
{
    return QJsonArray{fitMember(QStringLiteral("me"), QStringLiteral("KG4VCF"), -1, QJsonValue::Null, true),
                      fitMember(QStringLiteral("a"), QStringLiteral("W4ABC"), 640, fitReading(-73, 2000)),
                      fitMember(QStringLiteral("b"), QStringLiteral("N3XYZ"), 120, fitReading(-85, 1000)),
                      fitMember(QStringLiteral("c"), QStringLiteral("K9AWY"), 900, fitReading(-90, 1000),
                             false, true),
                      fitMember(QStringLiteral("t"), QStringLiteral("W1TLK"), 300, fitReading(-60, 500))};
}

QJsonObject fitRoomOf(const QString& id, const QJsonArray& now, const QJsonArray& earlier,
                   const QJsonArray& messages, const QJsonArray& who = fitMembers())
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("room")}, {QStringLiteral("id"), id},
                       {QStringLiteral("members"), who},               {QStringLiteral("now"), now},
                       {QStringLiteral("earlier"), earlier},           {QStringLiteral("messages"), messages}};
}

QJsonObject fitTalkingRoom(const QJsonArray& messages = {})
{
    return fitRoomOf(kFitRoom,
                  QJsonArray{fitTurn(30, QStringLiteral("talking"),
                                  QJsonArray{fitTalker(QStringLiteral("t"), QStringLiteral("W1TLK"))},
                                  g_now - 42000, QJsonValue::Null)},
                  QJsonArray{fitTurn(20, QStringLiteral("unknown"), QJsonArray{}, g_now - 300000,
                                  g_now - 240000),
                             fitTurn(10, QStringLiteral("talking"),
                                  QJsonArray{fitTalker(QStringLiteral("a"), QStringLiteral("W4ABC"))},
                                  g_now - 600000, g_now - 400000)},
                  messages);
}

template <typename T>
T* child(QWidget* root, const QString& name)
{
    return root->findChild<T*>(name);
}

int yIn(QWidget* root, QWidget* w)
{
    return w->mapTo(root, QPoint(0, 0)).y();
}

// The header row's parts as fitCount measures them: everything in the row
// but the count (margins and spacings in), the count on one line and on two,
// and how much of the panel's width is not the room the row has.
struct HeaderFit {
    int row = 0;
    int spacing = 0;
    int oneLine = 0;
    int twoLines = 0;
    int outside = 0;
};

HeaderFit headerFit(QWidget* panel, const QString& oneLine, const QString& twoLines, bool withTag = true)
{
    HeaderFit fit;
    auto* body = panel->findChild<QWidget*>(QStringLiteral("roundtableHeaderBody"));
    auto* freq = panel->findChild<QLabel*>(QStringLiteral("roundtableFrequency"));
    QHBoxLayout* row = nullptr;
    for (int i = 0; i < body->layout()->count(); ++i) {
        auto* candidate = qobject_cast<QHBoxLayout*>(body->layout()->itemAt(i)->layout());
        if (candidate != nullptr && candidate->indexOf(freq) >= 0) {
            row = candidate;
        }
    }
    fit.spacing = row->spacing();
    const QMargins margins = body->layout()->contentsMargins();
    fit.row = margins.left() + margins.right();
    int parts = 0;
    for (const QString& name : {QStringLiteral("roundtableTag"), QStringLiteral("roundtableFrequency"),
                                QStringLiteral("roundtableMode"), QStringLiteral("roundtablePopBtn"),
                                QStringLiteral("roundtableMenuBtn")}) {
        for (QWidget* w : body->findChildren<QWidget*>(name)) {
            const bool tag = name == QLatin1String("roundtableTag");
            if (row->indexOf(w) >= 0 && (tag ? withTag && w->isVisibleTo(body) : true)) {
                fit.row += w->sizeHint().width();
                ++parts;
            }
        }
    }
    fit.row += (parts - 1) * fit.spacing;
    QLabel* count = nullptr;
    for (QLabel* label : panel->findChildren<QLabel*>(QStringLiteral("roundtableCount"))) {
        if (label->isVisibleTo(panel)) {
            count = label;
        }
    }
    count->ensurePolished();
    const QFontMetrics metrics(count->font());
    fit.oneLine = metrics.horizontalAdvance(oneLine);
    for (const QString& line : twoLines.split(QLatin1Char('\n'))) {
        fit.twoLines = std::max(fit.twoLines, metrics.horizontalAdvance(line));
    }
    auto* scroll = panel->findChild<QScrollArea*>(QStringLiteral("roundtableScroll"));
    const int viewport = scroll->viewport()->width();
    const int room = scroll->verticalScrollBar()->isVisible()
                      ? viewport
                      : viewport - panel->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    fit.outside = panel->width() - room;
    return fit;
}


struct Posed {
    RoundtableHub hub;

    Posed()
    {
        hub.setLinkStateForTest(RoundtableModel::LinkState::Live);
        QJsonArray rooms{
            QJsonObject{{QStringLiteral("id"), kFitRoom}, {QStringLiteral("hz"), 7185000},
                        {QStringLiteral("family"), QStringLiteral("lsb")}, {QStringLiteral("members"), 5}},
            QJsonObject{{QStringLiteral("id"), kFitOther}, {QStringLiteral("hz"), 7249000},
                        {QStringLiteral("family"), QStringLiteral("lsb")}, {QStringLiteral("members"), 3}}};
        emit hub.client()->roomsReceived(QJsonObject{{QStringLiteral("rooms"), rooms}});
        placeSliceAt(7185000, QStringLiteral("LSB"));
    }

    void placeSliceAt(qint64 hz, const QString& mode)
    {
        RoundtableSliceState s;
        s.id = 0;
        s.hz = hz;
        s.mode = mode;
        s.filterHz = 2700;
        hub.setSlicesForTest({s});
    }

    void setLink(RoundtableModel::LinkState s, qint64 since = 0) { hub.setLinkStateForTest(s, since); }

    void mention(const QString& room)
    {
        emit hub.client()->mentionReceived(QJsonObject{
            {QStringLiteral("room"), room},
            {QStringLiteral("message"), QJsonObject{{QStringLiteral("id"), 99},
                                                    {QStringLiteral("t"), g_now},
                                                    {QStringLiteral("callsign"), QStringLiteral("W4ABC")}}}});
    }
};

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
        applet.resize(260, 640);
        applet.show();
        settle();

        auto* line = applet.findChild<QLabel*>(QStringLiteral("roundtableConnectionLine"));
        QVERIFY(line);
        QVERIFY2(line->text().contains(QStringLiteral("answer in Settings")), qPrintable(line->text()));
        // Kein Server gefragt, also nichts ueber ihn gesagt.
        QVERIFY2(!line->text().contains(QStringLiteral("Legacy")), qPrintable(line->text()));
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
        applet.resize(260, 760);
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
        window->resize(800, RoundtableWindow::kWideHeight);
        settle();
        QCOMPARE(window->shape(), RoundtableWindow::Shape::TwoColumns);
        bild(*window, QStringLiteral("fenster-zwei-spalten.png"));
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
        applet.resize(260, 760);
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

    void fitsTheRealAppletColumn_data()
    {
        QTest::addColumn<int>("scene");
        QTest::addColumn<int>("width");
        const QStringList scenes{QStringLiteral("talking"),  QStringLiteral("unknown"),
                                 QStringLiteral("doubled"),  QStringLiteral("keyed"),
                                 QStringLiteral("quiet"),    QStringLiteral("reading"),
                                 QStringLiteral("hidden"),   QStringLiteral("naming"),
                                 QStringLiteral("muted"),    QStringLiteral("mention")};
        // 260: the applet column's minimum (Style::kAppletPanelW). 298: the
        // column at 1512x869 (a 280 px viewport inside the applet frame),
        // 280 that viewport itself. 340: the design's width.
        for (int width : {260, 280, 298, 340}) {
            for (int i = 0; i < scenes.size(); ++i) {
                QTest::newRow(qPrintable(QStringLiteral("%1 %2").arg(scenes.at(i)).arg(width))) << i << width;
            }
        }
    }

    void fitsTheRealAppletColumn()
    {
        RoundtableModel::setClockForTest([] { return g_now; });
        const auto restore = qScopeGuard([] { RoundtableModel::setClockForTest({}); });
        // Every scene in the real applet column: no widget runs past the
        // viewport, no label is clipped, and a long callsign elides with the
        // whole one in a tooltip rather than widening the panel.
        QFETCH(int, scene);
        QFETCH(int, width);
        Posed p;
        QJsonArray who = fitMembers();
        who.append(fitMember(QStringLiteral("l"), QStringLiteral("KH6/W1LONGCALL"), 4800, fitReading(-79, 1500)));
        const QJsonArray chat{
            fitMessage(1, QStringLiteral("b"), QStringLiteral("N3XYZ"), QStringLiteral("muted")),
            fitMessage(2, QStringLiteral("a"), QStringLiteral("W4ABC"),
                    QStringLiteral("Who was that on at the top of the hour? Strong but no ID.")),
            fitMessage(3, QStringLiteral("l"), QStringLiteral("KH6/W1LONGCALL"),
                    QStringLiteral("KG4VCF are you there? Good signal into Hawaii tonight."))};
        QJsonObject room = fitTalkingRoom(chat);
        room.insert(QStringLiteral("members"), who);
        const QJsonArray pair{fitTalker(QStringLiteral("t"), QStringLiteral("W1TLK")),
                              fitTalker(QStringLiteral("l"), QStringLiteral("KH6/W1LONGCALL"))};
        switch (scene) {
        case 1:
            room.insert(QStringLiteral("now"),
                        QJsonArray{fitTurn(31, QStringLiteral("unknown"), {}, g_now - 9000, QJsonValue::Null)});
            break;
        case 2:
            room.insert(QStringLiteral("now"),
                        QJsonArray{fitTurn(31, QStringLiteral("doubled"), pair, g_now - 9000, QJsonValue::Null)});
            break;
        case 3:
            room.insert(QStringLiteral("now"),
                        QJsonArray{fitTurn(40, QStringLiteral("talking"),
                                        QJsonArray{fitTalker(QStringLiteral("me"), QStringLiteral("KG4VCF"))},
                                        g_now - 65000, QJsonValue::Null, true)});
            break;
        case 4:
            room.insert(QStringLiteral("now"), QJsonArray{});
            break;
        case 8:
            AppSettings::instance().setValue(QStringLiteral("Roundtable/MuteList"), QStringLiteral("N3XYZ"));
            p.hub.reloadSettings();
            break;
        case 9:
            p.mention(kFitRoom);
            break;
        default:
            break;
        }
        p.hub.model()->applyRoom(room);
        if (scene == 5) {
            QJsonObject other = fitTalkingRoom(chat);
            other.insert(QStringLiteral("members"), who);
            other.insert(QStringLiteral("id"), kFitOther);
            p.hub.model()->applyRoom(other);
        }
        if (scene == 6) {
            p.setLink(RoundtableModel::LinkState::Hidden);
        }
        RoundtablePanel panel(p.hub.model());
        panel.resize(width, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        if (scene == 5) {
            panel.readRoom(kFitOther);
            settle();
            QVERIFY(panel.isReading());
        }
        if (scene == 6) {
            panel.setOwnReading(-73.0, g_now);
            settle();
            // Longpath: no server word before the first-run answer.
            QCOMPARE(texts(&panel, QStringLiteral("roundtableConnectionLine")),
                     QStringList{QStringLiteral("You are hidden")});
        }
        if (scene == 7) {
            auto names = panel.findChildren<QPushButton*>(QStringLiteral("roundtableTurnNameBtn"));
            QVERIFY(!names.isEmpty());
            names.first()->click();
            settle();
            QVERIFY(child<QLineEdit>(&panel, QStringLiteral("roundtableNameInput")) != nullptr);
        }
        // The resize-driven rebuild runs on the next turn of the loop.
        settle();

        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"));
        auto* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        const int viewport = scroll->viewport()->width();
        QVERIFY(viewport > 0);
        // What the content may ask for: the viewport, less a scroll bar
        // when none shows (so one appearing cannot clip the content).
        const int allowed = scroll->verticalScrollBar()->isVisible()
                             ? viewport
                             : viewport - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QStringList wide;
        if (content->width() > viewport) {
            wide.append(QStringLiteral("content %1").arg(content->width()));
        }
        for (QWidget* w : content->findChildren<QWidget*>()) {
            if (!w->isVisibleTo(content)) {
                continue;
            }
            const int right = w->mapTo(content, QPoint(w->width(), 0)).x();
            const int hint = w->layout() != nullptr ? w->layout()->minimumSize().width()
                                                    : w->minimumSizeHint().width();
            // Elided or wrapped, never clipped: a label narrower than its
            // minimum (an eliding label's is its kept characters) is cut.
            if (auto* label = qobject_cast<QLabel*>(w); label != nullptr && !label->wordWrap()
                                                         && !label->text().isEmpty() && label->pixmap().isNull()
                                                         && label->width() < label->minimumSizeHint().width()) {
                wide.append(QStringLiteral("QLabel %1 \"%2\" clipped to %3 of %4")
                                .arg(label->objectName(), label->text())
                                .arg(label->width())
                                .arg(label->minimumSizeHint().width()));
            }
            if (right > viewport || std::max(hint, w->minimumWidth()) > allowed) {
                wide.append(QStringLiteral("%1 %2 ends at %3, asks %4")
                                .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName())
                                .arg(right)
                                .arg(std::max(hint, w->minimumWidth())));
            }
        }
        QVERIFY2(wide.isEmpty(), qPrintable(QStringLiteral("viewport %1: %2").arg(viewport).arg(
                                                wide.join(QStringLiteral("; ")))));
        // Room for a scroll bar too, so the scroll area cannot flip.
        const int withBar = width - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QVERIFY2(content->minimumSizeHint().width() <= withBar,
                 qPrintable(QString::number(content->minimumSizeHint().width())));

        // The long callsign's reading row elides it, with the whole
        // callsign in a tooltip once it is painted short. (Doubled, it is
        // talking; keyed, the rows are of you; quiet, a list of names.)
        if (scene != 2 && scene != 3 && scene != 4) {
            QLabel* longCall = nullptr;
            for (QLabel* label : panel.findChildren<QLabel*>(QStringLiteral("roundtableReadingCall"))) {
                if (label->text() == QLatin1String("KH6/W1LONGCALL") && label->isVisibleTo(&panel)) {
                    longCall = label;
                }
            }
            QVERIFY(longCall != nullptr);
            longCall->repaint();
            const int whole = longCall->fontMetrics().horizontalAdvance(longCall->text());
            if (longCall->width() < whole) {
                QCOMPARE(longCall->toolTip(), QStringLiteral("KH6/W1LONGCALL"));
            }
            // Six characters always show whole.
            QVERIFY2(longCall->width() >= longCall->fontMetrics().horizontalAdvance(QStringLiteral("KH6/W1")),
                     qPrintable(QString::number(longCall->width())));
        }
    }

    void aTenCharacterFrequencyFits_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<bool>("reading");
        for (int width : {260, 280, 298}) {
            QTest::newRow(qPrintable(QStringLiteral("tuned %1").arg(width))) << width << false;
            QTest::newRow(qPrintable(QStringLiteral("reading %1").arg(width))) << width << true;
        }
    }

    void aTenCharacterFrequencyFits()
    {
        RoundtableModel::setClockForTest([] { return g_now; });
        const auto restore = qScopeGuard([] { RoundtableModel::setClockForTest({}); });
        // A 20 m room ("14.212.000", ten characters) in the docked column:
        // the header fits, and when the row would not fit even with the
        // count below it, the mode moves down beside the count.
        QFETCH(int, width);
        QFETCH(bool, reading);
        const QString twenty = QStringLiteral("usb-14212000");
        Posed p;
        if (!reading) {
            p.placeSliceAt(14212000, QStringLiteral("USB"));
        }
        QJsonArray rooms{
            QJsonObject{{QStringLiteral("id"), kFitRoom}, {QStringLiteral("hz"), 7185000},
                        {QStringLiteral("family"), QStringLiteral("lsb")}, {QStringLiteral("members"), 5}},
            QJsonObject{{QStringLiteral("id"), twenty}, {QStringLiteral("hz"), 14212000},
                        {QStringLiteral("family"), QStringLiteral("usb")}, {QStringLiteral("members"), 5}}};
        emit p.hub.client()->roomsReceived(QJsonObject{{QStringLiteral("rooms"), rooms}});
        p.hub.model()->applyRoom(fitTalkingRoom());
        QJsonObject room = fitTalkingRoom();
        room.insert(QStringLiteral("id"), twenty);
        p.hub.model()->applyRoom(room);
        RoundtablePanel panel(p.hub.model());
        panel.resize(width, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        if (reading) {
            panel.readRoom(twenty);
            settle();
            QVERIFY(panel.isReading());
        }
        settle();
        QCOMPARE(panel.roomId(), twenty);
        QCOMPARE(texts(&panel, QStringLiteral("roundtableFrequency")), QStringList{QStringLiteral("14.212.000")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableMode")), QStringList{QStringLiteral("USB")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTag")).size(), 1);

        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"));
        auto* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        auto* header = child<QWidget>(&panel, QStringLiteral("roundtableHeader"));
        const int viewport = scroll->viewport()->width();
        const int allowed = scroll->verticalScrollBar()->isVisible()
                             ? viewport
                             : viewport - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QStringList wide;
        QList<QWidget*> parts = header->findChildren<QWidget*>();
        parts.prepend(header);
        for (QWidget* w : parts) {
            if (!w->isVisibleTo(content)) {
                continue;
            }
            const int right = w->mapTo(content, QPoint(w->width(), 0)).x();
            const int hint = w->layout() != nullptr ? w->layout()->minimumSize().width()
                                                    : w->minimumSizeHint().width();
            if (auto* label = qobject_cast<QLabel*>(w); label != nullptr && !label->text().isEmpty()
                                                         && label->width() < label->minimumSizeHint().width()) {
                wide.append(QStringLiteral("QLabel %1 \"%2\" clipped to %3 of %4")
                                .arg(label->objectName(), label->text())
                                .arg(label->width())
                                .arg(label->minimumSizeHint().width()));
            }
            if (right > viewport || std::max(hint, w->minimumWidth()) > allowed) {
                wide.append(QStringLiteral("%1 %2 ends at %3, asks %4")
                                .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName())
                                .arg(right)
                                .arg(std::max(hint, w->minimumWidth())));
            }
        }
        QVERIFY2(wide.isEmpty(), qPrintable(QStringLiteral("viewport %1: %2").arg(viewport).arg(
                                                wide.join(QStringLiteral("; ")))));

        // The mode is in the row exactly when the row (count aside) fits.
        const HeaderFit fit = headerFit(&panel, QStringLiteral("5 in the room · 1 away"),
                                        QStringLiteral("5 in the room\n1 away"));
        QLabel* freq = child<QLabel>(&panel, QStringLiteral("roundtableFrequency"));
        QLabel* mode = nullptr;
        for (QLabel* label : panel.findChildren<QLabel*>(QStringLiteral("roundtableMode"))) {
            if (label->isVisibleTo(&panel)) {
                mode = label;
            }
        }
        QVERIFY(mode != nullptr);
        const bool rowFits = fit.row <= width - fit.outside;
        const int freqBottom = yIn(&panel, freq) + freq->height();
        QVERIFY2(rowFits ? yIn(&panel, mode) < freqBottom : yIn(&panel, mode) >= freqBottom,
                 qPrintable(QStringLiteral("row %1 in %2, mode at %3, frequency ends %4")
                                .arg(fit.row)
                                .arg(width - fit.outside)
                                .arg(yIn(&panel, mode))
                                .arg(freqBottom)));
    }
};

QTEST_MAIN(TstRoundtablePanel)
#include "tst_roundtable_panel.moc"
