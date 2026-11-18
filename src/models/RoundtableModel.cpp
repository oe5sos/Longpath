// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/models/RoundtableModel.cpp  (Longpath)
// =================================================================
//
// See RoundtableModel.h.
//
// Uebernommen aus dem Upstream NereusSDR (`src/models/RoundtableModel.cpp`
// @ e56b3b44c, J.J. Boyd KG4VCF). Longpath-Abweichungen siehe
// RoundtableModel.h: nur attachLocal, RoundtableHub statt RoundtableState,
// kein Schreibbeobachter in AppSettings.
//
// Modification history (NereusSDR):
//   2026-11-02: Separate active-frequency and detail ownership for existing reads.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-30: Bound retained-history validation and correlate its one-shot answers.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-30: Shared miles-default and bearing display preferences.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 11).
//   2026-10-24: Roundtable plan Task 12: available(), muteList(), the
//               Core's Roundtable settings and settingChanged. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-24: Roundtable plan Task 13: earlier turns of the last 30
//               minutes only, pruned on a timer, and the test clock.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-24: Roundtable plan Task 13: recentNames(), the names used on
//               a room's frequency in the last 24 hours (design 8.6).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// Modification history (Longpath):
//   2026-11-18 - Uebernommen fuer Longpath (Roundtable Stufe 2) von
//                Martin Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "models/RoundtableModel.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/roundtable/RoundtableWire.h"
#include "models/RoundtableHub.h"

#include <QDateTime>
#include <QJsonValue>

#include <algorithm>

namespace Longpath {

namespace {

QJsonObject without(QJsonObject object, const QString& key)
{
    object.remove(key);
    return object;
}

qint64 idOf(const QJsonObject& object)
{
    return object.value(QStringLiteral("id")).toInteger();
}

// A turn's names by count, highest first, then by name.
QJsonObject withSortedNames(QJsonObject turn)
{
    QList<QJsonObject> names;
    for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
        names.append(value.toObject());
    }
    std::sort(names.begin(), names.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 ca = a.value(QStringLiteral("count")).toInteger();
        const qint64 cb = b.value(QStringLiteral("count")).toInteger();
        if (ca != cb) {
            return ca > cb;
        }
        return a.value(QStringLiteral("name")).toString() < b.value(QStringLiteral("name")).toString();
    });
    QJsonArray sorted;
    for (const QJsonObject& name : names) {
        sorted.append(name);
    }
    turn.insert(QStringLiteral("names"), sorted);
    return turn;
}

std::function<qint64()>& testClock()
{
    static std::function<qint64()> clock;
    return clock;
}

bool isOpen(const QJsonObject& turn)
{
    // An open turn's end is null (or left out on the way).
    const QJsonValue end = turn.value(QStringLiteral("end"));
    return end.isNull() || end.isUndefined();
}

bool endedBefore(const QJsonObject& turn, qint64 cutoffMs)
{
    return !isOpen(turn) && turn.value(QStringLiteral("end")).toInteger() < cutoffMs;
}

} // namespace

qint64 RoundtableModel::nowMs()
{
    const std::function<qint64()>& clock = testClock();
    return clock ? clock() : QDateTime::currentMSecsSinceEpoch();
}

void RoundtableModel::setClockForTest(std::function<qint64()> clock)
{
    testClock() = std::move(clock);
}

QString RoundtableModel::frequencyContextId(qint64 hz, const QString& family)
{
    return QStringLiteral("frequency:") + family + QLatin1Char('-') + QString::number(hz);
}

bool RoundtableModel::isFrequencyContext(const QString& id)
{
    return id.startsWith(QLatin1String("frequency:"));
}

QStringList RoundtableModel::stationSettingNames()
{
    return {QStringLiteral("AutoStart"), QStringLiteral("AwayChoice"),
            QStringLiteral("HighlightMentions"), QStringLiteral("Callsign"),
            QStringLiteral("GridSquare"), QStringLiteral("DistanceUnits"),
            QStringLiteral("ShowBearing")};
}

QString RoundtableModel::stationSettingDefault(const QString& name)
{
    // The defaults SpotSourceHost and RoundtableClient read (design C15).
    if (name == QLatin1String("AutoStart") || name == QLatin1String("HighlightMentions")
        || name == QLatin1String("ShowBearing")) {
        return QStringLiteral("True");
    }
    // Longpath: km. Upstream's approved default is mi (a US operator);
    // Longpath's operator is in Austria. The panel's menu switches it.
    if (name == QLatin1String("DistanceUnits")) {
        return QStringLiteral("km");
    }
    if (name == QLatin1String("AwayChoice")) {
        return QStringLiteral("Report");
    }
    return {};
}

RoundtableModel::RoundtableModel(QObject* parent)
    : QObject(parent)
{
    // Longpath: AppSettings has no write observer. A setting written here
    // is announced by setStationSetting(); the hub's reload announces the
    // rest (attachLocal).
    m_pruneTimer.setSingleShot(true);
    connect(&m_pruneTimer, &QTimer::timeout, this, &RoundtableModel::pruneEarlierTurns);
}

RoundtableModel::~RoundtableModel() = default;

void RoundtableModel::detach()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        disconnect(connection);
    }
    m_connections.clear();
    m_client = nullptr;
    m_hub = nullptr;
    const bool hadRooms = !m_summaries.isEmpty();
    const QStringList viewed = viewedRoomIds();
    m_summaries.clear();
    m_views.clear();
    m_readingRoomId.clear();
    m_activeFrequencyContext.clear();
    if (hadRooms) {
        emit roomsChanged();
    }
    for (const QString& id : viewed) {
        emit roomChanged(id);
    }
}

void RoundtableModel::attachLocal(RoundtableClient* client, RoundtableHub* hub)
{
    detach();
    m_client = client;
    if (client != nullptr) {
        m_connections.append(connect(client, &RoundtableClient::roomsReceived, this,
                                     &RoundtableModel::applyRooms));
        m_connections.append(connect(client, &RoundtableClient::roomsDeltaReceived, this,
                                     &RoundtableModel::applyRoomsDelta));
        m_connections.append(connect(client, &RoundtableClient::roomReceived, this,
                                     &RoundtableModel::applyRoom));
        m_connections.append(connect(client, &RoundtableClient::roomDeltaReceived, this,
                                     &RoundtableModel::applyRoomDelta));
        m_connections.append(connect(client, &RoundtableClient::historyResult, this,
                                     &RoundtableModel::applyHistoryResult));
        m_connections.append(connect(client, &RoundtableClient::postResult, this,
                                     &RoundtableModel::postResult));
        m_connections.append(connect(client, &RoundtableClient::webLinkReady, this,
                                     &RoundtableModel::webLinkReady));
        m_connections.append(connect(client, &RoundtableClient::errorReceived, this,
                                     [this](const QString& code, const QString& reason) {
            // The archive's own failures (the key, the server out of
            // reach) answer the web sign-in or archive request.
            if (code == QLatin1String("no-key") || code == QLatin1String("unreachable")) {
                refuse(QStringLiteral("webSignIn"), reason);
            }
        }));
    }
    connectHub(hub);
}

void RoundtableModel::connectHub(RoundtableHub* hub)
{
    m_hub = hub;
    if (hub != nullptr) {
        m_connections.append(connect(hub, &RoundtableHub::stateChanged, this,
                                     &RoundtableModel::onStateChanged));
        // Longpath: the hub reread Roundtable/* (a Spot Hub write, an
        // answer, a mute): possibly any setting changed.
        m_connections.append(connect(hub, &RoundtableHub::settingsReloaded, this,
                                     [this]() { noteSettingChanged(QString()); }));
    }
    onStateChanged();
}

void RoundtableModel::onStateChanged()
{
    const LinkState link = linkState();
    const qint64 since = linkSince();
    const bool firstRun = firstRunAnswered();
    const bool isHidden = hidden();
    const QString mentions = m_hub ? m_hub->mentionCounts() : QString();
    const QString sliceRooms = m_hub ? m_hub->sliceRooms() : QString();
    if (link != m_seenLinkState || since != m_seenLinkSince || isHidden != m_seenHidden) {
        m_seenLinkState = link;
        m_seenLinkSince = since;
        m_seenHidden = isHidden;
        emit linkChanged();
    }
    if (firstRun != m_seenFirstRun) {
        m_seenFirstRun = firstRun;
        emit firstRunChanged();
    }
    if (mentions != m_seenMentions) {
        m_seenMentions = mentions;
        emit mentionsChanged();
    }
    if (sliceRooms != m_seenSliceRooms) {
        m_seenSliceRooms = sliceRooms;
        emit sliceRoomsChanged();
    }
}

// ---------------------------------------------------------------- views

void RoundtableModel::applyRooms(const QJsonObject& full)
{
    m_summaries.clear();
    for (const QJsonValue& value : full.value(QStringLiteral("rooms")).toArray()) {
        const QJsonObject summary = value.toObject();
        const QString id = summary.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            m_summaries.insert(id, summary);
        }
    }
    emit roomsChanged();
}

void RoundtableModel::applyRoomsDelta(const QJsonObject& delta)
{
    for (const QString& key : {QStringLiteral("add"), QStringLiteral("change")}) {
        for (const QJsonValue& value : delta.value(key).toArray()) {
            const QJsonObject summary = value.toObject();
            const QString id = summary.value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) {
                m_summaries.insert(id, summary);
            }
        }
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("remove")).toArray()) {
        m_summaries.remove(value.toString());
    }
    emit roomsChanged();
}

void RoundtableModel::upsertMember(RoomView& view, const QJsonObject& member, bool scoped)
{
    const QString wireRef = member.value(QStringLiteral("ref")).toString();
    const QString source = scoped ? member.value(QStringLiteral("sourceRoom")).toString() : QString();
    const QString ref = source.isEmpty() ? wireRef : source + QLatin1Char('/') + wireRef;
    if (wireRef.isEmpty()) {
        return;
    }
    QJsonObject stored = member;
    const auto held = view.members.constFind(ref);
    // A row keeps its place in joining order while it stays (6.8).
    stored.insert(QStringLiteral("seq"), held != view.members.cend()
                                             ? held->value(QStringLiteral("seq")).toInteger()
                                             : view.nextSeq++);
    view.members.insert(ref, stored);
}

void RoundtableModel::trimMessages(RoomView& view)
{
    if (view.messages.size() <= kRoomMessages) {
        return;
    }
    QList<qint64> ids = view.messages.keys();
    std::sort(ids.begin(), ids.end());
    for (qsizetype i = 0; i < ids.size() - kRoomMessages; ++i) {
        view.messages.remove(ids.at(i));
    }
}

void RoundtableModel::applyRoom(const QJsonObject& full)
{
    const QString id = (full.value(QStringLiteral("view")).toString() == QLatin1String("frequency")
                        ? QStringLiteral("frequency:") : QString())
                       + full.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        return;
    }
    RoomView view;
    for (const QJsonValue& value : full.value(QStringLiteral("members")).toArray()) {
        upsertMember(view, value.toObject(), full.value(QStringLiteral("view")).toString() == QLatin1String("frequency"));
    }
    for (const QString& key : {QStringLiteral("now"), QStringLiteral("earlier")}) {
        for (const QJsonValue& value : full.value(key).toArray()) {
            const QJsonObject turn = value.toObject();
            if (idOf(turn) > 0) {
                view.turns.insert(idOf(turn), turn);
            }
        }
    }
    for (const QJsonValue& value : full.value(QStringLiteral("messages")).toArray()) {
        const QJsonObject message = value.toObject();
        if (idOf(message) > 0) {
            view.messages.insert(idOf(message), message);
        }
    }
    trimMessages(view);
    noteRecentNames(id, view);
    pruneView(view, nowMs() - kEarlierWindowMs);
    m_views.insert(id, view);
    schedulePrune();
    emit roomChanged(id);
}

void RoundtableModel::applyRoomDelta(const QJsonObject& delta)
{
    const QString id = (delta.value(QStringLiteral("view")).toString() == QLatin1String("frequency")
                        ? QStringLiteral("frequency:") : QString())
                       + delta.value(QStringLiteral("room")).toString();
    const auto it = m_views.find(id);
    if (it == m_views.end()) {
        return; // the client asks again for a view it lost
    }
    if (delta.value(QStringLiteral("closed")).toBool()) {
        m_views.erase(it);
        if (m_readingRoomId == id) {
            m_readingRoomId.clear();
            emit readingRoomChanged();
        }
        emit roomChanged(id);
        return;
    }
    RoomView& view = it.value();
    for (const QJsonValue& value : delta.value(QStringLiteral("members")).toArray()) {
        upsertMember(view, value.toObject(), delta.value(QStringLiteral("view")).toString() == QLatin1String("frequency"));
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("left")).toArray()) {
        view.members.remove(value.toString());
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("turns")).toArray()) {
        const QJsonObject turn = value.toObject();
        if (idOf(turn) > 0) {
            view.turns.insert(idOf(turn), turn);
        }
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("messages")).toArray()) {
        const QJsonObject message = value.toObject();
        if (idOf(message) > 0) {
            view.messages.insert(idOf(message), message);
        }
    }
    trimMessages(view);
    noteRecentNames(id, view);
    pruneView(view, nowMs() - kEarlierWindowMs);
    schedulePrune();
    emit roomChanged(id);
}

// ---------------------------------------------------------------- streams

// ---------------------------------------------------------------- readers

QJsonArray RoundtableModel::rooms() const
{
    QList<QJsonObject> list = m_summaries.values();
    std::sort(list.begin(), list.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 ha = a.value(QStringLiteral("hz")).toInteger();
        const qint64 hb = b.value(QStringLiteral("hz")).toInteger();
        if (ha != hb) {
            return ha < hb;
        }
        return a.value(QStringLiteral("id")).toString() < b.value(QStringLiteral("id")).toString();
    });
    QJsonArray out;
    for (const QJsonObject& summary : list) {
        out.append(summary);
    }
    return out;
}

QJsonObject RoundtableModel::room(const QString& roomId) const
{
    const auto it = m_views.constFind(roomId);
    if (it == m_views.cend()) {
        if (roomId == m_readingRoomId && isFrequencyContext(roomId)) {
            const QString context = roomId.mid(10);
            const qsizetype dash = context.indexOf(QLatin1Char('-'));
            const qint64 hz = context.mid(dash + 1).toLongLong();
            return {{QStringLiteral("id"), context}, {QStringLiteral("hz"), hz},
                {QStringLiteral("family"), context.left(dash)},
                {QStringLiteral("band"), RoundtableWire::bandOf(hz)},
                {QStringLiteral("view"), QStringLiteral("frequency")},
                {QStringLiteral("members"), QJsonArray{}}, {QStringLiteral("now"), QJsonArray{}},
                {QStringLiteral("earlier"), QJsonArray{}}, {QStringLiteral("messages"), QJsonArray{}}};
        }
        return {};
    }
    const RoomView& view = it.value();
    // The head comes from the id (6.3), so both placements agree.
    const QString context = isFrequencyContext(roomId) ? roomId.mid(10) : roomId;
    const qsizetype dash = context.indexOf(QLatin1Char('-'));
    const QString family = dash > 0 ? context.left(dash) : QString();
    const qint64 hz = dash > 0 ? context.mid(dash + 1).toLongLong() : 0;

    QList<QJsonObject> members = view.members.values();
    std::sort(members.begin(), members.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value(QStringLiteral("seq")).toInteger() < b.value(QStringLiteral("seq")).toInteger();
    });
    QJsonArray memberArray;
    for (const QJsonObject& member : members) {
        memberArray.append(without(member, QStringLiteral("seq")));
    }

    QList<QJsonObject> open;
    QList<QJsonObject> ended;
    // Wire spec 5.4.6: only the turns that ended in the last 30 minutes,
    // even between the prune timer's runs.
    const qint64 cutoff = nowMs() - kEarlierWindowMs;
    for (const QJsonObject& turn : view.turns) {
        if (isOpen(turn)) {
            open.append(turn);
        } else if (!endedBefore(turn, cutoff)) {
            ended.append(turn);
        }
    }
    std::sort(open.begin(), open.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 sa = a.value(QStringLiteral("start")).toInteger();
        const qint64 sb = b.value(QStringLiteral("start")).toInteger();
        return sa != sb ? sa < sb : idOf(a) < idOf(b);
    });
    std::sort(ended.begin(), ended.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 ea = a.value(QStringLiteral("end")).toInteger();
        const qint64 eb = b.value(QStringLiteral("end")).toInteger();
        return ea != eb ? ea > eb : idOf(a) > idOf(b);
    });
    QJsonArray now;
    for (const QJsonObject& turn : open) {
        now.append(withSortedNames(turn));
    }
    QJsonArray earlier;
    for (const QJsonObject& turn : ended) {
        earlier.append(withSortedNames(turn));
    }

    QList<qint64> messageIds = view.messages.keys();
    std::sort(messageIds.begin(), messageIds.end());
    QJsonArray messages;
    for (const qint64 id : messageIds) {
        messages.append(view.messages.value(id));
    }

    QJsonObject result{{QStringLiteral("id"), context},
                       {QStringLiteral("band"), RoundtableWire::bandOf(hz)},
                       {QStringLiteral("family"), family},
                       {QStringLiteral("hz"), hz},
                       {QStringLiteral("members"), memberArray},
                       {QStringLiteral("now"), now},
                       {QStringLiteral("earlier"), earlier},
                       {QStringLiteral("messages"), messages}};
    if (isFrequencyContext(roomId)) {
        result.insert(QStringLiteral("view"), QStringLiteral("frequency"));
    }
    return result;
}

QStringList RoundtableModel::viewedRoomIds() const
{
    QStringList ids = m_views.keys();
    std::sort(ids.begin(), ids.end());
    return ids;
}

QString RoundtableModel::sliceRoomId(int sliceId) const
{
    return m_hub ? m_hub->sliceRoomId(sliceId) : QString();
}

RoundtableModel::LinkState RoundtableModel::linkState() const
{
    return m_hub ? m_hub->linkState() : LinkState::Off;
}

qint64 RoundtableModel::linkSince() const
{
    return m_hub ? m_hub->linkSince() : 0;
}

bool RoundtableModel::firstRunAnswered() const
{
    return m_hub && m_hub->firstRunAnswered();
}

bool RoundtableModel::hidden() const
{
    return m_hub && m_hub->hidden();
}

int RoundtableModel::mentionCount(const QString& roomId) const
{
    return m_hub ? m_hub->mentionCount(roomId) : 0;
}

bool RoundtableModel::muted(const QString& callsign) const
{
    return m_hub && m_hub->muted(callsign);
}

QStringList RoundtableModel::muteList() const
{
    return m_hub ? m_hub->muteList() : QStringList();
}

bool RoundtableModel::available() const
{
    return !m_client.isNull() && !m_hub.isNull();
}

QString RoundtableModel::stationSetting(const QString& name) const
{
    return AppSettings::instance()
        .value(QStringLiteral("Roundtable/") + name, stationSettingDefault(name))
        .toString();
}

bool RoundtableModel::setStationSetting(const QString& name, const QString& value)
{
    if (!stationSettingNames().contains(name)) {
        qCWarning(lcSpots) << "Roundtable setting not written:" << name
                         << "changes only through Roundtable";
        return false;
    }
    if ((name == QLatin1String("DistanceUnits") && value != QLatin1String("mi")
         && value != QLatin1String("km"))
        || (name == QLatin1String("ShowBearing") && value != QLatin1String("True")
            && value != QLatin1String("False"))) {
        return false;
    }
    auto& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("Roundtable/") + name, value);
    settings.save();
    // Longpath: no write observer. The hub rereads (callsign, grid, away
    // choice and on/off reach the client) and announces it; without one,
    // the change is announced here.
    if (m_hub) {
        m_hub->reloadSettings();
    } else {
        noteSettingChanged(QStringLiteral("Roundtable/") + name);
    }
    return true;
}

void RoundtableModel::noteSettingChanged(const QString& key)
{
    if (key.isEmpty() || key.startsWith(QLatin1String("Roundtable/"), Qt::CaseInsensitive)) {
        emit settingChanged(key);
    }
}

void RoundtableModel::setDraft(const QString& roomId, const QString& text)
{
    if (text.isEmpty()) {
        m_drafts.remove(roomId);
    } else {
        m_drafts.insert(roomId, text);
    }
}

bool RoundtableModel::frequencyAvailable() const
{
    return m_client && m_client->frequencyVersion() >= 1;
}

bool RoundtableModel::requestHistory(const QJsonArray& ids, const QString& clientId)
{
    QString reason;
    if (!m_hub || !m_hub->requestHistory(ids, clientId, &reason)) {
        emit historyResult(clientId, false, {}, reason);
        return false;
    }
    return true;
}

void RoundtableModel::applyHistoryResult(const QString& clientId, bool ok, const QJsonArray& states,
                                        const QString& reason)
{
    emit historyResult(clientId, ok, ok ? states : QJsonArray{}, reason);
}

bool RoundtableModel::postAt(int sliceId, qint64 hz, const QString& family,
                            const QString& text, const QString& clientId)
{
    QString reason;
    if (!m_hub || !m_hub->postAt(sliceId, hz, family, text, clientId, &reason)) {
        refuse(QStringLiteral("postAt"), reason);
        return false;
    }
    return true;
}

bool RoundtableModel::requestFrequencyRead(qint64 hz, const QString& family)
{
    QString reason;
    const bool accepted = m_hub && m_hub->readAt(hz, family, &reason);
    if (!accepted) { refuse(QStringLiteral("readAt"), reason); }
    return accepted;
}

void RoundtableModel::releaseFrequencyRead(const QString& context)
{
    if (!isFrequencyContext(context)) { return; }
    const QString address = context.mid(10);
    const qsizetype dash = address.indexOf(QLatin1Char('-'));
    const qint64 hz = address.mid(dash + 1).toLongLong();
    const QString family = address.left(dash);
    if (m_hub) {
        m_hub->unreadAt(hz, family);
    }
}

QStringList RoundtableModel::wantedFrequencyContexts() const
{
    QStringList contexts;
    if (isFrequencyContext(m_activeFrequencyContext)) { contexts.append(m_activeFrequencyContext); }
    if (isFrequencyContext(m_readingRoomId) && !contexts.contains(m_readingRoomId)) { contexts.append(m_readingRoomId); }
    return contexts;
}

bool RoundtableModel::setActiveFrequencyContext(qint64 hz, const QString& family)
{
    const QString id = hz > 0 && !family.isEmpty() ? frequencyContextId(hz, family) : QString();
    if (id == m_activeFrequencyContext) { return true; }
    if (!id.isEmpty() && id != m_readingRoomId && !requestFrequencyRead(hz, family)) { return false; }
    const QString previous = m_activeFrequencyContext;
    m_activeFrequencyContext = id;
    if (!previous.isEmpty() && previous != m_readingRoomId) { releaseFrequencyRead(previous); }
    return true;
}

bool RoundtableModel::readAt(qint64 hz, const QString& family)
{
    const QString id = frequencyContextId(hz, family);
    if (m_readingRoomId == id) { return true; }
    if (id != m_activeFrequencyContext && !requestFrequencyRead(hz, family)) { return false; }
    const QString previous = m_readingRoomId;
    m_readingRoomId = id;
    if (isFrequencyContext(previous) && previous != m_activeFrequencyContext) { releaseFrequencyRead(previous); }
    emit readingRoomChanged();
    return true;
}

void RoundtableModel::unreadAt(qint64 hz, const QString& family)
{
    const QString id = frequencyContextId(hz, family);
    if (id != m_activeFrequencyContext) { releaseFrequencyRead(id); }
    if (m_readingRoomId == id) {
        m_readingRoomId.clear();
        emit readingRoomChanged();
    }
}

// ---------------------------------------------------------------- actions

void RoundtableModel::refuse(const QString& verb, const QString& reason)
{
    emit actionRefused(verb, reason);
}

namespace {

QString notRunningReason()
{
    return QStringLiteral("Roundtable is not running here.");
}

} // namespace

void RoundtableModel::post(const QString& roomId, const QString& text, const QString& clientId)
{
    QString reason;
    if (!m_client || !m_hub) {
        emit postResult(clientId, false, notRunningReason());
        return;
    }
    // The client answers through postResult.
    if (!m_hub->post(roomId, text, clientId, &reason)) {
        emit postResult(clientId, false, reason);
    }
}

void RoundtableModel::deleteMessage(const QString& messageId)
{
    QString reason;
    if (!m_hub || !m_hub->deleteMessage(messageId, &reason)) {
        refuse(QStringLiteral("delete"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::nameTurn(const QString& turnId, const QString& name)
{
    QString reason;
    if (!m_hub || !m_hub->nameTurn(turnId, name, &reason)) {
        refuse(QStringLiteral("name"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::report(const QString& kind, const QString& target, const QString& reason)
{
    QString refusal;
    if (!m_hub || !m_hub->report(kind, target, reason, &refusal)) {
        refuse(QStringLiteral("report"), m_hub ? refusal : notRunningReason());
    }
}

void RoundtableModel::readRoom(const QString& roomId)
{
    if (roomId == m_readingRoomId) {
        return;
    }
    if (!m_readingRoomId.isEmpty()) {
        unreadRoom();
    }
    QString reason;
    bool sent = false;
    if (m_hub && m_hub->readRoom(roomId, &reason)) {
        sent = true;
    } else {
        refuse(QStringLiteral("read"), m_hub ? reason : notRunningReason());
    }
    if (sent) {
        m_readingRoomId = roomId;
        emit readingRoomChanged();
    }
}

void RoundtableModel::unreadRoom()
{
    if (m_readingRoomId.isEmpty()) {
        return;
    }
    if (isFrequencyContext(m_readingRoomId)) {
        const QString previous = m_readingRoomId.mid(10);
        const qsizetype dash = previous.indexOf(QLatin1Char('-'));
        unreadAt(previous.mid(dash + 1).toLongLong(), previous.left(dash));
        return;
    }
    const QString roomId = m_readingRoomId;
    m_readingRoomId.clear();
    if (m_hub) {
        m_hub->unreadRoom(roomId);
    }
    emit readingRoomChanged();
}

void RoundtableModel::setHidden(bool isHidden)
{
    QString reason;
    if (!m_hub || !m_hub->setOperatorHidden(isHidden, &reason)) {
        refuse(QStringLiteral("setHidden"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::answerFirstRun(bool live)
{
    QString reason;
    if (!m_hub || !m_hub->answerFirstRun(live, &reason)) {
        refuse(QStringLiteral("answerFirstRun"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::mute(const QString& callsign)
{
    QString reason;
    if (!m_hub || !m_hub->mute(callsign, &reason)) {
        refuse(QStringLiteral("mute"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::unmute(const QString& callsign)
{
    QString reason;
    if (!m_hub || !m_hub->unmute(callsign, &reason)) {
        refuse(QStringLiteral("unmute"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::markMentionRead(qint64 upToMs)
{
    QString reason;
    if (!m_hub || !m_hub->markMentionRead(upToMs, &reason)) {
        refuse(QStringLiteral("markMentionRead"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::requestWebLink()
{
    QString reason;
    if (!m_hub || !m_hub->requestWebLink(&reason)) {
        refuse(QStringLiteral("webSignIn"), m_hub ? reason : notRunningReason());
    }
}

void RoundtableModel::deleteArchive(const QString& confirm)
{
    QString reason;
    if (!m_hub || !m_hub->deleteArchive(confirm, &reason)) {
        refuse(QStringLiteral("deleteArchive"), m_hub ? reason : notRunningReason());
    }
}

// ---------------------------------------------------------------- earlier

bool RoundtableModel::pruneView(RoomView& view, qint64 cutoffMs)
{
    bool pruned = false;
    for (auto it = view.turns.begin(); it != view.turns.end();) {
        if (endedBefore(it.value(), cutoffMs)) {
            it = view.turns.erase(it);
            pruned = true;
        } else {
            ++it;
        }
    }
    return pruned;
}

void RoundtableModel::noteRecentNames(const QString& roomId, const RoomView& view)
{
    const qint64 now = nowMs();
    QHash<QString, RecentName>& names = m_recentNames[roomId];
    const auto note = [&names](const QString& given, qint64 usedMs) {
        const QString name = given.trimmed().toUpper();
        const QString base = RoundtableWire::baseCallsign(name);
        if (base.isEmpty()) {
            return;
        }
        RecentName& entry = names[base];
        if (usedMs >= entry.usedMs) {
            entry.name = name;
            entry.usedMs = usedMs;
        }
    };
    for (const QJsonObject& turn : view.turns) {
        const qint64 usedMs = isOpen(turn) ? now : turn.value(QStringLiteral("end")).toInteger();
        for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
            note(value.toObject().value(QStringLiteral("callsign")).toString(), usedMs);
        }
        for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
            note(value.toObject().value(QStringLiteral("name")).toString(), usedMs);
        }
    }
    const qint64 cutoff = now - kRecentNamesWindowMs;
    for (auto it = names.begin(); it != names.end();) {
        if (it->usedMs < cutoff) {
            it = names.erase(it);
        } else {
            ++it;
        }
    }
    if (names.isEmpty()) {
        m_recentNames.remove(roomId);
    }
}

QStringList RoundtableModel::recentNames(const QString& roomId) const
{
    const qint64 cutoff = nowMs() - kRecentNamesWindowMs;
    QList<RecentName> recent;
    for (const RecentName& entry : m_recentNames.value(roomId)) {
        if (entry.usedMs >= cutoff) {
            recent.append(entry);
        }
    }
    std::sort(recent.begin(), recent.end(), [](const RecentName& a, const RecentName& b) {
        return a.usedMs != b.usedMs ? a.usedMs > b.usedMs : a.name < b.name;
    });
    QStringList out;
    for (const RecentName& entry : recent) {
        out.append(entry.name);
    }
    return out;
}

void RoundtableModel::pruneEarlierTurns()
{
    const qint64 cutoff = nowMs() - kEarlierWindowMs;
    QStringList changed;
    for (auto it = m_views.begin(); it != m_views.end(); ++it) {
        if (pruneView(it.value(), cutoff)) {
            changed.append(it.key());
        }
    }
    schedulePrune();
    std::sort(changed.begin(), changed.end());
    for (const QString& id : std::as_const(changed)) {
        emit roomChanged(id);
    }
}

void RoundtableModel::schedulePrune()
{
    qint64 oldestEnd = -1;
    for (const RoomView& view : std::as_const(m_views)) {
        for (const QJsonObject& turn : view.turns) {
            if (isOpen(turn)) {
                continue;
            }
            const qint64 end = turn.value(QStringLiteral("end")).toInteger();
            if (oldestEnd < 0 || end < oldestEnd) {
                oldestEnd = end;
            }
        }
    }
    if (oldestEnd < 0) {
        m_pruneTimer.stop();
        return;
    }
    // A second past the moment the oldest leaves the window, at least a
    // second from now.
    const qint64 wait = std::clamp<qint64>(oldestEnd + kEarlierWindowMs - nowMs() + 1000, 1000,
                                           kEarlierWindowMs);
    m_pruneTimer.start(static_cast<int>(wait));
}

} // namespace Longpath
