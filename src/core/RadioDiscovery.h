#pragma once

// =================================================================
// src/core/RadioDiscovery.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   HPSDR/clsRadioDiscovery.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

/*  clsRadioDiscovery.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "HpsdrModel.h"

#include <QObject>
#include <QDeadlineTimer>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QMetaType>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <chrono>
#include <functional>

namespace Longpath {

// Protocol version supported by the radio.
//
// SunSdr is not an OpenHPSDR protocol at all — it is SunSDR2's own
// native UDP wire protocol (design doc
// docs/architecture/2026-08-24-sunsdr-native-driver-design.md). It sits
// in this enum because RadioConnection::create() and BoardCapabilities
// both key off it as "which wire format", and there is no broadcast
// discovery for this protocol (the radio's IP is supplied out-of-band,
// same design doc) — see AddCustomRadioDialog's manual-entry path,
// plan docs/architecture/2026-08-26-sunsdr-connection-plan.md §Phase A.
// Flex ist, wie SunSdr, keine OpenHPSDR-Protokollfassung: es ist
// FlexRadios eigenes SmartSDR-Protokoll -- ein Erkennungsruf im
// Sekundentakt auf UDP 4992 und ein Textbefehlskanal auf TCP 4992
// (Satzarten C/R/S/M). Steht hier aus demselben Grund wie SunSdr:
// RadioConnection::create() und BoardCapabilities fragen beide „welches
// Drahtformat" an diesem Feld ab.
//
// Anders als bei SunSdr gibt es hier eine Erkennung, sie laeuft aber
// umgekehrt: die Maschine ruft von selbst, Longpath hoert nur zu
// (FlexRadioDiscovery). Es wird nichts gefragt.
enum class ProtocolVersion : int {
    Protocol1 = 1,
    Protocol2 = 2,
    SunSdr    = 3,
    Flex      = 4
};

// Information about a discovered OpenHPSDR radio.
struct RadioInfo {
    // Identity
    QString name;                        // User-friendly name, e.g. "ANAN-G2"
    QString macAddress;                  // MAC as "AA:BB:CC:DD:EE:FF" (primary key)
    QHostAddress address;                // IP address on local network
    quint16 port{1024};                  // 1024 for OpenHPSDR (P1/P2); SunSdr uses 50001 (control)

    // Hardware
    HPSDRHW boardType{HPSDRHW::Unknown};
    int firmwareVersion{0};
    int adcCount{1};                     // Derived from boardType (1 or 2)
    int maxReceivers{4};                 // Board-dependent max simultaneous RX

    // Protocol
    ProtocolVersion protocol{ProtocolVersion::Protocol1};
    bool inUse{false};                   // Another client already connected

    // Capabilities (derived from boardType at parse time)
    bool hasDiversityReceiver{false};    // 2-ADC boards support diversity
    bool hasPureSignal{false};           // Boards supporting PA linearization
    int maxSampleRate{384000};           // Max supported sample rate in Hz

    // Model override — set by user in ConnectionPanel, persisted per-MAC.
    // When set to a value other than FIRST, overrides the auto-detected model.
    HPSDRModel modelOverride{HPSDRModel::FIRST}; // FIRST = no override (auto-detect)

    // Display helpers
    QString displayName() const;
    static int adcCountForBoard(HPSDRHW type);
    static int maxReceiversForBoard(HPSDRHW type);

    // Comparison — radios are identified by MAC address
    bool operator==(const RadioInfo& other) const {
        return macAddress == other.macAddress;
    }
};

// -------------------------------------------------------------------------
// Discovery timing profiles
// -------------------------------------------------------------------------
// Source: mi0bot/Thetis@Hermes-Lite clsRadioDiscovery.cs (timing guide comment, lines 49-70)
//
// AttemptsPerNic = how many send/wait cycles per NIC
// QuietPollsBeforeResend = how many consecutive empty polls before retry/give up
// PollTimeoutMs = how long each poll waits (milliseconds)
//
// Effective max wait per NIC ≈ AttemptsPerNic × QuietPollsBeforeResend × PollTimeoutMs
//
// Profile        AttemptsPerNic   QuietPollsBeforeResend   PollTimeoutMs   Approx time per NIC
// -----------    ---------------  ----------------------   -------------   -------------------
// UltraFast      1                2                        40              ~80 ms
// VeryFast       1                3                        60              ~180 ms
// Fast           2                3                        80              ~480 ms
// Balanced       2                4                        100             ~800 ms
// SafeDefault    3                5                        150             ~1800 ms
// VeryTolerant   3                6                        300             ~5400 ms
// -------------------------------------------------------------------------

enum class DiscoveryProfile {
    UltraFast,
    VeryFast,
    Fast,
    Balanced,
    SafeDefault,
    VeryTolerant
};

struct DiscoveryTiming {
    int attemptsPerNic;
    int quietPollsBeforeResend;
    int pollTimeoutMs;
};

// From mi0bot/Thetis@Hermes-Lite clsRadioDiscovery.cs applyScanPerformanceProfile()
constexpr DiscoveryTiming timingFor(DiscoveryProfile p) noexcept {
    switch (p) {
        case DiscoveryProfile::UltraFast:    return {1, 2,  40};
        case DiscoveryProfile::VeryFast:     return {1, 3,  60};
        case DiscoveryProfile::Fast:         return {2, 3,  80};
        case DiscoveryProfile::Balanced:     return {2, 4, 100};
        case DiscoveryProfile::SafeDefault:  return {3, 5, 150};
        case DiscoveryProfile::VeryTolerant: return {3, 6, 300};
    }
    return {3, 5, 150};  // SafeDefault fallback
}

// Discovers OpenHPSDR radios on the local network.
// Both P1 and P2 radios respond to UDP broadcasts on port 1024.
// Uses the mi0bot/Thetis NIC-walk + tunable timing-profile pattern:
// bind per NIC, send P1+P2 probes, poll up to attemptsPerNic×quietPollsBeforeResend×pollTimeoutMs.
class RadioDiscovery : public QObject {
    Q_OBJECT

public:
    // Outcome of one quietPollAttempt() call (private, near scanAllNics()
    // below, for the actual per-attempt quiet-poll loop and full rationale).
    // Declared public and up here — rather than beside quietPollAttempt()
    // itself — only because quietPollAttemptForTest()'s signature, further
    // down in this same public section, needs the name in scope already;
    // enum access can't differ between one declaration and another.
    enum class QuietPollOutcome { Quiet, DeadlineExceeded, Cancelled };

    explicit RadioDiscovery(QObject* parent = nullptr);
    ~RadioDiscovery() override;

    void startDiscovery();
    void stopDiscovery();

    // Post-disconnect quiet period (2026-07-27, ANAN-G2E lockup).
    //
    // Wire captures show Longpath fires a broadcast discovery burst 7-15 ms
    // after sending run=0 (disconnect reopens the ConnectionPanel, whose ctor
    // auto-scans) and both observed G2E lockups happened inside that window,
    // while Thetis goes completely silent after its stop frame and never
    // wedges the same radio.  Probing a radio while its stop-transition state
    // machines are still settling is a race Thetis never enters.
    //
    // Calling holdOffScans() defers — never drops — any startDiscovery() or
    // probeAddress() issued before the deadline: the work runs when the quiet
    // period expires, so the panel still populates and Connect flows still
    // probe, just after the radio has finished stopping.
    void holdOffScans(std::chrono::milliseconds quiet);
    qint64 holdOffRemainingMs() const;

    DiscoveryProfile profile() const { return m_profile; }
    void setProfile(DiscoveryProfile profile) { m_profile = profile; }

    QList<RadioInfo> discoveredRadios() const;

    // Mark a MAC as currently-connected so onStaleCheck will not emit
    // radioLost for it. Once a radio is streaming (P1 ep6 / P2 DDC),
    // it stops replying to discovery broadcasts, so the stale timer
    // would otherwise fire after the discovered-only timeout.
    void setConnectedMac(const QString& mac) { m_connectedMac = mac; }
    void clearConnectedMac() { m_connectedMac.clear(); }

    // Saved-MAC tracking — design §7.4.
    // Radios in this set are never removed by onStaleCheck; they stay in the
    // panel (pill transitions Stale → Offline) but the row is permanent.
    // Call setSavedMacs from ConnectionPanel after seeding from AppSettings.
    void setSavedMacs(const QStringList& macs) { m_savedMacs = QSet<QString>(macs.begin(), macs.end()); }
    void addSavedMac(const QString& mac) { m_savedMacs.insert(mac); }
    void removeSavedMac(const QString& mac) { m_savedMacs.remove(mac); }

#ifdef LONGPATH_BUILD_TESTS
    // Test-only hooks — only compiled when LONGPATH_BUILD_TESTS is defined.
    // Allow unit tests to inject a stale lastSeen entry and trigger the sweep
    // without needing a real UDP scan. Not part of the public API.
    void injectLastSeenForTest(const QString& mac, const RadioInfo& info, qint64 lastSeenMs) {
        m_radios.insert(mac, info);
        m_lastSeen.insert(mac, lastSeenMs);
    }
    void forceStaleCheckForTest() { onStaleCheck(); }

    // The holdOffScans deadline is process-wide (see s_scanHoldOff) AND, as
    // of 2026-09-05, mirrored into a cross-process shared-memory segment
    // (see the anonymous-namespace helpers in RadioDiscovery.cpp) — either
    // one surviving across test functions would defer probes in unrelated
    // cases. Call from a QTest init() for a clean slate. Defined in the .cpp
    // file because clearing the shared segment needs those TU-local helpers.
    static void clearHoldOffForTest();

    // Test-only forwarding to the private quiet-poll loop scanAllNics() uses
    // per attempt — see quietPollAttempt()'s declaration for behaviour. Lets
    // a test drive that exact loop against a fully-controlled loopback
    // socket: scanAllNics() itself always skips loopback interfaces (see its
    // isNicCandidate check), so the loop is otherwise unreachable without a
    // real non-loopback NIC and a live radio on it.
    // What the loop DID, as counts instead of milliseconds — see
    // quietPollAttempt()'s `quietPollsUsed` for why. `datagrams` is counted
    // here in the forwarding callback, so a test can tell "the reply never
    // arrived" (loopback loss, a thread that did not get scheduled) apart
    // from "the reply arrived and the counter stayed frozen" — without it,
    // a lost datagram would make the regression test pass for the wrong
    // reason.
    struct QuietPollMetrics {
        int quietPolls = 0;
        int datagrams = 0;
    };

    QuietPollOutcome quietPollAttemptForTest(QUdpSocket& sock, int quietBeforeStop, int pollMs,
                                              const QDeadlineTimer& deadline,
                                              QuietPollMetrics* metrics = nullptr) {
        return quietPollAttempt(
            sock, quietBeforeStop, pollMs, deadline,
            [metrics](const QByteArray&, const QHostAddress&, quint16) {
                if (metrics != nullptr) {
                    metrics->datagrams++;
                }
            },
            metrics != nullptr ? &metrics->quietPolls : nullptr);
    }
#endif

    // Public static parsers — exposed for unit-testing in Task 5.
    // Both return true on a valid discovery reply and populate 'out'.
    // From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P1/P2 branches.
    static bool parseP1Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out);
    static bool parseP2Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out);

    // Send a unicast P1+P2 probe in parallel to the given address.
    // On reply: emits radioDiscovered() with parsed RadioInfo.
    // On timeout: emits probeFailed() with the address that was probed.
    //
    // Used by the Add Radio dialog and saved-row Connect path to validate
    // reachability before opening a full connection. Times out fast (1.5 s
    // default) so users get a clear error instead of a UDP-ghost long wait.
    //
    // From Phase 3Q design §7.2.
    void probeAddress(const QHostAddress& addr,
                      quint16 port = 1024,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(1500));

signals:
    void discoveryStarted();
    void discoveryFinished();
    void radioDiscovered(const Longpath::RadioInfo& info);
    void radioUpdated(const Longpath::RadioInfo& info);
    void radioLost(const QString& macAddress);
    void probeFailed(const QHostAddress& addr, quint16 port);

private slots:
    void onStaleCheck();

private:
    static constexpr quint16 kDiscoveryPort    = 1024;
    static constexpr int kContinuousIntervalMs = 5000;   // re-run NIC walk every 5s during active monitoring
    // Design §7.4: discovered-only radios age out at 60 s (raised from 15 s).
    // Connected MAC and saved MACs are always exempt regardless of this value.
    static constexpr int kDiscoveredOnlyTimeoutMs = 60 * 1000;

    // MAC extraction from raw bytes
    static QString macToString(const char* bytes);

    // Per-NIC synchronous scan (mi0bot pattern).
    // Sends P1+P2 probes, polls for replies using the timing profile,
    // calls parseP1Reply / parseP2Reply and de-duplicates by MAC.
    void scanAllNics();

    // One attempt's quiet-poll loop, extracted out of scanAllNics() for
    // testability (2026-09-09 — CI-only 120s GUI-test-timeout investigation,
    // docs/architecture/2026-09-09-ci-discovery-hang-investigation.md).
    //
    // Polls `sock` up to `pollMs` at a time. A poll that comes back
    // readable leaves the quiet counter untouched — matching Thetis
    // clsRadioDiscovery.cs:964-976 [@852bf0e], where only the not-readable
    // branch advances it — and every pending datagram is drained and
    // handed to `onDatagram` in arrival order; a poll that comes back
    // not-readable advances the quiet counter by one.
    //
    // Until 2026-09-09 a readable poll reset the counter to 0 instead
    // (Longpath-original drift from the 2026-04-12 port, not a deliberate
    // divergence — see docs/architecture/2026-09-09-ci-discovery-hang-
    // investigation.md for how it was found and confirmed unintentional).
    // That made a single bursty reply push the natural give-up point out by
    // a full extra quietBeforeStop polls; freezing the counter instead
    // matches upstream exactly and bounds that to one poll no matter when
    // in the loop the reply arrives.
    //
    // Returns Quiet once quietBeforeStop consecutive not-readable polls are
    // observed — the class header's documented
    // "attemptsPerNic x quietPollsBeforeResend x pollTimeoutMs" bound.
    // Returns DeadlineExceeded if that never happens before `deadline`
    // expires: nothing before this existed to stop a sustained "readable,
    // but nothing usable comes out of it" condition (a stray reply from an
    // unrelated host on a shared subnet, a spurious wakeup, or a duplicate
    // NIC entry each doing their own confusingly-fast-looking scan — see the
    // investigation doc for what the CI log actually showed) from holding
    // the quiet counter at 0 forever, which is a genuinely unbounded hang,
    // not merely a slow path. Returns Cancelled if stopDiscovery() fires
    // mid-poll (checked once per poll, same cadence scanAllNics() always
    // had).
    //
    // `quietPollsUsed` (optional, 2026-10-17) reports how many NOT-readable
    // polls the loop consumed IN TOTAL — which is not the same as the
    // internal counter's final value: that one always ends at
    // quietBeforeStop, in the fixed behaviour and in the regression alike,
    // so it tells the two apart not at all. The total does: the
    // reset-on-readable regression spends the polls it had already made
    // before the reply a second time. It exists because the test that
    // guards the frozen counter above had to measure wall-clock time to
    // see it, and that is a bet on the runner's scheduling: the fixed
    // behaviour needs quietBeforeStop polls (6 x 30 ms = 180 ms in that
    // test), the reset-on-readable regression needs three more (270 ms),
    // and on 2026-10-17 a loaded macOS runner took 257 ms for the FIXED
    // path — inside the regression's band, with the test failing although
    // the code was right. The count is the quantity that actually carries
    // the invariant, and it is the same number on a fast machine and a
    // slow one.
    QuietPollOutcome quietPollAttempt(
        QUdpSocket& sock, int quietBeforeStop, int pollMs, const QDeadlineTimer& deadline,
        const std::function<void(const QByteArray& datagram, const QHostAddress& sender,
                                  quint16 senderPort)>& onDatagram,
        int* quietPollsUsed = nullptr);

    // ── Der Suchlauf als Zustandsautomat (2026-10-16) ────────────────
    //
    // Bis heute lief scanAllNics() als eine Schleife durch, die je
    // Netzkarte attemptsPerNic × quietPollsBeforeResend × pollTimeoutMs
    // in waitForReadyRead() stand — auf SafeDefault 2250 ms je Karte,
    // und zwar im Hauptfaden, ohne Faden dahinter. Der Konstruktor
    // dieser Klasse kuendigte den Umbau an ("Async rewrite is a
    // follow-up"); dies ist er.
    //
    // Was dabei GLEICH bleibt: die Karten werden nacheinander
    // abgeklopft, je Karte attemptsPerNic Versuche, je Versuch bis zu
    // quietPollsBeforeResend stille Fenster von pollTimeoutMs, und der
    // Stille-Zaehler friert bei einer Antwort statt sich
    // zurueckzusetzen (Thetis clsRadioDiscovery.cs:964-976). Nur die
    // Wartezeit kommt jetzt von einem Zeitgeber statt von poll().
    //
    // Was NEU ist: zwischen zwei Fenstern laeuft die Ereignisschleife.
    // Das macht auch die Oberflaeche ehrlich — ConnectionPanel setzt
    // bei discoveryStarted "Searching for radios..." und schaltet den
    // Knopf ab, was bisher niemand sehen konnte, weil beide Signale im
    // selben Takt kamen.
    struct NicCandidate {
        QString      name;
        QHostAddress ipv4;
        QHostAddress broadcast;
    };

    // Eine Karte beginnen (oder den Lauf beenden, wenn keine mehr da
    // ist), einen Versuch beginnen, ein Poll-Fenster abschliessen.
    void beginNextNic();
    void beginAttempt();
    void onScanPoll();
    void onScanReadyRead();
    void finishScan();

    // Der Rumpf der alten onDatagram-Lambda, unveraendert: Laenge
    // pruefen, P1/P2 unterscheiden, MAC-Dedup, m_radios pflegen,
    // radioDiscovered/radioUpdated melden.
    void handleDatagram(const QByteArray& data, const QHostAddress& sender);

    QList<NicCandidate> m_scanNics;
    int  m_scanNicIndex{-1};
    int  m_scanAttempt{0};
    int  m_scanQuietPolls{0};
    bool m_scanSawDatagram{false};   // in DIESEM Poll-Fenster
    bool m_scanRunning{false};
    QUdpSocket* m_scanSock{nullptr}; // Kind dieses Objekts, lebt je Karte
    QTimer m_scanPollTimer;
    QDeadlineTimer m_scanAttemptDeadline;
    QSet<QString> m_scanSeen;        // MAC-Dedup fuer DIESEN Lauf
    qint64 m_scanNowMs{0};
    QByteArray m_scanP1;
    QByteArray m_scanP2;
    DiscoveryTiming m_scanTiming{timingFor(DiscoveryProfile::SafeDefault)};

    DiscoveryProfile m_profile{DiscoveryProfile::SafeDefault};

    QTimer m_staleTimer;

    // holdOffScans deadline.  Default-constructed QDeadlineTimer is already
    // expired, which is the "no holdoff armed" state.
    //
    // PROCESS-WIDE, not per-instance (Codex review, PR #306).  The constraint
    // belongs to the radio and the wire, not to any one RadioDiscovery object,
    // and RadioModel::discovery() is not the only instance: e.g.
    // AddCustomRadioDialog.cpp:589 constructs its own.  A per-object deadline
    // would let that dialog probe a just-disconnected radio inside the quiet
    // window and re-enter the post-stop race this exists to prevent.
    //
    // MONOTONIC, not wall-clock (Codex review, PR #306).  This was
    // QDateTime::currentMSecsSinceEpoch() arithmetic, which an NTP step could
    // move underneath us: a forward correction expires the holdoff early and
    // lets a probe land inside the G2E stop-transition window this exists to
    // avoid, and a backward correction defers discovery for as long as the
    // correction was large.  QDeadlineTimer measures against a monotonic
    // source, so clock synchronisation cannot shorten a radio-safety interval
    // or wedge discovery.
    static QDeadlineTimer s_scanHoldOff;

    // Per-instance: stops a burst of startDiscovery() calls on THIS object
    // from queueing multiple delayed scans.
    bool m_deferredScanPending{false};
    QMap<QString, RadioInfo> m_radios;   // keyed by MAC address
    QMap<QString, qint64> m_lastSeen;    // MAC -> timestamp
    QString m_connectedMac;              // MAC currently in use by a RadioConnection; exempt from stale-removal
    QSet<QString>  m_savedMacs;          // Design §7.4: MACs that are saved in AppSettings; exempt from stale-removal

    // Abbruch fuer den laufenden Suchlauf. Seit dem Umbau auf den
    // Zustandsautomaten (2026-10-16) blockiert der Lauf nichts mehr;
    // das Flag beendet ihn aber weiterhin beim naechsten Poll-Fenster,
    // und finishScan() macht den Socket zu. Vorher war es die einzige
    // Bremse: die blockierende Variante hielt das Beenden bis zu ~4,5 s
    // auf einer Maschine mit zwei Karten fest (SafeDefault). Atomar,
    // weil stopDiscovery() aus jedem Fadenzusammenhang kommen darf (in
    // der Praxis der Hauptfaden, aber billig).
    std::atomic<bool> m_stopRequested{false};
};

} // namespace Longpath

Q_DECLARE_METATYPE(Longpath::RadioInfo)
Q_DECLARE_METATYPE(Longpath::ProtocolVersion)
Q_DECLARE_METATYPE(Longpath::DiscoveryProfile)
