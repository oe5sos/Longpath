#pragma once

// =================================================================
// src/core/audio/RecordingScheduler.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. No Thetis equivalent to port from.
//
// Thetis's own scheduler (Memory/MemoryForm.cs SCHEDULER(), design doc
// docs/architecture/phase3m-recording-design.md §3, §8) is a
// per-memory-slot thread tied to Thetis's Memory system — Longpath has
// no equivalent to hang this off of. This is new persistent settings
// and a new timer, built from scratch, not a port. The one behavior
// deliberately carried over from Thetis rather than invented: minute
// resolution (Thetis polls once/minute; there is no reason a
// recording needs to start any more precisely than that, and a finer
// timer would just wake the app more often for no benefit).
//
// ── What this class does and does not own ────────────────────────────
//
// RecordingScheduler owns the SCHEDULE: a persisted list of (start
// time, duration, which recorder) entries, and a timer that checks
// them against the clock. It does NOT own a WavRecorder or IqRecorder
// itself, and does not touch AudioEngine/RadioModel — when an entry
// comes due it emits recordingDue(); whatever DOES own the actual
// recorder (a future coordinator, not written yet) connects to that
// signal and calls start() on the right controller. Same reasoning as
// WavRecorder/IqRecorder themselves ("this class gets samples handed
// to it and doesn't know where from" — see WavRecorder.h): a scheduler
// that doesn't reach into the audio pipeline is testable with a fake
// clock and nothing else running.
//
// ── Persistence shape ──────────────────────────────────────────────
//
// Flat, prefixed keys — same convention AppSettings::saveRadio uses
// for the saved-radio list (AppSettings.cpp:871+), not a JSON blob in
// one key. `Recording/Scheduled/Count` holds the next-id counter;
// each entry lives at `Recording/Scheduled/<id>/*`. IDs are never
// reused within a session (monotonic counter), so a removed-then-
// re-added entry can't collide with a stale reference someone is
// still holding.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

#include <optional>
#include <vector>

namespace Longpath {

enum class ScheduledRecordingKind { Audio, Iq };

struct ScheduledRecording {
    int      id{-1};              // assigned by add(); -1 = not yet added
    QDateTime startAtUtc;
    int      durationMinutes{0};  // 0 = no auto-stop (runs until removed/stopped externally)
    ScheduledRecordingKind kind{ScheduledRecordingKind::Audio};
    QString  path;                // where the recording should be written
    bool     enabled{true};       // disabled entries are kept but never fire
};

class RecordingScheduler : public QObject
{
    Q_OBJECT

public:
    // Matches Thetis's own cadence (design doc §3) — see file header.
    static constexpr int kPollIntervalMs = 60'000;

    explicit RecordingScheduler(QObject* parent = nullptr);

    // Reads every `Recording/Scheduled/*` entry from AppSettings.
    // Clears whatever was in memory first — call once at startup, not
    // repeatedly (repeated calls would re-import already-loaded
    // entries as if they were new, since add() always assigns a fresh
    // id rather than preserving the persisted one... actually it DOES
    // preserve persisted ids on load, see .cpp — but this is still not
    // meant to be called mid-session).
    void loadFromSettings();

    // Writes every current entry back to AppSettings, replacing
    // whatever was there. Called automatically by add()/remove()/
    // setEnabled() — exposed publicly only for tests that want to
    // assert on persisted state without going through a second
    // RecordingScheduler instance.
    void saveToSettings() const;

    // Adds a new entry, assigns it an id, persists immediately.
    // `entry.id` is ignored on input (an id is always assigned) and
    // set on the returned copy... no — returns the assigned id
    // directly, simpler for callers than digging it out of a struct.
    int add(ScheduledRecording entry);

    void remove(int id);
    void setEnabled(int id, bool enabled);

    std::vector<ScheduledRecording> entries() const { return m_entries; }
    std::optional<ScheduledRecording> entry(int id) const;

    // Starts/stops the poll timer. Tests use checkNow() instead, to
    // avoid depending on real wall-clock time.
    void start();
    void stop();
    bool isRunning() const { return m_timer.isActive(); }

    // Checks every entry against `nowUtc` right now, instead of
    // waiting for the timer. Production code never needs this (the
    // timer calls the equivalent internally); tests use it to drive
    // the scheduler against a controlled clock instead of sleeping
    // for real minutes.
    void checkNow(const QDateTime& nowUtc);

signals:
    // An entry's start time has arrived. The receiver is responsible
    // for actually starting a WavRecorder/IqRecorder — this class
    // doesn't own one (see file header).
    void recordingDue(int id, Longpath::ScheduledRecording entry);

    // `durationMinutes` has elapsed since the matching recordingDue().
    // Not emitted for entries with durationMinutes == 0 (no auto-stop
    // — matches Thetis's DurationCount==0-means-unbounded convention,
    // design doc §3).
    void recordingShouldStop(int id);

private:
    void poll();
    static QString entryKeyPrefix(int id);

    std::vector<ScheduledRecording> m_entries;
    // Which ids have already fired recordingDue() this "run" (cleared
    // when an entry is re-enabled or its start time is edited via
    // remove()+add()) — a due entry fires exactly once, not once per
    // poll tick for as long as its start time is in the past.
    std::vector<int> m_fired;
    // id -> the nowUtc at which recordingDue() fired, so checkNow()
    // can tell whether durationMinutes has since elapsed.
    struct FiredAt { int id; QDateTime whenUtc; };
    std::vector<FiredAt> m_firedAt;

    int m_nextId{1};
    QTimer m_timer;
};

} // namespace Longpath

Q_DECLARE_METATYPE(Longpath::ScheduledRecording)
