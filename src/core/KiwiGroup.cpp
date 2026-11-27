// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Siehe KiwiGroup.h.
//
//   2026-11-25 — Neu angelegt.

#include "core/KiwiGroup.h"

#include <algorithm>

namespace Longpath {

void KiwiGroup::setCapacity(int capacity)
{
    m_capacity = std::clamp(capacity, 0, kKiwiMaxTotal);
}

int KiwiGroup::indexOf(const QString& id) const
{
    for (int i = 0; i < m_members.size(); ++i) {
        if (m_members[i].id == id) { return i; }
    }
    return -1;
}

KiwiGroup::AddResult KiwiGroup::add(const KiwiGroupMember& member)
{
    if (indexOf(member.id) >= 0) { return AddResult::AlreadyThere; }
    if (m_members.size() >= m_capacity) { return AddResult::Full; }
    m_members.append(member);
    return AddResult::Added;
}

bool KiwiGroup::remove(const QString& id)
{
    const int i = indexOf(id);
    if (i < 0) { return false; }
    m_members.removeAt(i);
    return true;
}

bool KiwiGroup::contains(const QString& id) const { return indexOf(id) >= 0; }

void KiwiGroup::clear() { m_members.clear(); }

QStringList KiwiGroup::ids() const
{
    QStringList out;
    for (const KiwiGroupMember& m : m_members) { out << m.id; }
    return out;
}

void KiwiGroup::setMarkHz(double hz)
{
    for (KiwiGroupMember& m : m_members) { m.meter.setMarkHz(hz); }
}

void KiwiGroup::txChanged(bool transmitting, qint64 nowMs)
{
    for (KiwiGroupMember& m : m_members) { m.meter.txChanged(transmitting, nowMs); }
}

bool KiwiGroup::feedRow(const QString& id, const QVector<float>& binsDbm,
                        double lowHz, double highHz, qint64 nowMs)
{
    const int i = indexOf(id);
    if (i < 0) { return false; }
    m_members[i].meter.feedRow(binsDbm, lowHz, highHz, nowMs);
    return true;
}

KiwiHearMeMeter::Result KiwiGroup::result(const QString& id) const
{
    const int i = indexOf(id);
    return i < 0 ? KiwiHearMeMeter::Result() : m_members[i].meter.result();
}

bool KiwiGroup::isWeak(const KiwiHearMeMeter::Result& r)
{
    return r.verdict == KiwiHearMeMeter::Verdict::Hears && r.snrDb < kWeakSnrDb;
}

KiwiGroup::Summary KiwiGroup::summarize(
    const QVector<KiwiHearMeMeter::Result>& results, const QVector<double>& kms)
{
    Summary s;
    s.total = results.size();
    for (int i = 0; i < results.size(); ++i) {
        const KiwiHearMeMeter::Result& r = results[i];
        if (r.verdict == KiwiHearMeMeter::Verdict::Hears) {
            ++s.judged; ++s.hears;
            if (isWeak(r)) { ++s.weak; }
            const double km = i < kms.size() ? kms[i] : -1.0;
            s.farthestKm = std::max(s.farthestKm, km);
        } else if (r.verdict == KiwiHearMeMeter::Verdict::Nothing) {
            ++s.judged; ++s.nothing;
        }
    }
    return s;
}

} // namespace Longpath
