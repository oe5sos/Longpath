// no-port-check: Longpath-original, see RundumLauf.h.
//
// src/core/RundumLauf.cpp  (Longpath)

#include "core/RundumLauf.h"

#include "core/RotorController.h"

#include <cmath>

namespace Longpath {

namespace {
double norm360(double a)
{
    double r = std::fmod(a, 360.0);
    return r < 0.0 ? r + 360.0 : r;
}
double abstand(double a, double b)
{
    const double d = std::abs(norm360(a) - norm360(b));
    return std::min(d, 360.0 - d);
}
} // namespace

// ── RundumKarte ─────────────────────────────────────────────────────

void RundumKarte::leeren()
{
    m_s = QVector<Sektor>(kSektoren);
}

void RundumKarte::nimm(double azimutGrad, float bodenDbm, float belegung, int signale)
{
    const int i = static_cast<int>(norm360(azimutGrad + kSektorGrad / 2.0) / kSektorGrad) % kSektoren;
    Sektor& s = m_s[i];
    ++s.proben;
    s.bodenSumme += bodenDbm;
    s.belegungMax = std::max(s.belegungMax, belegung);
    s.signaleMax  = std::max(s.signaleMax, signale);
}

int RundumKarte::abgedeckt() const
{
    int n = 0;
    for (const Sektor& s : m_s) { if (s.proben > 0) { ++n; } }
    return n;
}

int RundumKarte::meisteBelegung() const
{
    int best = -1;
    for (int i = 0; i < kSektoren; ++i) {
        if (m_s[i].proben == 0) { continue; }
        if (best < 0 || m_s[i].belegungMax > m_s[best].belegungMax) { best = i; }
    }
    return best < 0 ? -1 : best * kSektorGrad;
}

int RundumKarte::lautesterBoden(float* ueberRuhigstemDb) const
{
    int laut = -1;
    int ruhig = -1;
    for (int i = 0; i < kSektoren; ++i) {
        if (m_s[i].proben == 0) { continue; }
        if (laut < 0 || m_s[i].boden() > m_s[laut].boden()) { laut = i; }
        if (ruhig < 0 || m_s[i].boden() < m_s[ruhig].boden()) { ruhig = i; }
    }
    if (laut < 0) { return -1; }
    if (ueberRuhigstemDb) { *ueberRuhigstemDb = m_s[laut].boden() - m_s[ruhig].boden(); }
    return laut * kSektorGrad;
}

// ── RundumLauf ──────────────────────────────────────────────────────

RundumLauf::RundumLauf(QObject* parent)
    : QObject(parent)
{
    m_wache.setSingleShot(true);
    m_wache.setInterval(kStillstandMs);
    connect(&m_wache, &QTimer::timeout, this, &RundumLauf::stillstand);
}

void RundumLauf::setRotor(RotorController* r)
{
    if (m_rotor) { disconnect(m_rotor, nullptr, this, nullptr); }
    m_rotor = r;
    if (m_rotor) {
        connect(m_rotor, &RotorController::positionChanged, this, &RundumLauf::beiPosition);
    }
}

bool RundumLauf::start()
{
    if (!m_rotor || !m_rotor->isConnected()) {
        emit fehler(tr("No rotator connected."));
        return false;
    }
    if (!m_rotor->hasFreshPosition()) {
        emit fehler(tr("The rotator has not reported its position yet."));
        return false;
    }
    m_karte.leeren();
    m_startAz = norm360(m_rotor->azimuth());
    m_az = m_startAz;
    m_azFrisch = true;
    m_schritt = 0;
    m_phase = Phase::Lauf;
    naechsterSchritt();
    return true;
}

void RundumLauf::abbrechen()
{
    if (m_phase == Phase::Ruhe) { return; }
    m_wache.stop();
    if (m_rotor) {
        m_rotor->stop();
        m_rotor->moveTo(m_startAz);
    }
    m_phase = Phase::Ruhe;
    emit fertig(false);
}

void RundumLauf::naechsterSchritt()
{
    ++m_schritt;
    const int schritte = 360 / kSchrittGrad;
    if (m_schritt > schritte) {
        m_wache.stop();
        m_phase = Phase::Ruhe;
        emit fertig(true);
        return;
    }
    m_ziel = norm360(m_startAz + m_schritt * kSchrittGrad);
    emit fortschritt(m_schritt, schritte, m_az);
    if (m_rotor) { m_rotor->moveTo(m_ziel); }
    m_wache.start();
}

void RundumLauf::beiPosition(double az)
{
    m_az = norm360(az);
    m_azFrisch = true;
    if (m_phase != Phase::Lauf) { return; }
    m_wache.start();                         // er bewegt sich noch
    if (abstand(m_az, m_ziel) <= kAngekommenGrad) {
        naechsterSchritt();
    }
}

void RundumLauf::nimmBandLage(float bodenDbm, float belegung, int signale)
{
    if (m_phase != Phase::Lauf || !m_azFrisch || !m_rotor || !m_rotor->hasFreshPosition()) {
        return;
    }
    m_karte.nimm(m_az, bodenDbm, belegung, signale);
}

void RundumLauf::stillstand()
{
    if (m_phase == Phase::Ruhe) { return; }
    emit fehler(tr("The rotator stopped reporting for %1 s — sweep aborted.")
                    .arg(kStillstandMs / 1000));
    abbrechen();
}

} // namespace Longpath
