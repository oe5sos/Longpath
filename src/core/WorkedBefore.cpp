// =================================================================
// src/core/WorkedBefore.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see WorkedBefore.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "WorkedBefore.h"

#include "core/AdifLog.h"
#include "core/CallsignInfo.h"

namespace NereusSDR {

namespace {

QString norm(const QString& s) { return s.trimmed().toUpper(); }

QString bandKey(const QString& entity, const QString& band)
{
    return entity + QLatin1Char('|') + band.trimmed().toLower();
}

QString modeKey(const QString& entity, const QString& group)
{
    return entity + QLatin1Char('|') + group;
}

} // namespace

QString WorkedBefore::modeGroup(const QString& mode, const QString& submode)
{
    const QString m = norm(mode);
    if (m.isEmpty()) { return QStringLiteral("UNKNOWN"); }

    if (m == QLatin1String("CW")) { return QStringLiteral("CW"); }

    // Phone. SSB carries LSB/USB as a submode, and AM/FM are their own
    // modes; all of them are one award slot.
    if (m == QLatin1String("SSB") || m == QLatin1String("AM")
        || m == QLatin1String("FM") || m == QLatin1String("LSB")
        || m == QLatin1String("USB") || m == QLatin1String("DIGITALVOICE")) {
        return QStringLiteral("PHONE");
    }
    // A submode can be the only place the voice-ness is recorded.
    const QString s = norm(submode);
    if (s == QLatin1String("LSB") || s == QLatin1String("USB")) {
        return QStringLiteral("PHONE");
    }

    // Everything else — RTTY, PSK31, FT8, FT4, JT65, MFSK — is data.
    // Grouping them is the DXCC rule, not a simplification: an entity
    // worked on RTTY is not new again on FT8.
    return QStringLiteral("DATA");
}

void WorkedBefore::rebuild(const QVector<LogEntry>& entries,
                           const PrefixResolver& prefixOf)
{
    m_entries = entries;
    m_callCount.clear();
    m_callLast.clear();
    m_entities.clear();
    m_entityBand.clear();
    m_entityMode.clear();
    m_byCall.clear();

    for (int i = 0; i < m_entries.size(); ++i) {
        const LogEntry& e = m_entries.at(i);
        const QString call = Callsigns::normalized(e.call);
        if (call.isEmpty()) { continue; }

        m_byCall[call].append(i);
        m_callCount[call] += 1;

        const QDateTime t = e.timeOn.toUTC();
        if (t.isValid()) {
            const QDateTime prev = m_callLast.value(call);
            if (!prev.isValid() || t > prev) { m_callLast[call] = t; }
        }

        const QString entity = prefixOf ? norm(prefixOf(call)) : QString{};
        if (entity.isEmpty()) { continue; }

        m_entities.insert(entity);
        if (!e.band.trimmed().isEmpty()) {
            m_entityBand.insert(bandKey(entity, e.band));
        }
        const QString group = modeGroup(e.mode, e.submode);
        if (group != QLatin1String("UNKNOWN")) {
            m_entityMode.insert(modeKey(entity, group));
        }
    }
}

WorkedSummary WorkedBefore::lookup(const QString& call, const QString& band,
                                   const QString& mode,
                                   const PrefixResolver& prefixOf) const
{
    WorkedSummary s;
    const QString c = Callsigns::normalized(call);
    if (c.isEmpty()) { return s; }

    s.timesWorked = m_callCount.value(c, 0);
    s.lastWorked  = m_callLast.value(c);

    const QString entity = prefixOf ? norm(prefixOf(c)) : QString{};
    if (entity.isEmpty()) {
        // No entity: the per-callsign counts above are still true and
        // still useful. The award flags stay at their defaults and
        // knownEntity says not to trust them.
        return s;
    }

    s.entity      = entity;
    s.knownEntity = true;
    s.newEntity   = !m_entities.contains(entity);

    // A band or mode we do not know cannot be called new — claiming a
    // new band because the radio has not reported one yet would send
    // the operator after a contact that counts for nothing.
    const QString b = band.trimmed();
    s.newBand = b.isEmpty() ? false : !m_entityBand.contains(bandKey(entity, b));

    const QString group = modeGroup(mode);
    s.newMode = group == QLatin1String("UNKNOWN")
                    ? false
                    : !m_entityMode.contains(modeKey(entity, group));

    return s;
}

bool WorkedBefore::wouldDuplicate(const LogEntry& entry) const
{
    const QString c = Callsigns::normalized(entry.call);
    if (c.isEmpty()) { return false; }

    for (int idx : m_byCall.value(c)) {
        if (AdifLog::isSameQso(m_entries.at(idx), entry)) { return true; }
    }
    return false;
}

} // namespace NereusSDR
