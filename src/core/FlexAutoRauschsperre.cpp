// no-port-check: Longpath-original, see FlexAutoRauschsperre.h.
//
// src/core/FlexAutoRauschsperre.cpp  (Longpath)

#include "core/FlexAutoRauschsperre.h"

#include "core/AppSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace Longpath {

namespace {
const QString kMargeSchluessel = QStringLiteral("FlexAutoSqlMarginDb");
} // namespace

int FlexAutoRauschsperre::stufeFuerDbm(double dbm)
{
    // From AetherSDR SpectrumWidget.cpp:4644-4646 [@d58e2b8a]:
    //   std::clamp(static_cast<int>(targetDbm - kSqlMinDbm + 0.5f), 1, 100)
    const long s = std::lround(dbm - kStufeNullDbm);
    return static_cast<int>(std::clamp(s, 1L, 100L));
}

double FlexAutoRauschsperre::dbmFuerStufe(int stufe)
{
    return kStufeNullDbm + static_cast<double>(stufe);
}

int FlexAutoRauschsperre::ladeMarge()
{
    bool ok = false;
    const int db = AppSettings::instance()
                       .value(kMargeSchluessel, QString::number(kMargeVorgabeDb))
                       .toInt(&ok);
    return ok ? std::clamp(db, kMargeMinDb, kMargeMaxDb) : kMargeVorgabeDb;
}

void FlexAutoRauschsperre::speichereMarge(int db)
{
    AppSettings::instance().setValue(
        kMargeSchluessel,
        QString::number(std::clamp(db, kMargeMinDb, kMargeMaxDb)));
}

void FlexAutoRauschsperre::setMarge(int db)
{
    db = std::clamp(db, kMargeMinDb, kMargeMaxDb);
    if (db == m_marge) { return; }
    m_marge  = db;
    m_sofort = true;
}

void FlexAutoRauschsperre::messung(double pegelDbm, qint64 jetztMs)
{
    if (!std::isfinite(pegelDbm)) { return; }
    m_proben.emplace_back(jetztMs, pegelDbm);
    while (!m_proben.empty() && jetztMs - m_proben.front().first > kFensterMs) {
        m_proben.pop_front();
    }
    if (static_cast<int>(m_proben.size()) < kMindestProben) { return; }

    std::vector<double> werte;
    werte.reserve(m_proben.size());
    for (const auto& p : m_proben) { werte.push_back(p.second); }
    const auto idx = static_cast<std::size_t>(
        std::floor(kAnteil * static_cast<double>(werte.size() - 1)));
    std::nth_element(werte.begin(), werte.begin() + static_cast<std::ptrdiff_t>(idx),
                     werte.end());
    m_rausch = werte[idx];
}

std::optional<int> FlexAutoRauschsperre::stufe() const
{
    if (!m_rausch) { return std::nullopt; }
    return stufeFuerDbm(*m_rausch + static_cast<double>(m_marge));
}

std::optional<int> FlexAutoRauschsperre::faellig(qint64 jetztMs)
{
    const auto s = stufe();
    if (!s) { return std::nullopt; }
    const qint64 seit = jetztMs - m_gesendetUm;
    bool senden = false;
    if (m_sofort || m_gesendet < 0) {
        senden = true;
    } else if (std::abs(*s - m_gesendet) >= kTotbandStufen
               && seit >= kMindestAbstandMs) {
        senden = true;
    } else if (*s != m_gesendet && seit >= kNachziehenMs) {
        senden = true;
    }
    if (!senden) { return std::nullopt; }
    m_gesendet   = *s;
    m_gesendetUm = jetztMs;
    m_sofort     = false;
    return *s;
}

void FlexAutoRauschsperre::zuruecksetzen()
{
    m_proben.clear();
    m_rausch.reset();
    m_sofort = true;
}

} // namespace Longpath
