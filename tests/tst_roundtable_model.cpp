// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_roundtable_model.cpp  (Longpath)
// =================================================================
//
// RoundtableModel: what the Roundtable screens read, fed by the client's
// band list and room views, its state from RoundtableHub.
//
// Uebernommen aus dem Upstream NereusSDR (`tests/tst_roundtable_model.cpp`
// @ e56b3b44c, J.J. Boyd KG4VCF). Longpath-Abweichungen:
//   - Nur die Faelle der Everything-Aufstellung. Entfallen sind die, die
//     ein Fenster ueber die Stationsverbindung pruefen (Placements mit
//     RecordingLink und CoreStream, bothPlacementsHoldEqualRooms,
//     aWindowsActionsGoToItsLink, theWindowReadsTheCoresRoundtableObject,
//     otherStreamsAreNotRoundtables, backendNegotiationReachesAnEmptyBand-
//     Window, frequencyViewsKeepRoomScopedRefsSeparate,
//     historyAnswersStaySeparateFromNearbyViews, die beiden readAt-Faelle):
//     Longpath hat keine Fenster eines Cores.
//   - Die Faelle laufen gegen ein Modell am RoundtableHub (Local), nicht
//     gegen zwei Aufstellungen nebeneinander; die Vergleiche mit dem
//     Fenster sind weggelassen.
//   - OperatorWording gibt es bei uns nicht: geprueft wird, dass ein Grund
//     dasteht.
//
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 11).
//   2026-10-24: Roundtable plan Task 13: earlier turns and recentNames.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// Modification history (Longpath):
//   2026-11-18 - Uebernommen fuer Longpath (Roundtable Stufe 2) von
//                Martin Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>

#include <algorithm>

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableClient.h"
#include "models/RoundtableHub.h"
#include "models/RoundtableModel.h"

using namespace Longpath;

namespace {

const QString kRoom = QStringLiteral("usb-14200000");
const QString kOtherRoom = QStringLiteral("cw-7030000");

QJsonObject member(const QString& ref, const QString& callsign, const QJsonValue& reading)
{
    return QJsonObject{{QStringLiteral("ref"), ref},
                       {QStringLiteral("callsign"), callsign},
                       {QStringLiteral("grid"), QStringLiteral("EM73")},
                       {QStringLiteral("reading"), reading}};
}

QJsonObject reading(double dbm, qint64 t)
{
    return QJsonObject{{QStringLiteral("dbm"), dbm}, {QStringLiteral("t"), t}};
}

QJsonObject turn(qint64 id, qint64 start, const QJsonValue& end, const QJsonArray& names)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("kind"), QStringLiteral("voice")},
                       {QStringLiteral("start"), start},
                       {QStringLiteral("end"), end},
                       {QStringLiteral("names"), names}};
}

QJsonObject name(const QString& callsign, int count)
{
    return QJsonObject{{QStringLiteral("name"), callsign}, {QStringLiteral("count"), count}};
}

QJsonObject message(qint64 id, qint64 t, const QString& callsign, const QString& text)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("t"), t},
                       {QStringLiteral("callsign"), callsign},
                       {QStringLiteral("text"), text}};
}

QJsonObject summary(const QString& id, const QString& family, qint64 hz, int members)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("family"), family},
                       {QStringLiteral("hz"), hz},
                       {QStringLiteral("members"), members}};
}

QJsonObject roomsFull()
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("rooms")},
                       {QStringLiteral("rooms"),
                        QJsonArray{summary(kRoom, QStringLiteral("usb"), 14200000, 2),
                                   summary(kOtherRoom, QStringLiteral("cw"), 7030000, 1)}}};
}

QJsonObject roomFull()
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("room")},
        {QStringLiteral("id"), kRoom},
        {QStringLiteral("members"),
         QJsonArray{member(QStringLiteral("a/1"), QStringLiteral("W1AW"), reading(-73.5, 1000)),
                    member(QStringLiteral("b/0"), QStringLiteral("KG4VCF"), QJsonValue::Null)}},
        {QStringLiteral("now"),
         QJsonArray{turn(11, 900, QJsonValue::Null,
                         QJsonArray{name(QStringLiteral("W1AW"), 1),
                                    name(QStringLiteral("K1ABC"), 3)})}},
        {QStringLiteral("earlier"),
         QJsonArray{turn(9, 100, 400, QJsonArray{}),
                    turn(10, 450, 800, QJsonArray{name(QStringLiteral("N0CALL"), 2)})}},
        {QStringLiteral("messages"),
         QJsonArray{message(21, 950, QStringLiteral("W1AW"), QStringLiteral("good signal")),
                    message(20, 940, QStringLiteral("KG4VCF"), QStringLiteral("hello all"))}}};
}

QJsonObject roomDelta()
{
    // A third station joins, the first leaves, a turn ends and another
    // opens, and a message arrives.
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("roomDelta")},
        {QStringLiteral("room"), kRoom},
        {QStringLiteral("members"),
         QJsonArray{member(QStringLiteral("c/2"), QStringLiteral("K1ABC"), reading(-80.0, 1200)),
                    member(QStringLiteral("b/0"), QStringLiteral("KG4VCF"), reading(-61.0, 1210))}},
        {QStringLiteral("left"), QJsonArray{QStringLiteral("a/1")}},
        {QStringLiteral("turns"),
         QJsonArray{turn(11, 900, 1100, QJsonArray{name(QStringLiteral("K1ABC"), 4)}),
                    turn(12, 1150, QJsonValue::Null, QJsonArray{})}},
        {QStringLiteral("messages"),
         QJsonArray{message(22, 1160, QStringLiteral("K1ABC"), QStringLiteral("thanks"))}}};
}

// Longpath: the one placement there is. The hub owns the client and the
// model, as RadioModel's hub does; nothing here starts the client.
struct Local {
    RoundtableHub hub;
    RoundtableClient& client = *hub.client();
    RoundtableModel& local = *hub.model();
};

QString compact(const QJsonObject& o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QStringList refsOf(const QJsonObject& room)
{
    QStringList refs;
    for (const QJsonValue& m : room.value(QStringLiteral("members")).toArray()) {
        refs.append(m.toObject().value(QStringLiteral("ref")).toString());
    }
    return refs;
}

} // namespace

class TstRoundtableModel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Nie in die echten Einstellungen schreiben.
        QStandardPaths::setTestModeEnabled(true);
        auto& s = AppSettings::instance();
        for (const char* k : {"Roundtable/AutoStart", "Roundtable/FirstRunAnswer",
                              "Roundtable/Hidden", "Roundtable/MuteList",
                              "Roundtable/MentionsSeenMs"}) {
            s.remove(QLatin1String(k));
        }
        // Nie zu roundtable.nereussdr.com: eine Adresse auf diesem Rechner,
        // an der niemand lauscht. Ohne Erstantwort oeffnet der Client
        // ohnehin keine Verbindung.
        s.setValue(QStringLiteral("Roundtable/ServerUrl"), QStringLiteral("ws://127.0.0.1:9/v1/station"));
        s.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("OE5SOS"));
        // The scripted rooms' times are a few seconds after the epoch.
        RoundtableModel::setClockForTest([] { return qint64(2000); });
    }

    void cleanupTestCase() { RoundtableModel::setClockForTest({}); }

    void sharedPathPreferencesHaveApprovedDefaults()
    {
        // Longpath: km (Upstream: mi).
        QCOMPARE(RoundtableModel::stationSettingDefault(QStringLiteral("DistanceUnits")), QStringLiteral("km"));
        QCOMPARE(RoundtableModel::stationSettingDefault(QStringLiteral("ShowBearing")), QStringLiteral("True"));
        QVERIFY(RoundtableModel::stationSettingNames().contains(QStringLiteral("DistanceUnits")));
        QVERIFY(RoundtableModel::stationSettingNames().contains(QStringLiteral("ShowBearing")));
    }

    void roomsAndRoomReadAsTheDesignSays()
    {
        Local p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        // The band list by center frequency.
        const QJsonArray rooms = p.local.rooms();
        QCOMPARE(rooms.size(), 2);
        QCOMPARE(rooms.at(0).toObject().value(QStringLiteral("id")).toString(), kOtherRoom);
        QCOMPARE(rooms.at(1).toObject().value(QStringLiteral("id")).toString(), kRoom);
        QVERIFY(!rooms.at(0).toObject().contains(QStringLiteral("record")));

        const QJsonObject room = p.local.room(kRoom);
        QCOMPARE(room.value(QStringLiteral("family")).toString(), QStringLiteral("usb"));
        QCOMPARE(room.value(QStringLiteral("hz")).toInteger(), 14200000);
        QCOMPARE(refsOf(room), (QStringList{QStringLiteral("a/1"), QStringLiteral("b/0")}));
        const QJsonArray now = room.value(QStringLiteral("now")).toArray();
        QCOMPARE(now.size(), 1);
        // A turn's names by count, highest first.
        const QJsonArray names = now.at(0).toObject().value(QStringLiteral("names")).toArray();
        QCOMPARE(names.at(0).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("K1ABC"));
        QCOMPARE(names.at(1).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("W1AW"));
        // Ended turns newest first; messages oldest first.
        const QJsonArray earlier = room.value(QStringLiteral("earlier")).toArray();
        QCOMPARE(earlier.at(0).toObject().value(QStringLiteral("id")).toInteger(), 10);
        QCOMPARE(earlier.at(1).toObject().value(QStringLiteral("id")).toInteger(), 9);
        const QJsonArray messages = room.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.at(0).toObject().value(QStringLiteral("id")).toInteger(), 20);
        QCOMPARE(messages.at(1).toObject().value(QStringLiteral("id")).toInteger(), 21);
    }

    void joiningOrderSurvivesAMemberLeaving()
    {
        Local p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        emit p.client.roomDeltaReceived(roomDelta());
        // b/0 stayed in its place; c/2 joined after it.
        QCOMPARE(refsOf(p.local.room(kRoom)),
                 (QStringList{QStringLiteral("b/0"), QStringLiteral("c/2")}));
    }

    void aRoomKeepsItsLastFiftyMessages()
    {
        Local p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        QJsonArray many;
        for (int i = 0; i < 60; ++i) {
            many.append(message(100 + i, 2000 + i, QStringLiteral("W1AW"),
                                QStringLiteral("line %1").arg(i)));
        }
        emit p.client.roomDeltaReceived(
            QJsonObject{{QStringLiteral("room"), kRoom}, {QStringLiteral("messages"), many}});
        const QJsonArray messages = p.local.room(kRoom).value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.size(), RoundtableModel::kRoomMessages);
        QCOMPARE(messages.first().toObject().value(QStringLiteral("id")).toInteger(), 110);
    }

    void aClosedRoomLeavesTheView()
    {
        Local p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        QVERIFY(!p.local.room(kRoom).isEmpty());
        QSignalSpy changed(&p.local, &RoundtableModel::roomChanged);
        emit p.client.roomDeltaReceived(
            QJsonObject{{QStringLiteral("room"), kRoom}, {QStringLiteral("closed"), true}});
        QVERIFY(p.local.room(kRoom).isEmpty());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(changed.last().at(0).toString(), kRoom);
    }

    // Wire spec 5.4.6: a turn that leaves the 30-minute earlier window is
    // not announced; each placement drops it itself, on its own timer.
    void earlierTurnsAreTheLastThirtyMinutes()
    {
        constexpr qint64 kMinute = 60LL * 1000LL;
        qint64 now = 100LL * kMinute;
        RoundtableModel::setClockForTest([&now] { return now; });

        QJsonObject full = roomFull();
        full.insert(QStringLiteral("now"), QJsonArray{});
        full.insert(QStringLiteral("earlier"),
                    QJsonArray{turn(1, 60LL * kMinute, 65LL * kMinute, QJsonArray{}),
                               turn(2, 70LL * kMinute, 75LL * kMinute, QJsonArray{}),
                               turn(3, 90LL * kMinute, 95LL * kMinute, QJsonArray{})});
        Local p;
        emit p.client.roomReceived(full);
        const auto earlierIds = [](const QJsonObject& room) {
            QList<qint64> ids;
            for (const QJsonValue& t : room.value(QStringLiteral("earlier")).toArray()) {
                ids.append(t.toObject().value(QStringLiteral("id")).toInteger());
            }
            return ids;
        };
        // Turn 1 ended 35 minutes ago: dropped on arrival.
        QCOMPARE(earlierIds(p.local.room(kRoom)), (QList<qint64>{3, 2}));

        // Turn 2 leaves the window: room() stops showing it at once, and
        // the prune drops it and tells the screens.
        now = 105LL * kMinute + 1;
        QCOMPARE(earlierIds(p.local.room(kRoom)), (QList<qint64>{3}));
        QSignalSpy changed(&p.local, &RoundtableModel::roomChanged);
        p.local.pruneEarlierTurns();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), kRoom);
        p.local.pruneEarlierTurns();
        QCOMPARE(changed.size(), 1); // nothing more to drop

        // The timer does it unasked once the last one ages out: half a
        // second before turn 3 leaves, on a clock that runs.
        QElapsedTimer running;
        running.start();
        const qint64 base = 125LL * kMinute - 500;
        RoundtableModel::setClockForTest([&running, base] { return base + running.elapsed(); });
        p.local.pruneEarlierTurns(); // reschedules from the running clock
        changed.clear();
        QVERIFY(!earlierIds(p.local.room(kRoom)).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(changed.size() >= 1, 4000);
        QVERIFY(earlierIds(p.local.room(kRoom)).isEmpty());
        RoundtableModel::setClockForTest([] { return qint64(2000); });
    }

    void recentNamesAreTheLastDayOnThatFrequency()
    {
        // Design 8.6: names are picked from those used on that frequency
        // in the last 24 hours.
        constexpr qint64 kMinute = 60LL * 1000LL;
        constexpr qint64 kHour = 60LL * kMinute;
        qint64 now = 30LL * kHour;
        RoundtableModel::setClockForTest([&now] { return now; });

        const auto talked = [](qint64 id, qint64 start, const QJsonValue& end, const QString& callsign,
                               const QJsonArray& names) {
            QJsonObject t = turn(id, start, end, names);
            t.insert(QStringLiteral("talkers"),
                     QJsonArray{QJsonObject{{QStringLiteral("ref"), QJsonValue::Null},
                                            {QStringLiteral("callsign"), callsign},
                                            {QStringLiteral("nth"), 1}}});
            return t;
        };
        QJsonObject full = roomFull();
        full.insert(QStringLiteral("now"), QJsonArray{});
        // K4ABC/P talked 10 minutes ago; an unknown turn 5 minutes ago was
        // named W4XYZ by two and k9awy by one.
        full.insert(QStringLiteral("earlier"),
                    QJsonArray{talked(1, now - 12 * kMinute, now - 10 * kMinute, QStringLiteral("K4ABC/P"),
                                      QJsonArray{}),
                               turn(2, now - 6 * kMinute, now - 5 * kMinute,
                                    QJsonArray{name(QStringLiteral("W4XYZ"), 2),
                                               name(QStringLiteral("k9awy"), 1)})});
        Local p;
        emit p.client.roomReceived(full);
        const QStringList firstSeen{QStringLiteral("K9AWY"), QStringLiteral("W4XYZ"),
                                    QStringLiteral("K4ABC/P")};
        QCOMPARE(p.local.recentNames(kRoom), firstSeen);
        QVERIFY(p.local.recentNames(kOtherRoom).isEmpty());

        // Two hours on, the earlier turns are gone from the room; their
        // names are not. W1AW talks now: used now, so first. A name
        // given as the base callsign of one already listed replaces it.
        now += 2 * kHour;
        p.local.pruneEarlierTurns();
        QVERIFY(p.local.room(kRoom).value(QStringLiteral("earlier")).toArray().isEmpty());
        emit p.client.roomDeltaReceived(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("turns"),
             QJsonArray{talked(3, now - kMinute, QJsonValue::Null, QStringLiteral("W1AW"),
                               QJsonArray{name(QStringLiteral("K4ABC"), 1)})}}});
        const QStringList later{QStringLiteral("K4ABC"), QStringLiteral("W1AW"), QStringLiteral("K9AWY"),
                                QStringLiteral("W4XYZ")};
        QCOMPARE(p.local.recentNames(kRoom), later);

        // The room closes and reopens on the same center: the same room
        // for this, so its names stay.
        emit p.client.roomDeltaReceived(
            QJsonObject{{QStringLiteral("room"), kRoom}, {QStringLiteral("closed"), true}});
        QVERIFY(p.local.room(kRoom).isEmpty());
        QCOMPARE(p.local.recentNames(kRoom), later);
        QJsonObject reopened = roomFull();
        reopened.insert(QStringLiteral("now"), QJsonArray{});
        reopened.insert(QStringLiteral("earlier"), QJsonArray{});
        emit p.client.roomReceived(reopened);
        QCOMPARE(p.local.recentNames(kRoom), later);

        // A day after each was last used, it leaves the list: the names of
        // turn 2 (ended at 30 h less 5 minutes) go first.
        now = 30LL * kHour - 5 * kMinute + RoundtableModel::kRecentNamesWindowMs + 1;
        QCOMPARE(p.local.recentNames(kRoom), (QStringList{QStringLiteral("K4ABC"), QStringLiteral("W1AW")}));
        now = 32LL * kHour + RoundtableModel::kRecentNamesWindowMs + 1;
        QVERIFY(p.local.recentNames(kRoom).isEmpty());
        RoundtableModel::setClockForTest([] { return qint64(2000); });
    }

    void aModelWithNoPlacementRefusesPlainly()
    {
        RoundtableModel model;
        QSignalSpy posted(&model, &RoundtableModel::postResult);
        QSignalSpy refused(&model, &RoundtableModel::actionRefused);
        model.post(kRoom, QStringLiteral("hello"), QStringLiteral("c1"));
        model.mute(QStringLiteral("W1AW"));
        QCOMPARE(posted.size(), 1);
        QCOMPARE(posted.at(0).at(1).toBool(), false);
        QCOMPARE(refused.size(), 1);
        QVERIFY(!posted.at(0).at(2).toString().isEmpty());
        QVERIFY(!refused.at(0).at(1).toString().isEmpty());
    }

    // ---- Longpath: what Upstream's RoundtableState did, from the hub.

    // U6: a message naming this station counts in its room until the read
    // mark passes it; a muted sender's messages do not count. The mark and
    // the mute list are kept in Roundtable/*.
    void mentionsCountUntilReadAndNotFromTheMuted()
    {
        Local p;
        QSignalSpy mentions(&p.local, &RoundtableModel::mentionsChanged);
        const auto mention = [](qint64 id, qint64 t, const QString& from) {
            return QJsonObject{{QStringLiteral("type"), QStringLiteral("mention")},
                               {QStringLiteral("room"), kRoom},
                               {QStringLiteral("message"),
                                message(id, t, from, QStringLiteral("OE5SOS are you there"))}};
        };
        emit p.client.mentionReceived(mention(30, 1500, QStringLiteral("W1AW")));
        emit p.client.mentionReceived(mention(31, 1600, QStringLiteral("K1ABC")));
        QCOMPARE(p.local.mentionCount(kRoom), 2);
        QCOMPARE(p.local.mentionCount(kOtherRoom), 0);
        QVERIFY(!mentions.isEmpty());

        p.local.mute(QStringLiteral("K1ABC/P"));
        QCOMPARE(p.local.muteList(), QStringList{QStringLiteral("K1ABC")});
        QVERIFY(p.local.muted(QStringLiteral("K1ABC")));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/MuteList")).toString(),
                 QStringLiteral("K1ABC"));
        QCOMPARE(p.local.mentionCount(kRoom), 1);

        p.local.markMentionRead(1500);
        QCOMPARE(p.local.mentionCount(kRoom), 0);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1500"));
        // The mark only moves forward.
        p.local.markMentionRead(100);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1500"));

        p.local.unmute(QStringLiteral("K1ABC"));
        QVERIFY(p.local.muteList().isEmpty());
        // A non-callsign is refused in words.
        QSignalSpy refused(&p.local, &RoundtableModel::actionRefused);
        p.local.mute(QStringLiteral("not a call!"));
        QCOMPARE(refused.size(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("mute"));
        AppSettings::instance().remove(QStringLiteral("Roundtable/MentionsSeenMs"));
        AppSettings::instance().remove(QStringLiteral("Roundtable/MuteList"));
    }

    // Wire spec 6.3: each slice is in the listed room of its family nearest
    // its frequency, within the family's width; the model reads it from the
    // hub (sliceRoomId), as the panel follows it.
    void eachSliceIsInItsNearestRoom()
    {
        Local p;
        QSignalSpy changed(&p.local, &RoundtableModel::sliceRoomsChanged);
        RoundtableSliceState usb;
        usb.id = 0;
        usb.hz = 14200400;
        usb.mode = QStringLiteral("USB");
        usb.filterHz = 2700;
        RoundtableSliceState cw;
        cw.id = 1;
        cw.hz = 7030100;
        cw.mode = QStringLiteral("CWU");
        cw.filterHz = 500;
        p.hub.setSlicesForTest({usb, cw});
        QVERIFY(p.local.sliceRoomId(0).isEmpty()); // no band list yet
        emit p.client.roomsReceived(roomsFull());
        QCOMPARE(p.local.sliceRoomId(0), kRoom);
        QCOMPARE(p.local.sliceRoomId(1), kOtherRoom);
        QVERIFY(!changed.isEmpty());
        // Far off: in no room.
        usb.hz = 14250000;
        p.hub.setSlicesForTest({usb, cw});
        QVERIFY(p.local.sliceRoomId(0).isEmpty());
        QCOMPARE(p.local.sliceRoomId(1), kOtherRoom);
    }

    // Nothing reaches a server that is not connected: the verbs that need
    // the station connection are refused in words, a post is answered
    // "not connected" by the client, and a turn is named only with a
    // callsign (5.3.6).
    void theVerbsWaitForTheStationConnection()
    {
        Local p;
        QCOMPARE(p.local.linkState(), RoundtableModel::LinkState::Off);
        QSignalSpy refused(&p.local, &RoundtableModel::actionRefused);
        QSignalSpy posted(&p.local, &RoundtableModel::postResult);
        p.local.post(kRoom, QStringLiteral("hello"), QStringLiteral("c1"));
        QCOMPARE(posted.size(), 1);
        QCOMPARE(posted.at(0).at(0).toString(), QStringLiteral("c1"));
        QCOMPARE(posted.at(0).at(1).toBool(), false);
        p.local.deleteMessage(QStringLiteral("21"));
        p.local.nameTurn(QStringLiteral("11"), QStringLiteral("W1AW"));
        p.local.report(QStringLiteral("message"), QStringLiteral("21"), QString());
        QCOMPARE(refused.size(), 3);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("delete"));
        QCOMPARE(refused.at(1).at(0).toString(), QStringLiteral("name"));
        QCOMPARE(refused.at(2).at(0).toString(), QStringLiteral("report"));
        for (const QList<QVariant>& r : std::as_const(refused)) {
            QCOMPARE(r.at(1).toString(), RoundtableHub::notConnectedReason());
        }
        refused.clear();
        p.local.nameTurn(QStringLiteral("11"), QStringLiteral("not a call"));
        QCOMPARE(refused.size(), 1);
        QCOMPARE(refused.at(0).at(1).toString(),
                 QStringLiteral("Name a turn with the callsign you heard."));
        // A room that is not listed cannot be read.
        p.local.readRoom(kRoom);
        QCOMPARE(refused.size(), 2);
        QVERIFY(p.local.readingRoomId().isEmpty());
        QCOMPARE(p.client.linkState(), RoundtableClient::LinkState::Off);
    }

    // "How do others hear me": the hub reads each listed slice's S-meter
    // and gives it to the client (pushMeter, at most one reading a second
    // goes out, 6.5) and to the panel as its own row. No channel (-200) or
    // a keyed slice gives nothing to show.
    void theMeterFeedsTheClientAndThePanel()
    {
        Local p;
        RoundtableSliceState usb;
        usb.id = 0;
        usb.hz = 14200400;
        usb.mode = QStringLiteral("USB");
        usb.filterHz = 2700;
        p.hub.setSlicesForTest({usb});
        double dbm = -73.0;
        p.hub.setMeterSourceForTest([&dbm](int) { return dbm; });
        QSignalSpy read(&p.hub, &RoundtableHub::sliceMeterRead);
        p.hub.pollMetersForTest();
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.at(0).at(0).toInt(), 0);
        QCOMPARE(read.at(0).at(1).toDouble(), -73.0);
        dbm = -200.0;
        p.hub.pollMetersForTest();
        QCOMPARE(read.size(), 1);
        dbm = -80.0;
        usb.keyed = true;
        p.hub.setSlicesForTest({usb});
        p.hub.pollMetersForTest();
        QCOMPARE(read.size(), 1);
        // And on its own, every kMeterPollMs.
        usb.keyed = false;
        p.hub.setSlicesForTest({usb});
        QTRY_VERIFY_WITH_TIMEOUT(read.size() >= 3, 2000);
        p.hub.setMeterSourceForTest({});
    }
};

QTEST_MAIN(TstRoundtableModel)
#include "tst_roundtable_model.moc"
