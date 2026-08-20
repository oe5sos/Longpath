#pragma once

// =================================================================
// src/core/WorkedBefore.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// "Have I had this one before, and does it count for anything?"
//
// The question an operator asks in the two seconds between hearing a
// callsign and deciding whether to call. Answering it after the QSO is
// worthless; answering it while the callsign is being typed is the
// whole point, so this is an index built once from the log rather than
// a search run per keystroke.
//
// NOT src/core/DxccWorkedStatus.h. That one colours the band plan from
// an imported log and thinks in DXCC entities and band/mode groups
// only. This one also answers "how many times, and when last", which
// is what decides whether a contact is worth making — and it is fed by
// the operator's own logbook, which is the thing being added to.
//
// DXCC prefix resolution is injected rather than looked up here, so the
// award logic can be tested without cty.dat and so this file does not
// depend on the spot-colouring stack.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>

namespace Longpath {

struct WorkedSummary {
    // The entity this callsign belongs to, or empty when the prefix
    // could not be resolved. Everything below is meaningless without
    // it except timesWorked and lastWorked, which are per callsign.
    QString entity;

    int       timesWorked{0};   // this exact callsign, any band or mode
    QDateTime lastWorked;       // UTC, invalid when never

    bool knownEntity{false};    // resolution succeeded
    bool newEntity{true};       // entity never worked at all
    bool newBand{true};         // entity never worked on this band
    bool newMode{true};         // entity never worked in this mode group

    // Worth calling for an award, as opposed to worth calling for fun.
    bool isNew() const { return knownEntity && (newEntity || newBand || newMode); }
};

class WorkedBefore {
public:
    // Resolves a callsign to its DXCC primary prefix, or empty.
    using PrefixResolver = std::function<QString(const QString&)>;

    // Rebuild from the whole log. Cheap enough to run on every write:
    // a 20,000-contact log is a few hash inserts per record, once.
    void rebuild(const QVector<LogEntry>& entries,
                 const PrefixResolver& prefixOf);

    // What the operator needs to know about this callsign on this band
    // and mode, right now.
    WorkedSummary lookup(const QString& call, const QString& band,
                         const QString& mode,
                         const PrefixResolver& prefixOf) const;

    // Would logging this be a second copy of something already there?
    // Uses the same rule as the ADIF importer, so a contact the
    // importer would skip is a contact this warns about — one idea of
    // "the same QSO", not two that disagree.
    bool wouldDuplicate(const LogEntry& entry) const;

    int entryCount() const { return m_entries.size(); }

    // DXCC counts modes in groups, not individually: every phone mode
    // is one award slot, every data mode another. Working an entity on
    // FT8 having had it on RTTY is not a new mode, and telling an
    // operator it is would send them chasing a contact that counts for
    // nothing.
    static QString modeGroup(const QString& mode, const QString& submode = {});

private:
    QVector<LogEntry> m_entries;

    // call → how many, and when last
    QHash<QString, int>       m_callCount;
    QHash<QString, QDateTime> m_callLast;

    // entity, entity+band, entity+modegroup
    QSet<QString> m_entities;
    QSet<QString> m_entityBand;
    QSet<QString> m_entityMode;

    // call → indices into m_entries, for the duplicate check
    QHash<QString, QVector<int>> m_byCall;
};

} // namespace Longpath
