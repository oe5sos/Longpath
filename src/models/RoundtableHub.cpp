// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/models/RoundtableHub.cpp  (Longpath)
// =================================================================
//
// Siehe RoundtableHub.h.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude. modeOf() und die
//                Reihenfolge in sliceStates() aus Upstream NereusSDR
//                RadioModel::roundtableModeOf / pushRoundtableSlices
//                (src/models/RadioModel.cpp @ 9036ffc04).
//   2026-11-18 - Stufe 2: Zustand und Verben aus Upstream
//                RoundtableState.cpp @ e56b3b44c (Core-Haelfte: onMention,
//                recount, replace, die Verben mit ihren Gruenden), die
//                Messwerte und das RoundtableModel. Martin Fischer
//                (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen und angepasst.

#include "models/RoundtableHub.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableKey.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

const QString kSource = QStringLiteral("Roundtable");

QString setting(const char* key, const QString& fallback = QString())
{
    return AppSettings::instance().value(QLatin1String(key), fallback).toString();
}

// Die Familie (Protokoll 6.1), wie die Spot-Liste eine Betriebsart zeigt.
QString spotModeOf(const QString& family)
{
    static const QHash<QString, QString> modes{
        {QStringLiteral("usb"), QStringLiteral("USB")},   {QStringLiteral("lsb"), QStringLiteral("LSB")},
        {QStringLiteral("cw"), QStringLiteral("CW")},     {QStringLiteral("am"), QStringLiteral("AM")},
        {QStringLiteral("fm"), QStringLiteral("FM")},     {QStringLiteral("digu"), QStringLiteral("DIGU")},
        {QStringLiteral("digl"), QStringLiteral("DIGL")}, {QStringLiteral("fdvu"), QStringLiteral("RADE")},
        {QStringLiteral("fdvl"), QStringLiteral("RADE")},
    };
    return modes.value(family, family.toUpper());
}

QString compact(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// Wire spec 5.2: an id is a whole number from 1 to 9007199254740991.
bool isWireId(const QString& text)
{
    bool ok = false;
    const qint64 id = text.toLongLong(&ok);
    return ok && id >= 1 && id <= 9007199254740991LL && QString::number(id) == text;
}

constexpr qint64 kMaxUnixMs = 9007199254740991LL;

// Der Zeitstempel eines Messwerts: eine monotone Uhr in ns, wie
// RxChannel::meterClockNs bei Upstream. Jeder Lesedurchgang ist ein neuer
// Wert (WDSP rechnet den Pegel je Puffer, also oefter als wir lesen).
qint64 meterClockNs()
{
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    return clock.nsecsElapsed() + 1;
}

} // namespace

RoundtableHub::RoundtableHub(QObject* parent)
    : QObject(parent)
    , m_client(std::make_unique<RoundtableClient>())
    , m_model(std::make_unique<RoundtableModel>())
{
    // Der Schluessel wird erst gebraucht, wenn eine Verbindung der
    // Station aufgeht — vorher entsteht keine Datei.
    m_client->setIdentityProvider([this]() { return key(); });
    connect(m_client.get(), &RoundtableClient::roomsReceived, this, &RoundtableHub::onRooms);
    connect(m_client.get(), &RoundtableClient::roomsDeltaReceived, this,
            &RoundtableHub::onRoomsDelta);
    connect(m_client.get(), &RoundtableClient::linkStateChanged, this,
            &RoundtableHub::onLinkStateChanged);
    connect(m_client.get(), &RoundtableClient::statusTextChanged, this,
            &RoundtableHub::statusChanged);
    connect(m_client.get(), &RoundtableClient::mentionReceived, this,
            &RoundtableHub::onMention);

    // Die Raumliste meldet nur Aenderungen; ohne Auffrischen liefen die
    // Spots einer ruhigen Station nach kSpotLifetimeSec ab.
    m_refresh.setInterval(kSpotRefreshMs);
    connect(&m_refresh, &QTimer::timeout, this, [this]() { syncSpots(true); });

    // Messwerte: nur solange eine Scheibe gemeldet wird und Roundtable an
    // ist (updateMeterPolling).
    m_meterPoll.setInterval(kMeterPollMs);
    connect(&m_meterPoll, &QTimer::timeout, this, &RoundtableHub::pollMeters);

    // Die Stummliste steht in den Einstellungen; ohne reloadSettings()
    // (das startet den Client) schon hier gelesen.
    for (const QString& part : setting(kMuteListKey).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString base = RoundtableWire::baseCallsign(part.trimmed());
        if (!base.isEmpty() && !m_muteList.contains(base)) {
            m_muteList.append(base);
        }
    }
    m_model->attachLocal(m_client.get(), this);
}

RoundtableHub::~RoundtableHub()
{
    // Erst das Modell (es haengt an beiden), dann den Client (er schickt
    // beim Abbau noch), dann den Schluessel.
    m_model.reset();
    m_client.reset();
}

// ----------------------------------------------------------------- radio

void RoundtableHub::attachRadio(RadioModel* radio)
{
    m_radio = radio;
    if (radio == nullptr) {
        return;
    }
    for (SliceModel* slice : radio->slices()) {
        watchSlice(slice);
    }
    connect(radio, &RadioModel::sliceAdded, this, [this](int) {
        if (m_radio) {
            for (SliceModel* slice : m_radio->slices()) {
                watchSlice(slice);
            }
        }
        pushSlices();
    });
    connect(radio, &RadioModel::sliceRemoved, this, &RoundtableHub::pushSlices);
    connect(radio, &RadioModel::activeSliceChanged, this, &RoundtableHub::pushSlices);
    if (MoxController* mox = radio->moxController()) {
        // Das Argument, nicht isMox(): das Signal kommt am Ende des
        // Umschaltens, und nur sein Wert ist dann sicher der neue.
        connect(mox, &MoxController::moxStateChanged, this, [this](bool on) {
            m_keyed = on;
            pushSlices();
        });
    }
    pushSlices();
}

void RoundtableHub::watchSlice(SliceModel* slice)
{
    if (slice == nullptr) {
        return;
    }
    connect(slice, &SliceModel::frequencyChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
    connect(slice, &SliceModel::dspModeChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
    connect(slice, &SliceModel::filterChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
    connect(slice, &SliceModel::txSliceChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
}

QString RoundtableHub::modeOf(DSPMode mode)
{
    // Upstream RadioModel.cpp roundtableModeOf [@9036ffc04]; RADE2 gibt es
    // bei uns nicht.
    switch (mode) {
    case DSPMode::LSB:
        return QStringLiteral("LSB");
    case DSPMode::USB:
        return QStringLiteral("USB");
    case DSPMode::DIGL:
        return QStringLiteral("DIGL");
    case DSPMode::DIGU:
        return QStringLiteral("DIGU");
    case DSPMode::AM:
        return QStringLiteral("AM");
    case DSPMode::SAM:
        return QStringLiteral("SAM");
    case DSPMode::FM:
        return QStringLiteral("FM");
    case DSPMode::CWL:
        return QStringLiteral("CWL");
    case DSPMode::CWU:
        return QStringLiteral("CWU");
    case DSPMode::RADE_U:
        return QStringLiteral("FDVU");
    case DSPMode::RADE_L:
        return QStringLiteral("FDVL");
    case DSPMode::DSB:
    case DSPMode::SPEC:
    case DSPMode::DRM:
        break;
    }
    return {};
}

QList<RoundtableSliceState> RoundtableHub::sliceStates(const QList<SliceModel*>& slices,
                                                       SliceModel* active, SliceModel* keyed)
{
    QList<RoundtableSliceState> states;
    const auto add = [&states, keyed](SliceModel* slice) {
        // Ein fremder Empfaenger (KiwiSDR, SunSDR ueber TCI) ist nicht
        // diese Station — er wird nicht gemeldet.
        if (slice == nullptr || slice->suppressAutoStreamBinding()) {
            return;
        }
        RoundtableSliceState state;
        state.id = slice->sliceIndex();
        state.hz = std::llround(slice->frequency());
        state.mode = modeOf(slice->dspMode());
        state.filterHz = qAbs(slice->filterHigh() - slice->filterLow());
        state.keyed = slice == keyed;
        states.append(state);
    };
    // Upstream: die aktive Scheibe zuerst.
    add(active);
    for (SliceModel* slice : slices) {
        if (slice != active) {
            add(slice);
        }
    }
    return states;
}

void RoundtableHub::pushSlices()
{
    if (!m_radio) {
        return;
    }
    SliceModel* keyed = m_keyed ? m_radio->txBoundSlice() : nullptr;
    m_sliceStates = sliceStates(m_radio->slices(), m_radio->activeSlice(), keyed);
    m_client->setSlices(m_sliceStates);
    placeSlices();
    updateMeterPolling();
}

// ----------------------------------------------------------------- meters

void RoundtableHub::setMeterSourceForTest(std::function<double(int)> source)
{
    m_meterSource = std::move(source);
    updateMeterPolling();
}

void RoundtableHub::setSlicesForTest(const QList<RoundtableSliceState>& slices)
{
    m_sliceStates = slices;
    m_client->setSlices(m_sliceStates);
    placeSlices();
    updateMeterPolling();
}

void RoundtableHub::updateMeterPolling()
{
    // Gelesen wird, solange es eine gemeldete Scheibe gibt und Roundtable
    // an ist -- auch vor der Erstantwort und verborgen: dann geht nichts
    // hinaus (der Client schickt nur ueber die Stationsverbindung), aber
    // das Panel zeigt den eigenen Wert als "NOT SENT".
    const bool want = enabled() && !m_sliceStates.isEmpty() && (m_radio || m_meterSource);
    if (want && !m_meterPoll.isActive()) {
        m_meterPoll.start();
    } else if (!want && m_meterPoll.isActive()) {
        m_meterPoll.stop();
    }
}

void RoundtableHub::pollMeters()
{
    const qint64 stamp = meterClockNs();
    for (const RoundtableSliceState& slice : std::as_const(m_sliceStates)) {
        double dbm = 0.0;
        if (m_meterSource) {
            dbm = m_meterSource(slice.id);
        } else if (m_radio) {
            dbm = m_radio->rxSignalDbm(slice.id);
        } else {
            continue;
        }
        // Kein Kanal (-200), ein geleerter (-400) oder Unsinn: kein Wert.
        // Der Client wuerde ihn ohnehin nicht schicken (5.3.3).
        if (!std::isfinite(dbm) || dbm < RoundtableWire::kMinDbm || dbm > RoundtableWire::kMaxDbm) {
            continue;
        }
        m_client->pushMeter(slice.id, dbm, stamp);
        if (!slice.keyed) {
            emit sliceMeterRead(slice.id, dbm);
        }
    }
}

// ----------------------------------------------------------------- settings

bool RoundtableHub::enabled() const
{
    return setting("Roundtable/AutoStart", QStringLiteral("True")) == QLatin1String("True");
}

bool RoundtableHub::answered() const
{
    return !setting("Roundtable/FirstRunAnswer").isEmpty();
}

bool RoundtableHub::hidden() const
{
    return setting("Roundtable/Hidden", QStringLiteral("False")) == QLatin1String("True");
}

QString RoundtableHub::callsign() const
{
    QString call = setting("Roundtable/Callsign").trimmed();
    if (call.isEmpty()) {
        call = AppSettings::stationCallsign().trimmed();
    }
    return call.toUpper();
}

QString RoundtableHub::grid() const
{
    QString grid = setting("Roundtable/GridSquare").trimmed();
    if (grid.isEmpty()) {
        grid = AppSettings::stationGridSquare().trimmed();
    }
    return grid;
}

QString RoundtableHub::statusText() const
{
    if (!enabled()) {
        return tr("Off");
    }
    if (!answered()) {
        return tr("Waiting for your answer: nothing is sent until then");
    }
    return m_client->statusText();
}

void RoundtableHub::reloadSettings()
{
    const QString url = setting("Roundtable/ServerUrl").trimmed();
    m_client->setServerUrl(url.isEmpty() ? RoundtableClient::defaultServerUrl() : QUrl(url));
    m_client->setStationInfo(callsign(), grid());
    m_client->setStopWhenAway(setting("Roundtable/AwayChoice", QStringLiteral("Report"))
                              == QLatin1String("Stop"));
    // Erst verborgen, dann die Antwort: wer "verborgen" antwortet, geht
    // nie ueber eine Verbindung der Station (wie Upstreams answerFirstRun).
    m_client->setHidden(hidden());
    m_client->setFirstRunAnswered(answered());
    // Stufe 2: Stummliste und Lesemarke (Upstream RoundtableState::
    // reloadSettings).
    bool okSeen = false;
    const qint64 seen = setting(kMentionsSeenKey).toLongLong(&okSeen);
    if (okSeen && seen > 0) {
        m_client->markMentionsSeen(seen);
    }
    m_mentionsSeenMs = okSeen ? std::max<qint64>(0, seen) : 0;
    QStringList mutes;
    for (const QString& part : setting(kMuteListKey).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString base = RoundtableWire::baseCallsign(part.trimmed());
        if (!base.isEmpty() && !mutes.contains(base)) {
            mutes.append(base);
        }
    }
    m_muteList = mutes;
    recount();
    if (enabled()) {
        const RoundtableClient::LinkState link = m_client->linkState();
        if (link == RoundtableClient::LinkState::Off
            || link == RoundtableClient::LinkState::Offline) {
            m_client->start();
        }
    } else {
        m_client->stop();
        clearStations();
    }
    updateMeterPolling();
    emit statusChanged();
    emit stateChanged();
    emit settingsReloaded();
}

void RoundtableHub::answerFirstRun(bool live)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Roundtable/Hidden"),
               live ? QStringLiteral("False") : QStringLiteral("True"));
    s.setValue(QStringLiteral("Roundtable/FirstRunAnswer"),
               live ? QStringLiteral("Live") : QStringLiteral("Hidden"));
    s.save();
    qCInfo(lcSpots) << "Roundtable: Erstfrage beantwortet," << (live ? "live" : "verborgen");
    reloadSettings();
}

void RoundtableHub::setHidden(bool hide)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Roundtable/Hidden"),
               hide ? QStringLiteral("True") : QStringLiteral("False"));
    s.save();
    reloadSettings();
}

void RoundtableHub::setEnabled(bool on)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Roundtable/AutoStart"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    s.save();
    reloadSettings();
}

// ----------------------------------------------------------------- key

QString RoundtableHub::keyPath() const
{
    if (!m_keyPath.isEmpty()) {
        return m_keyPath;
    }
    return QFileInfo(AppSettings::instance().filePath()).absolutePath()
           + QStringLiteral("/roundtable-identity.pem");
}

const RoundtableKey* RoundtableHub::key()
{
    if (!m_keyTried) {
        m_keyTried = true;
        QString error;
        m_key = std::make_unique<RoundtableKey>(RoundtableKey::loadOrCreate(keyPath(), &error));
        if (!m_key->isValid()) {
            qCWarning(lcSpots) << "Roundtable: Stationsschluessel nicht geladen:" << error;
        }
    }
    return m_key.get();
}

// ----------------------------------------------------------------- rooms

void RoundtableHub::onRooms(const QJsonObject& full)
{
    m_rooms.clear();
    for (const QJsonValue& v : full.value(QStringLiteral("rooms")).toArray()) {
        const QJsonObject room = v.toObject();
        m_rooms.insert(room.value(QStringLiteral("id")).toString(), room);
    }
    rebuildStations();
    placeSlices();
}

void RoundtableHub::onRoomsDelta(const QJsonObject& delta)
{
    for (const char* list : {"add", "change"}) {
        for (const QJsonValue& v : delta.value(QLatin1String(list)).toArray()) {
            const QJsonObject room = v.toObject();
            m_rooms.insert(room.value(QStringLiteral("id")).toString(), room);
        }
    }
    for (const QJsonValue& v : delta.value(QStringLiteral("remove")).toArray()) {
        m_rooms.remove(v.toString());
    }
    rebuildStations();
    placeSlices();
}

void RoundtableHub::onLinkStateChanged()
{
    using S = RoundtableClient::LinkState;
    const S state = m_client->linkState();
    // Ohne Verbindung keine Marken: was dort stand, ist nicht mehr sicher.
    if (state != S::Live && state != S::Hidden) {
        clearStations();
        placeSlices();
        m_refresh.stop();
    } else if (!m_refresh.isActive()) {
        m_refresh.start();
    }
    if (state != m_linkState) {
        m_linkState = state;
        m_linkSince = RoundtableModel::nowMs();
        emit stateChanged();
    }
    emit statusChanged();
}

void RoundtableHub::rebuildStations()
{
    const QString own = RoundtableWire::baseCallsign(callsign());
    QHash<QString, Station> next;
    for (const QJsonObject& room : std::as_const(m_rooms)) {
        for (const QJsonValue& v : room.value(QStringLiteral("stations")).toArray()) {
            const QJsonObject st = v.toObject();
            Station s;
            s.callsign = st.value(QStringLiteral("callsign")).toString().trimmed().toUpper();
            if (s.callsign.isEmpty()) {
                continue;
            }
            // Die eigene Station steht nicht als Spot da.
            if (!own.isEmpty() && RoundtableWire::baseCallsign(s.callsign) == own) {
                continue;
            }
            s.key = st.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
                    + st.value(QStringLiteral("ref")).toString();
            s.hz = st.value(QStringLiteral("hz")).toInteger();
            s.family = st.value(QStringLiteral("family")).toString();
            s.keyed = st.value(QStringLiteral("keyed")).toBool();
            s.away = st.value(QStringLiteral("away")).toBool();
            s.joining = st.value(QStringLiteral("joining")).toBool();
            if (s.hz <= 0) {
                continue;
            }
            next.insert(s.key, s);
        }
    }
    m_stations = next;
    syncSpots(false);
    emit stationsChanged();
}

void RoundtableHub::clearStations()
{
    m_rooms.clear();
    if (m_stations.isEmpty() && m_spotIndex.isEmpty()) {
        return;
    }
    m_stations.clear();
    syncSpots(false);
    emit stationsChanged();
}

void RoundtableHub::syncSpots(bool /*refreshAll*/)
{
    if (!m_spots) {
        return;
    }
    const auto removeOurs = [this](int idx) {
        // Nur entfernen, was noch unser ist: eine Cluster-Meldung derselben
        // Station auf demselben kHz teilt sich den Index (SpotModel-Dedup).
        const auto it = m_spots->spots().constFind(idx);
        if (it != m_spots->spots().constEnd() && it->source == kSource) {
            m_spots->removeSpot(idx);
        }
    };
    for (auto it = m_spotIndex.begin(); it != m_spotIndex.end();) {
        if (!m_stations.contains(it.key())) {
            removeOurs(it.value());
            it = m_spotIndex.erase(it);
        } else {
            ++it;
        }
    }
    const QString color = setting("Roundtable/SpotColor", m_defaultSpotColor);
    for (const Station& s : std::as_const(m_stations)) {
        const double mhz = double(s.hz) / 1.0e6;
        const int idx = m_spots->dedupIndexFor(s.callsign, mhz);
        const auto old = m_spotIndex.constFind(s.key);
        if (old != m_spotIndex.constEnd() && old.value() != idx) {
            removeOurs(old.value());
        }
        QStringList notes{kSource};
        if (s.keyed) {
            notes << tr("on the air");
        }
        if (s.away) {
            notes << tr("away");
        }
        if (s.joining) {
            notes << tr("joining");
        }
        QMap<QString, QString> kvs;
        kvs[QStringLiteral("callsign")] = s.callsign;
        kvs[QStringLiteral("rx_freq")] = QString::number(mhz, 'f', 6);
        kvs[QStringLiteral("tx_freq")] = QString::number(mhz, 'f', 6);
        kvs[QStringLiteral("mode")] = spotModeOf(s.family);
        kvs[QStringLiteral("source")] = kSource;
        kvs[QStringLiteral("spotter_callsign")] = kSource;
        kvs[QStringLiteral("comment")] = notes.join(QStringLiteral(" · "));
        kvs[QStringLiteral("timestamp")] = QString::number(QDateTime::currentSecsSinceEpoch());
        kvs[QStringLiteral("lifetime_seconds")] = QString::number(kSpotLifetimeSec);
        kvs[QStringLiteral("color")] = color;
        m_spots->applySpotStatus(idx, kvs);
        m_spotIndex.insert(s.key, idx);
    }
}

// ----------------------------------------------------------------- state
//
// Die Core-Haelfte von Upstream RoundtableState (src/core/roundtable/
// RoundtableState.cpp @ e56b3b44c): onMention, recount, replace (hier
// placeSlices) und die Verben. Die Gruende sind dieselben Worte, nur
// "this Core" heisst bei uns "here".

RoundtableHub::LinkState RoundtableHub::linkState() const
{
    return m_linkStateForTest ? *m_linkStateForTest : m_client->linkState();
}

void RoundtableHub::setLinkStateForTest(LinkState state, qint64 sinceMs)
{
    m_linkStateForTest = state;
    m_linkSince = sinceMs;
    emit stateChanged();
}

QString RoundtableHub::mentionCounts() const
{
    return compact(m_mentionCounts);
}

int RoundtableHub::mentionCount(const QString& roomId) const
{
    return m_mentionCounts.value(roomId).toInt();
}

QString RoundtableHub::sliceRooms() const
{
    return compact(m_sliceRooms);
}

QString RoundtableHub::sliceRoomId(int sliceId) const
{
    return m_sliceRooms.value(QString::number(sliceId)).toString();
}

bool RoundtableHub::muted(const QString& callsign) const
{
    const QString base = RoundtableWire::baseCallsign(callsign);
    return !base.isEmpty() && m_muteList.contains(base);
}

bool RoundtableHub::isClientId(const QString& id)
{
    if (id.isEmpty() || id.size() > 64) {
        return false;
    }
    return std::all_of(id.cbegin(), id.cend(), [](QChar c) {
        return c.unicode() >= 0x21 && c.unicode() <= 0x7E;
    });
}

QString RoundtableHub::notConnectedReason()
{
    return QStringLiteral("Roundtable is not connected, so this can't be sent.");
}

QString RoundtableHub::unreadableReason()
{
    return QStringLiteral("Roundtable could not read this request.");
}

void RoundtableHub::onMention(const QJsonObject& mention)
{
    const QJsonObject message = mention.value(QStringLiteral("message")).toObject();
    const qint64 id = message.value(QStringLiteral("id")).toInteger();
    if (id <= 0) {
        return;
    }
    m_mentions.insert(id, Mention{mention.value(QStringLiteral("room")).toString(),
                                  message.value(QStringLiteral("t")).toInteger(),
                                  RoundtableWire::baseCallsign(
                                      message.value(QStringLiteral("callsign")).toString())});
    recount();
}

void RoundtableHub::recount()
{
    // U6: a message naming this station counts until the read mark passes
    // it. A muted station's messages are folded away (11.1), so they do not
    // count either.
    QJsonObject counts;
    for (auto it = m_mentions.begin(); it != m_mentions.end();) {
        if (it->t <= m_mentionsSeenMs) {
            it = m_mentions.erase(it);
            continue;
        }
        if (!m_muteList.contains(it->sender) && !it->room.isEmpty()) {
            counts.insert(it->room, counts.value(it->room).toInt() + 1);
        }
        ++it;
    }
    if (counts != m_mentionCounts) {
        m_mentionCounts = counts;
        emit stateChanged();
    }
}

void RoundtableHub::placeSlices()
{
    // Wire spec 6.3: a slice is in the listed room of its family whose
    // center is nearest, within the family's width, the lower center on a
    // tie; it stays in its room while its family is unchanged and it is
    // still within the width. A slice in no listed room (a candidate, or
    // none) has no room id yet.
    QHash<QString, RoundtableWire::ListedRoom> listed;
    for (auto it = m_rooms.cbegin(); it != m_rooms.cend(); ++it) {
        if (it.key().isEmpty()) {
            continue;
        }
        listed.insert(it.key(), RoundtableWire::ListedRoom{
                                    it.value().value(QStringLiteral("family")).toString(),
                                    it.value().value(QStringLiteral("hz")).toInteger()});
    }
    QJsonObject next;
    for (const RoundtableSliceState& slice : std::as_const(m_sliceStates)) {
        if (RoundtableWire::familyOfMode(slice.mode).isEmpty()) {
            continue;
        }
        const QString key = QString::number(slice.id);
        const QString room =
            RoundtableWire::placeSlice(slice, m_sliceRooms.value(key).toString(), listed);
        next.insert(key, room.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(room));
    }
    if (next != m_sliceRooms) {
        m_sliceRooms = next;
        emit stateChanged();
    }
}

bool RoundtableHub::requireLive(QString* reason) const
{
    if (m_client->linkState() == LinkState::Live) {
        return true;
    }
    if (reason != nullptr) {
        *reason = notConnectedReason();
    }
    return false;
}

void RoundtableHub::writeSetting(const char* key, const QString& value)
{
    auto& s = AppSettings::instance();
    s.setValue(QLatin1String(key), value);
    s.save();
    reloadSettings();
}

bool RoundtableHub::post(const QString& roomId, const QString& text, const QString& clientId,
                         QString* reason)
{
    if (roomId.isEmpty() || text.isEmpty() || !isClientId(clientId)) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    // The answer is the client's postResult, for this clientId. The client
    // sends a post only over the station connection, so nothing goes out
    // before the first-run answer or while hidden.
    m_client->post(roomId, text, clientId);
    return true;
}

bool RoundtableHub::deleteMessage(const QString& messageId, QString* reason)
{
    if (!isWireId(messageId)) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    if (!requireLive(reason)) {
        return false;
    }
    m_client->deleteMessage(messageId);
    return true;
}

bool RoundtableHub::nameTurn(const QString& turnId, const QString& name, QString* reason)
{
    if (!isWireId(turnId) || !RoundtableWire::isCallsign(name.trimmed())) {
        if (reason != nullptr) {
            *reason = isWireId(turnId) ? QStringLiteral("Name a turn with the callsign you heard.")
                                       : unreadableReason();
        }
        return false;
    }
    if (!requireLive(reason)) {
        return false;
    }
    m_client->nameTurn(turnId, name);
    return true;
}

bool RoundtableHub::report(const QString& kind, const QString& target, const QString& reason,
                           QString* refusal)
{
    bool readable = false;
    if (kind == QLatin1String("message") || kind == QLatin1String("turn")) {
        readable = isWireId(target);
    } else if (kind == QLatin1String("station")) {
        const qsizetype slash = target.indexOf(QLatin1Char('/'));
        readable = slash > 0 && slash + 1 < target.size();
    }
    if (!readable) {
        if (refusal != nullptr) {
            *refusal = unreadableReason();
        }
        return false;
    }
    if (!requireLive(refusal)) {
        return false;
    }
    m_client->report(kind, target, reason);
    return true;
}

bool RoundtableHub::postAt(int sliceId, qint64 hz, const QString& family, const QString& text,
                           const QString& clientId, QString* reason)
{
    if (!requireLive(reason)) {
        return false;
    }
    const auto checked = RoundtableWire::decode(
        QString::fromUtf8(RoundtableWire::encode(
            RoundtableWire::postAt(sliceId, hz, family, text, clientId))),
        RoundtableWire::Direction::StationToServer);
    if (!checked.ok || !isClientId(clientId) || m_client->frequencyVersion() < 1) {
        if (reason != nullptr) {
            *reason = QStringLiteral(
                "Frequency chat needs a valid slice and an updated Roundtable server.");
        }
        return false;
    }
    m_client->postAt(sliceId, hz, family, text, clientId);
    return true;
}

bool RoundtableHub::requestHistory(const QJsonArray& ids, const QString& clientId,
                                   QString* reason)
{
    if (!RoundtableWire::decode(
             QString::fromUtf8(RoundtableWire::encode(RoundtableWire::history(ids, clientId))),
             RoundtableWire::Direction::StationToServer)
             .ok) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    if (!m_client->requestHistory(ids, clientId)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable cannot check history right now. Try again shortly.");
        }
        return false;
    }
    return true;
}

bool RoundtableHub::readAt(qint64 hz, const QString& family, QString* reason)
{
    const auto checked = RoundtableWire::decode(
        QString::fromUtf8(RoundtableWire::encode(RoundtableWire::readAt(hz, family))),
        RoundtableWire::Direction::StationToServer);
    if (!checked.ok || m_client->frequencyVersion() < 1) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Frequency chat needs an updated Roundtable server.");
        }
        return false;
    }
    if (!m_client->readAt(hz, family)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Close another frequency view before opening this one.");
        }
        return false;
    }
    return true;
}

void RoundtableHub::unreadAt(qint64 hz, const QString& family)
{
    m_client->unreadAt(hz, family);
}

bool RoundtableHub::readRoom(const QString& roomId, QString* reason)
{
    if (!isListedRoom(roomId)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable has no room there right now.");
        }
        return false;
    }
    m_client->readRoom(roomId);
    return true;
}

void RoundtableHub::unreadRoom(const QString& roomId)
{
    m_client->unreadRoom(roomId);
}

bool RoundtableHub::setOperatorHidden(bool hide, QString* /*reason*/)
{
    setHidden(hide);
    return true;
}

bool RoundtableHub::answerFirstRun(bool live, QString* reason)
{
    if (answered()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("The Roundtable question is already answered here.");
        }
        return false;
    }
    answerFirstRun(live);
    return true;
}

bool RoundtableHub::mute(const QString& callsign, QString* reason)
{
    const QString base = RoundtableWire::baseCallsign(callsign.trimmed());
    if (!RoundtableWire::isCallsign(callsign.trimmed()) || base.isEmpty()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable can only mute a callsign.");
        }
        return false;
    }
    if (!m_muteList.contains(base)) {
        QStringList list = m_muteList;
        list.append(base);
        writeSetting(kMuteListKey, list.join(QLatin1Char(',')));
    }
    return true;
}

bool RoundtableHub::unmute(const QString& callsign, QString* reason)
{
    const QString base = RoundtableWire::baseCallsign(callsign.trimmed());
    if (!RoundtableWire::isCallsign(callsign.trimmed()) || base.isEmpty()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable can only mute a callsign.");
        }
        return false;
    }
    QStringList list = m_muteList;
    if (list.removeAll(base) > 0) {
        writeSetting(kMuteListKey, list.join(QLatin1Char(',')));
    }
    return true;
}

bool RoundtableHub::markMentionRead(qint64 upToMs, QString* reason)
{
    if (upToMs < 0 || upToMs > kMaxUnixMs) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    // The mark only moves forward (U6).
    if (upToMs > m_mentionsSeenMs) {
        writeSetting(kMentionsSeenKey, QString::number(upToMs));
    }
    return true;
}

bool RoundtableHub::requestWebLink(QString* reason)
{
    if (m_client->linkState() == LinkState::Off) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable is off here.");
        }
        return false;
    }
    m_client->requestWebLink();
    return true;
}

bool RoundtableHub::deleteArchive(const QString& confirm, QString* reason)
{
    if (confirm != QLatin1String(RoundtableWire::kDeleteArchiveWords)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Type delete my archive to confirm.");
        }
        return false;
    }
    if (m_client->linkState() == LinkState::Off) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable is off here.");
        }
        return false;
    }
    m_client->deleteArchive();
    return true;
}

void RoundtableHub::attachSpots(SpotModel* spots)
{
    m_spots = spots;
    syncSpots(true);
}

} // namespace Longpath
