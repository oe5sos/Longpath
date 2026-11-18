// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/models/RoundtableHub.h  (Longpath)
// =================================================================
//
// Roundtable in Longpath: die Bruecke zwischen dem uebernommenen
// RoundtableClient (src/core/roundtable) und dem, was Longpath hat.
//
//   hinein:  die Scheiben (Frequenz, Betriebsart, Filter, welche gerade
//            sendet), Rufzeichen und Locator, die Antworten aus
//            Roundtable/* in AppSettings, der Stationsschluessel;
//   heraus:  die anderen Stationen aus der Raumliste als Spots der Quelle
//            "Roundtable" im SpotModel — so stehen sie am Panadapter, in
//            der Spot-Liste und hinter dem Quellenschalter wie jede andere
//            Spot-Quelle, und ein Klick stimmt ab.
//
// Upstream NereusSDR macht dasselbe verteilt: RadioModel::wireRoundtable /
// pushRoundtableSlices / roundtableModeOf (src/models/RadioModel.cpp
// @ 9036ffc04), SpotSourceHost::startRoundtableWith (Autostart, Pruefung
// des Rufzeichens) und RoundtableState (die Roundtable/*-Schluessel). Die
// Zuordnung der Betriebsarten und die Reihenfolge der Scheiben (aktive
// zuerst) sind von dort; der Rest ist Longpath-eigen, weil es bei uns
// weder Core noch SpotSourceHost noch einen Schreibbeobachter in
// AppSettings gibt.
//
// Longpath-Regeln dazu:
//   - Vor der Antwort auf die Erstfrage geht nichts hinaus, auch keine
//     Verbindung (RoundtableClient::wanted).
//   - Scheiben eines fremden Empfaengers (KiwiSDR, SunSDR ueber TCI:
//     suppressAutoStreamBinding) werden nicht gemeldet — sie sind nicht
//     diese Station.
//   - Die eigene Station steht nicht als Spot da.
//
// Einstellungen (AppSettings, Werte wie bei Upstream):
//   Roundtable/AutoStart       "True" (Vorgabe) | "False"
//   Roundtable/FirstRunAnswer  "" | "Live" | "Hidden"
//   Roundtable/Hidden          "True" | "False"
//   Roundtable/Callsign        leer = StationCallsign
//   Roundtable/GridSquare      leer = Stations-Locator
//   Roundtable/AwayChoice      "Report" (Vorgabe) | "Stop"
//   Roundtable/ServerUrl       leer = wss://roundtable.nereussdr.com/v1/station
//   Roundtable/SpotColor       Farbe der Spots (Vorgabe: setDefaultSpotColor)
//   Roundtable/MuteList        Basisrufzeichen, kommagetrennt (Stummschalten)
//   Roundtable/MentionsSeenMs  bis wohin Erwaehnungen gelesen sind (U6)
//   Roundtable/HighlightMentions, DistanceUnits, ShowBearing — liest das
//                              Panel ueber RoundtableModel::stationSetting
//
// Stufe 2 (2026-11-18): der Hub ist auch das, was Upstream RoundtableState
// auf dem Core ist (src/core/roundtable/RoundtableState.cpp @ e56b3b44c):
// Verbindungszustand seit wann, Erwaehnungszaehler je Raum, welcher Raum
// je Scheibe (placeSlice), Stummliste und die Verben mit ihren Pruefungen
// (post, deleteMessage, nameTurn, report, postAt, readAt, readRoom, mute,
// markMentionRead, ...). Dazu besitzt er das RoundtableModel, an das die
// Oberflaeche bindet, und fuettert den Client mit Messwerten (pushMeter,
// "wie hoeren mich andere"): alle kMeterPollMs der gemittelte S-Meter-
// Pegel je gemeldeter Scheibe (RadioModel::rxSignalDbm). Upstream nimmt
// dafuer SliceModel::meterReadingNsChanged aus seiner SliceMeterPump; die
// gibt es bei uns nicht.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
//   2026-11-18 - Stufe 2: Zustand und Verben aus Upstream RoundtableState
//                (Core-Haelfte), Messwerte, RoundtableModel. Martin
//                Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/WdspTypes.h"
#include "core/roundtable/RoundtableWire.h"

#include "core/roundtable/RoundtableClient.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <memory>

namespace Longpath {

class RadioModel;
class RoundtableKey;
class RoundtableModel;
class SliceModel;
class SpotModel;

class RoundtableHub : public QObject {
    Q_OBJECT

public:
    /// Ein anderer auf Roundtable, wie die Raumliste ihn meldet
    /// (Frequenz-Erweiterung 1, `stations[]`).
    struct Station {
        QString key;        // sourceRoom/ref — wechselt mit jedem Raum
        QString callsign;
        qint64 hz = 0;
        QString family;
        bool keyed = false;
        bool away = false;
        bool joining = false;
    };

    using LinkState = RoundtableClient::LinkState;

    static constexpr int kSpotLifetimeSec = 90;
    static constexpr int kSpotRefreshMs = 30000;
    /// Wie oft der S-Meter je Scheibe gelesen wird (der Takt des
    /// MeterPollers). Der Client schickt davon hoechstens einen je Sekunde
    /// (6.5); der Rest formt den Rauschboden (RoundtableSignalFloor).
    static constexpr int kMeterPollMs = 100;

    /// Die Schluessel, die nur ueber die Verben geaendert werden.
    static constexpr const char* kMuteListKey = "Roundtable/MuteList";
    static constexpr const char* kMentionsSeenKey = "Roundtable/MentionsSeenMs";

    explicit RoundtableHub(QObject* parent = nullptr);
    ~RoundtableHub() override;

    /// Nicht besessen. Die Scheiben und der Sendezustand kommen von dort.
    void attachRadio(RadioModel* radio);
    /// Nicht besessen. Dorthin gehen die Spots.
    void attachSpots(SpotModel* spots);
    /// Die Farbe der Spots, wenn Roundtable/SpotColor leer ist. Kommt aus
    /// der Oberflaeche (Style::kSpotRoundtableDefault) — ein Modell kennt
    /// keine Stilkonstanten.
    void setDefaultSpotColor(const QString& color) { m_defaultSpotColor = color; }

    RoundtableClient* client() const { return m_client.get(); }
    /// Woran die Oberflaeche bindet (Panel, Leiste). Gehoert dem Hub.
    RoundtableModel* model() const { return m_model.get(); }

    /// Liest Roundtable/* und gibt es dem Client; startet oder haelt an.
    void reloadSettings();

    bool enabled() const;
    bool answered() const;
    bool hidden() const;
    QString callsign() const;
    QString grid() const;
    QString statusText() const;

    /// Die Erstfrage beantworten: live (Rufzeichen, Locator, Frequenz
    /// werden oeffentlich) oder verborgen (nur lesen).
    void answerFirstRun(bool live);
    void setHidden(bool hidden);
    void setEnabled(bool on);

    QList<Station> stations() const { return m_stations.values(); }

    // ---- Zustand (Upstream RoundtableState, Core-Haelfte).
    LinkState linkState() const;
    /// Wann sich der Verbindungszustand zuletzt aenderte (Unix ms, die Uhr
    /// von RoundtableModel::nowMs); 0 vor der ersten Aenderung.
    qint64 linkSince() const { return m_linkSince; }
    bool firstRunAnswered() const { return answered(); }
    /// Raum-Id -> Zahl der Nachrichten, die diese Station nennen und nach
    /// der Lesemarke kamen (U6), als kompaktes JSON.
    QString mentionCounts() const;
    int mentionCount(const QString& roomId) const;
    /// Scheibe -> Raum-Id oder null, als kompaktes JSON.
    QString sliceRooms() const;
    /// Der Raum, in dem die Scheibe ist, oder leer.
    QString sliceRoomId(int sliceId) const;
    QStringList muteList() const { return m_muteList; }
    bool muted(const QString& callsign) const;
    bool isListedRoom(const QString& roomId) const { return m_rooms.contains(roomId); }

    /// Wire spec 5.2: clientId, 1 bis 64 Bytes druckbares ASCII.
    static bool isClientId(const QString& id);
    static QString notConnectedReason();
    static QString unreadableReason();

    // ---- Die Verben: true, oder false mit Worten in *reason.
    bool post(const QString& roomId, const QString& text, const QString& clientId,
              QString* reason);
    bool deleteMessage(const QString& messageId, QString* reason);
    bool nameTurn(const QString& turnId, const QString& name, QString* reason);
    bool report(const QString& kind, const QString& target, const QString& reason,
                QString* refusal);
    bool postAt(int sliceId, qint64 hz, const QString& family, const QString& text,
                const QString& clientId, QString* reason);
    bool readAt(qint64 hz, const QString& family, QString* reason);
    bool requestHistory(const QJsonArray& ids, const QString& clientId, QString* reason);
    void unreadAt(qint64 hz, const QString& family);
    bool readRoom(const QString& roomId, QString* reason);
    void unreadRoom(const QString& roomId);
    bool setOperatorHidden(bool hidden, QString* reason);
    /// Wie answerFirstRun(bool), aber verweigert, wenn schon geantwortet.
    bool answerFirstRun(bool live, QString* reason);
    bool mute(const QString& callsign, QString* reason);
    bool unmute(const QString& callsign, QString* reason);
    bool markMentionRead(qint64 upToMs, QString* reason);
    bool requestWebLink(QString* reason);
    bool deleteArchive(const QString& confirm, QString* reason);

    /// Pruefstand: woher der S-Meter je Scheibe kommt (sonst
    /// RadioModel::rxSignalDbm), und ein Lesedurchgang von Hand.
    void setMeterSourceForTest(std::function<double(int)> source);
    void pollMetersForTest() { pollMeters(); }
    /// Pruefstand ohne Funkgeraet: die gemeldeten Scheiben direkt setzen.
    void setSlicesForTest(const QList<RoundtableSliceState>& slices);

    /// Aus den Scheiben die Liste, die `presence` meldet: aktive zuerst,
    /// fremde Empfaenger weg. Rein, fuer den Pruefstand.
    static QList<RoundtableSliceState> sliceStates(const QList<SliceModel*>& slices,
                                                   SliceModel* active, SliceModel* keyed);
    /// Die Betriebsart, wie Roundtable sie kennt; leer fuer keine Familie.
    static QString modeOf(DSPMode mode);

    /// Pruefstand: wo der Schluessel liegt (sonst neben den Einstellungen).
    void setKeyPathForTest(const QString& path) { m_keyPath = path; }
    /// Pruefstand: die Raumliste direkt einspeisen.
    void feedRoomsForTest(const QJsonObject& full) { onRooms(full); }
    void feedRoomsDeltaForTest(const QJsonObject& delta) { onRoomsDelta(delta); }

signals:
    void stationsChanged();
    void statusChanged();
    /// linkState/linkSince, firstRunAnswered, hidden, mentionCounts oder
    /// sliceRooms koennen sich geaendert haben.
    void stateChanged();
    /// Roundtable/* wurde neu gelesen: jede Einstellung kann anders sein.
    void settingsReloaded();
    /// Ein S-Meter-Wert einer gemeldeten Scheibe (dBm), so wie er an den
    /// Client ging — das Panel zeigt ihn als eigene Zeile "You".
    void sliceMeterRead(int sliceId, double dbm);

private:
    void watchSlice(SliceModel* slice);
    void pushSlices();
    void onRooms(const QJsonObject& full);
    void onRoomsDelta(const QJsonObject& delta);
    void onLinkStateChanged();
    void rebuildStations();
    void syncSpots(bool refreshAll);
    void clearStations();
    const RoundtableKey* key();
    QString keyPath() const;
    void onMention(const QJsonObject& mention);
    void recount();
    void placeSlices();
    void pollMeters();
    void updateMeterPolling();
    bool requireLive(QString* reason) const;
    void writeSetting(const char* key, const QString& value);

    QPointer<RadioModel> m_radio;
    QPointer<SpotModel> m_spots;
    std::unique_ptr<RoundtableClient> m_client;
    std::unique_ptr<RoundtableModel> m_model;
    std::unique_ptr<RoundtableKey> m_key;
    QString m_keyPath;
    bool m_keyTried = false;
    bool m_keyed = false;
    QString m_defaultSpotColor;

    QHash<QString, QJsonObject> m_rooms;   // id -> summary
    QHash<QString, Station> m_stations;    // key -> station
    QHash<QString, int> m_spotIndex;       // key -> SpotModel index
    QTimer m_refresh;

    // Stufe 2: Zustand wie Upstream RoundtableState.
    LinkState m_linkState = LinkState::Off;
    qint64 m_linkSince = 0;
    QList<RoundtableSliceState> m_sliceStates;
    QJsonObject m_sliceRooms;
    struct Mention {
        QString room;
        qint64 t = 0;
        QString sender;
    };
    QHash<qint64, Mention> m_mentions;
    qint64 m_mentionsSeenMs = 0;
    QJsonObject m_mentionCounts;
    QStringList m_muteList;

    // Messwerte.
    QTimer m_meterPoll;
    std::function<double(int)> m_meterSource;
};

} // namespace Longpath
