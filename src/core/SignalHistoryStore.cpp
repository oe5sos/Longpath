// =================================================================
// src/core/SignalHistoryStore.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]. Quellstellen, Abweichungen
// und deren Begruendung stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "core/SignalHistoryStore.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Alle Werte unveraendert aus AetherSDR MainWindow.cpp:9574-9589 [@0cd4559].
constexpr qint64 kQrmWindowMs    = 15000; // timestamp retention window
constexpr qint64 kHitWindowMs    = 1000;
constexpr int    kMinHits        = 1;
constexpr qint64 kQualifyMs      = 3000;  // min age before a new signal becomes visible
constexpr int    kQualifyMinHits = 3;     // min detections within kQualifyMs to qualify
constexpr qint64 kVoiceToQrmMs   = 120000;
constexpr qint64 kHideAfterMs    = 30000; // past-signals history window
constexpr qint64 kMaxQrmGapMs    = 1000;

// Sprachbreite: die Grenzen, an denen sich der ganze Einordnungsbaum
// entscheidet (MainWindow.cpp:9628, :9654-9655).
constexpr double kVoiceMinWidthHz = 1800.0;
constexpr double kVoiceMaxWidthHz = 8000.0;

bool isVoiceWidth(double widthHz)
{
    return widthHz >= kVoiceMinWidthHz && widthHz <= kVoiceMaxWidthHz;
}

} // namespace

void SignalHistoryStore::setQrmGateSeconds(int s)
{
    m_qrmGateS = std::clamp(s, 3, 30);
}

void SignalHistoryStore::setLifetimeSeconds(int s)
{
    m_lifetimeS = std::clamp(s, 15, 300);
}

// From AetherSDR MainWindow.cpp:9830-9877 [@0cd4559].
void SignalHistoryStore::ingest(const QVector<DetectedVoiceSignal>& detections,
                                qint64 nowMs)
{
    for (const auto& sig : detections) {
        bool found = false;
        for (auto& e : m_entries) {
            // ≈ up to ~700 Hz shift), small enough to stay below the minimum
            // SSB channel spacing of 2.7 kHz so adjacent stations get their
            // own entries.  Half the signal width for wideband QRM so
            // frame-to-frame centre drift doesn't create duplicates.
            const double mergeHz = (sig.widthHz > kVoiceMaxWidthHz)
                ? sig.widthHz / 2.0 : 2000.0;
            if (std::abs(e.freqMhz - sig.freqMhz) < mergeHz / 1e6) {
                e.lastSeenMs = nowMs;
                e.hitTimestamps.append(nowMs);
                // Track widest detection: a signal can look narrow when weak
                // but wider at peak — the widest seen determines classification.
                e.widthHz = std::max(e.widthHz, sig.widthHz);
                if (sig.peakDbm > e.peakDbm) {
                    e.peakDbm = sig.peakDbm;
                    e.freqMhz = sig.freqMhz;
                }
                // Voice over QRM: if this entry is already QRM-classified
                // and the current detection is voice-width, flag for double
                // marking so both a red QRM and a gold voice marker appear.
                if (e.suspectQrm && isVoiceWidth(sig.widthHz)) {
                    e.voiceOverQrmLastMs = nowMs;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            SignalHistoryEntry newEntry;
            newEntry.freqMhz         = sig.freqMhz;
            newEntry.peakDbm         = sig.peakDbm;
            newEntry.mode            = sig.mode;
            newEntry.firstDetectedMs = nowMs;
            newEntry.lastSeenMs      = nowMs;
            newEntry.widthHz         = sig.widthHz;
            newEntry.hitTimestamps   = {nowMs};
            newEntry.lastGapMs       = nowMs;  // treat appearance as a gap reset
            m_entries.append(std::move(newEntry));
        }
    }
}

// From AetherSDR MainWindow.cpp:9568-9676 [@0cd4559].
void SignalHistoryStore::rebuild(qint64 nowMs, float observedFps,
                                 qint64 suppressUntilMs)
{
    const qint64 narrowQrmGateMs = static_cast<qint64>(m_qrmGateS) * 1000LL;
    // Require 70% frame occupancy over 6 s, derived from observed fps rather than
    // the old hard-coded 105 (which assumed 25 fps and broke at 10 fps or 60 fps).
    const int narrowQrmHitsNeeded = static_cast<int>(
        std::clamp(observedFps * (narrowQrmGateMs / 1000.0f) * 0.70f, 30.0f, 500.0f));

    for (auto& e : m_entries) {
        // Keep 30 s of timestamps for QRM assessment.
        e.hitTimestamps.erase(
            std::remove_if(e.hitTimestamps.begin(), e.hitTimestamps.end(),
                [nowMs](qint64 t) { return (nowMs - t) > kQrmWindowMs; }),
            e.hitTimestamps.end());

        // How many hits in the last 1 second (display gate).
        const int recentHits1s = static_cast<int>(std::count_if(
            e.hitTimestamps.constBegin(), e.hitTimestamps.constEnd(),
            [nowMs](qint64 t) { return (nowMs - t) <= kHitWindowMs; }));
        const bool currentlyActive = (recentHits1s >= kMinHits);

        // Detect any gap > 1 s in the retained timestamp window.
        bool hasVoiceGap = false;
        for (int ti = 1; ti < e.hitTimestamps.size() && !hasVoiceGap; ++ti) {
            if ((e.hitTimestamps[ti] - e.hitTimestamps[ti - 1]) > kMaxQrmGapMs) {
                hasVoiceGap = true;
            }
        }
        if (hasVoiceGap) { e.lastGapMs = nowMs; }

        // QRM classification:
        //   Voice-width (≥1.8 kHz, ≤8 kHz): require 2 unbroken minutes.
        //   True narrow (< 1.8 kHz) / wideband (> 8 kHz): QRM after 6 s of
        //   continuous presence with no gap (checked via lastGapMs so gaps that
        //   age out of the 15 s timestamp window are still honoured).
        //
        // Die CNN-Ueberstimmung des Vorbilds faellt hier weg — Begruendung
        // im Header, Abweichung 2.
        bool qrmQualified;
        if (isVoiceWidth(e.widthHz)) {
            qrmQualified = (nowMs - e.lastGapMs) >= kVoiceToQrmMs;
        } else {
            const int hitsInGate = static_cast<int>(std::count_if(
                e.hitTimestamps.constBegin(), e.hitTimestamps.constEnd(),
                [nowMs, narrowQrmGateMs](qint64 t) { return (nowMs - t) <= narrowQrmGateMs; }));
            // Use lastGapMs so gaps that fell out of the 15 s window still
            // prevent a voice-like signal from being misclassified as QRM.
            const bool noRecentGap = (nowMs - e.lastGapMs) >= narrowQrmGateMs;
            qrmQualified = (nowMs - e.firstDetectedMs) >= narrowQrmGateMs
                           && noRecentGap
                           && (hitsInGate >= narrowQrmHitsNeeded);
        }
        e.suspectQrm = currentlyActive && qrmQualified && !hasVoiceGap;

        // Hide visible markers absent for 30 seconds (the "past signals" history window).
        if (e.visible && !currentlyActive && (nowMs - e.lastSeenMs) > kHideAfterMs) {
            e.visible = false;
        }

        if (!e.visible) {
            // < 1800 Hz: narrow carrier — QRM-only
            // 1800–8000 Hz: voice path (includes the 1800–2300 Hz borderline zone)
            // > 8000 Hz: wideband interference — QRM-only
            const bool isNarrowCarrier   = (e.widthHz < kVoiceMinWidthHz);
            const bool isWidebandCarrier = (e.widthHz > kVoiceMaxWidthHz);
            if (isNarrowCarrier || isWidebandCarrier) {
                if (e.suspectQrm) { e.visible = true; }
            } else if (currentlyActive) {
                // Qualify by total age since first detection, not streak length.
                // Bursty signals (pileups, intermittent operators) qualify as soon
                // as they have been known for kQualifyMs AND have accumulated at
                // least kQualifyMinHits detections in that window.  This rejects
                // transient noise that appears once and disappears while still
                // allowing intermittent but real signals (pileups, bursty digital).
                const qint64 requiredMs = e.confirmedVoice ? 2000LL : kQualifyMs;
                const int hitsInQualify = static_cast<int>(std::count_if(
                    e.hitTimestamps.constBegin(), e.hitTimestamps.constEnd(),
                    [nowMs, requiredMs](qint64 t) { return (nowMs - t) <= requiredMs; }));
                const bool enoughHits = e.confirmedVoice || (hitsInQualify >= kQualifyMinHits);
                if ((nowMs - e.firstDetectedMs) >= requiredMs && enoughHits
                        && nowMs >= suppressUntilMs) {
                    e.visible = true;
                    if (!e.suspectQrm) { e.confirmedVoice = true; }
                }
            }
        }
    }
}

// From AetherSDR MainWindow.cpp:9732-9755 [@0cd4559].
void SignalHistoryStore::expire(qint64 nowMs)
{
    const qint64 lifetimeMs = static_cast<qint64>(m_lifetimeS) * 1000LL;
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [nowMs, lifetimeMs](const SignalHistoryEntry& e) {
                return (nowMs - e.lastSeenMs) > lifetimeMs;
            }),
        m_entries.end());
}

QVector<SignalHistoryEntry> SignalHistoryStore::visibleEntries() const
{
    QVector<SignalHistoryEntry> out;
    for (const auto& e : m_entries) {
        if (e.visible) { out.append(e); }
    }
    return out;
}

} // namespace Longpath
