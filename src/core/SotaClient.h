// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SOTA (Summits On The Air) HTTPS spot poller
//
// NereusSDR-original. Neither Thetis nor AetherSDR has a SOTA spot
// client -- Thetis has no logbook/spot system at all, and AetherSDR's
// DxSpot::reference/entity/grid fields were extended for POTA with SOTA
// explicitly noted as "scoped but not yet connected" (see DxSpot.h,
// 2026-08-26 entry). This file is that connection, built on the same
// PotaClient shape (HTTPS poll -> DxSpot -> spotReceived) rather than a
// new pattern.
//
// Endpoints and the two rules below come from the SOTA API maintainer's
// own posted guidance (VK3ARR, reflector.sota.org.uk/c/api-information,
// forwarded by the operator 2026-09-03), not guesswork:
//   - a User-Agent naming the app and the operator's callsign
//   - no more than one request every 60 seconds
//   - fetch /api/spots/epoch (a bare GUID string) first; only fetch the
//     full spot list when it differs from the last-seen value, since
//     most polls find nothing has changed
// PotaClient has no enforced floor on its poll interval (only a UI
// spinbox range); this class enforces SOTA's 60 s explicitly, because
// nothing else in the codebase does it for me.
//
// Modification history (NereusSDR):
//   2026-09-03  AI (Anthropic Claude Code)  Original, operator-directed
//               follow-up to the SOTA/POTA logbook work earlier the same
//               session.

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QFile>
#include <QSet>
#include <QVector>
#include <atomic>

#include "DxSpot.h"

namespace Longpath {

// SOTA (Summits on the Air) spot client - polls
// https://api2.sota.org.uk/api/spots/60/all/all for active activations,
// gated by an epoch check against /api/spots/epoch so an unchanged spot
// list costs one small request instead of a full re-fetch. Emits
// spotReceived() for each new spot, exactly like PotaClient.
class SotaClient : public QObject {
    Q_OBJECT

public:
    explicit SotaClient(QObject* parent = nullptr);
    ~SotaClient() override;

    // Clamped up to kMinIntervalSec if given less -- see the class
    // comment for why that floor is enforced here and not left to the
    // caller, unlike PotaClient's startPolling().
    void startPolling(int intervalSec = 60);
    void stopPolling();
    bool isPolling() const { return m_polling; }

    QString logFilePath() const;

    // Public test seam, same contract as PotaClient::parseJsonForTest():
    // runs the live parser + dedup logic against `data` without a
    // QNetworkAccessManager or an HTTPS round-trip, and mutates
    // m_seenSpotIds the same way a real poll would.
    QVector<DxSpot> parseJsonForTest(const QByteArray& data) {
        return parseAndCollect(data);
    }

    static constexpr int kMinIntervalSec = 60;

signals:
    void started();
    void stopped();
    void pollError(const QString& error);
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);
    void pollComplete(int spotCount, int newCount);

private slots:
    void onPollTimer();

private:
    // Shared parse + dedup core, called from the live epoch-then-spots
    // poll path and from parseJsonForTest(). Pure data transformation:
    // does not touch the log file, the epoch, or emit signals.
    QVector<DxSpot> parseAndCollect(const QByteArray& data);

    // The actual spot list fetch, called once the epoch check says the
    // data has changed (or on the very first poll, when there is
    // nothing yet to compare against).
    void fetchSpots();

    QNetworkAccessManager* m_nam;
    QTimer*     m_pollTimer;
    QFile       m_logFile;
    QSet<int>   m_seenSpotIds;   // track id to only emit new spots
    QString     m_lastEpoch;     // last-seen value from /api/spots/epoch
    std::atomic<bool> m_polling{false};

    static constexpr const char* SpotsUrl =
        "https://api2.sota.org.uk/api/spots/60/all/all";
    static constexpr const char* EpochUrl =
        "https://api2.sota.org.uk/api/spots/epoch";
};

} // namespace Longpath
