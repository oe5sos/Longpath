// =================================================================
// src/core/audio/RecordingScheduler.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/RecordingScheduler.h"

#include "core/AppSettings.h"

#include <algorithm>

namespace Longpath {

namespace {

QString kindToString(ScheduledRecordingKind k)
{
    return k == ScheduledRecordingKind::Iq ? QStringLiteral("Iq")
                                           : QStringLiteral("Audio");
}

ScheduledRecordingKind kindFromString(const QString& s)
{
    return s == QStringLiteral("Iq") ? ScheduledRecordingKind::Iq
                                     : ScheduledRecordingKind::Audio;
}

} // namespace

RecordingScheduler::RecordingScheduler(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(kPollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &RecordingScheduler::poll);
}

QString RecordingScheduler::entryKeyPrefix(int id)
{
    return QStringLiteral("Recording/Scheduled/%1/").arg(id);
}

void RecordingScheduler::loadFromSettings()
{
    m_entries.clear();
    m_fired.clear();
    m_firedAt.clear();

    auto& s = AppSettings::instance();

    // Existence of `.../StartAtUtc` is the marker a slot is occupied —
    // same idea as AppSettings::savedRadio() using the `name` key
    // (AppSettings.cpp:954).
    int maxId = 0;
    for (const QString& key : s.allKeys()) {
        if (!key.startsWith(QStringLiteral("Recording/Scheduled/"))) {
            continue;
        }
        if (!key.endsWith(QStringLiteral("/StartAtUtc"))) {
            continue;
        }
        // Key shape: Recording/Scheduled/<id>/StartAtUtc
        const QStringList parts = key.split(QLatin1Char('/'));
        if (parts.size() != 4) { continue; }
        bool ok = false;
        const int id = parts.at(2).toInt(&ok);
        if (!ok || id <= 0) { continue; }

        const QString prefix = entryKeyPrefix(id);
        ScheduledRecording entry;
        entry.id = id;
        entry.startAtUtc = QDateTime::fromString(
            s.value(prefix + QStringLiteral("StartAtUtc")).toString(),
            Qt::ISODate);
        entry.durationMinutes =
            s.value(prefix + QStringLiteral("DurationMinutes"),
                    QStringLiteral("0")).toInt();
        entry.kind = kindFromString(
            s.value(prefix + QStringLiteral("Kind")).toString());
        entry.path = s.value(prefix + QStringLiteral("Path")).toString();
        entry.enabled =
            s.value(prefix + QStringLiteral("Enabled"),
                    QStringLiteral("True")).toString()
            == QStringLiteral("True");

        m_entries.push_back(entry);
        maxId = std::max(maxId, id);
    }

    std::sort(m_entries.begin(), m_entries.end(),
             [](const ScheduledRecording& a, const ScheduledRecording& b) {
        return a.id < b.id;
    });

    m_nextId = maxId + 1;
}

void RecordingScheduler::saveToSettings() const
{
    auto& s = AppSettings::instance();

    // Clear every previously-persisted entry first — a removed entry
    // must not survive a save/load round trip as a ghost. Scanning
    // allKeys() for our own prefix rather than trusting m_entries to
    // know what used to be there: m_entries only knows what's live
    // NOW, not what a previous save() left behind.
    for (const QString& key : s.allKeys()) {
        if (key.startsWith(QStringLiteral("Recording/Scheduled/"))) {
            s.remove(key);
        }
    }

    for (const ScheduledRecording& entry : m_entries) {
        const QString prefix = entryKeyPrefix(entry.id);
        s.setValue(prefix + QStringLiteral("StartAtUtc"),
                  entry.startAtUtc.toString(Qt::ISODate));
        s.setValue(prefix + QStringLiteral("DurationMinutes"),
                  QString::number(entry.durationMinutes));
        s.setValue(prefix + QStringLiteral("Kind"), kindToString(entry.kind));
        s.setValue(prefix + QStringLiteral("Path"), entry.path);
        s.setValue(prefix + QStringLiteral("Enabled"),
                  entry.enabled ? QStringLiteral("True")
                                : QStringLiteral("False"));
    }
}

int RecordingScheduler::add(ScheduledRecording entry)
{
    entry.id = m_nextId++;
    m_entries.push_back(entry);
    saveToSettings();
    return entry.id;
}

void RecordingScheduler::remove(int id)
{
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
                       [id](const ScheduledRecording& e) { return e.id == id; }),
        m_entries.end());
    m_fired.erase(std::remove(m_fired.begin(), m_fired.end(), id),
                 m_fired.end());
    m_firedAt.erase(
        std::remove_if(m_firedAt.begin(), m_firedAt.end(),
                       [id](const FiredAt& f) { return f.id == id; }),
        m_firedAt.end());
    saveToSettings();
}

void RecordingScheduler::setEnabled(int id, bool enabled)
{
    for (ScheduledRecording& e : m_entries) {
        if (e.id == id) {
            e.enabled = enabled;
            // Re-enabling lets a past-due entry fire again on the next
            // check — otherwise flipping an entry off and back on
            // would leave it permanently silent for the rest of the
            // session, which is a stranger surprise than firing it
            // once more.
            if (enabled) {
                m_fired.erase(std::remove(m_fired.begin(), m_fired.end(), id),
                             m_fired.end());
            }
            saveToSettings();
            return;
        }
    }
}

std::optional<ScheduledRecording> RecordingScheduler::entry(int id) const
{
    for (const ScheduledRecording& e : m_entries) {
        if (e.id == id) { return e; }
    }
    return std::nullopt;
}

void RecordingScheduler::start() { m_timer.start(); }
void RecordingScheduler::stop()  { m_timer.stop(); }

void RecordingScheduler::poll()
{
    checkNow(QDateTime::currentDateTimeUtc());
}

void RecordingScheduler::checkNow(const QDateTime& nowUtc)
{
    for (const ScheduledRecording& e : m_entries) {
        if (!e.enabled) { continue; }

        const bool alreadyFired =
            std::find(m_fired.begin(), m_fired.end(), e.id) != m_fired.end();

        if (!alreadyFired) {
            if (e.startAtUtc.isValid() && nowUtc >= e.startAtUtc) {
                m_fired.push_back(e.id);
                m_firedAt.push_back({e.id, nowUtc});
                emit recordingDue(e.id, e);
            }
            continue;
        }

        if (e.durationMinutes <= 0) { continue; }  // no auto-stop

        for (const FiredAt& f : m_firedAt) {
            if (f.id != e.id) { continue; }
            if (f.whenUtc.secsTo(nowUtc) >= e.durationMinutes * 60) {
                emit recordingShouldStop(e.id);
                // Remove the firedAt record so a later re-enable
                // doesn't immediately re-fire the stop signal for a
                // stale timestamp.
                m_firedAt.erase(
                    std::remove_if(m_firedAt.begin(), m_firedAt.end(),
                                   [id = e.id](const FiredAt& x) { return x.id == id; }),
                    m_firedAt.end());
            }
            break;
        }
    }
}

} // namespace Longpath
