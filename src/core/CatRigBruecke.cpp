// =================================================================
// src/core/CatRigBruecke.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original, siehe CatRigBruecke.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "CatRigBruecke.h"

#include "core/CompanionRigController.h"
#include "models/SliceModel.h"

#include <cmath>

namespace Longpath {

CatRigBruecke::CatRigBruecke(CompanionRigController* rig, SliceModel* scheibe,
                             QObject* parent)
    : QObject(parent)
    , m_rig(rig)
    , m_scheibe(scheibe)
{
    if (!rig || !scheibe) { return; }

    connect(rig, &CompanionRigController::zfFrequenzGeaendert,
            this, &CatRigBruecke::vomGeraetFrequenz);
    connect(rig, &CompanionRigController::modusGeaendert,
            this, &CatRigBruecke::vomGeraetModus);
    connect(scheibe, &SliceModel::frequencyChanged,
            this, &CatRigBruecke::vonScheibeFrequenz);
    connect(scheibe, &SliceModel::dspModeChanged,
            this, &CatRigBruecke::vonScheibeModus);

    // Beim Verbinden gibt das Geraet den Stand vor, nicht die Scheibe.
    if (const qint64 hz = rig->zfFrequenzHz(); hz > 0) {
        vomGeraetFrequenz(hz);
    }
    if (const QString modus = rig->modus(); !modus.isEmpty()) {
        vomGeraetModus(modus);
    }
}

bool CatRigBruecke::modusAusGeraet(const QString& rigModus, DSPMode& modus)
{
    const QString m = rigModus.trimmed().toUpper();
    if (m == QStringLiteral("USB"))    { modus = DSPMode::USB;  return true; }
    if (m == QStringLiteral("LSB"))    { modus = DSPMode::LSB;  return true; }
    if (m == QStringLiteral("CW"))     { modus = DSPMode::CWU;  return true; }
    if (m == QStringLiteral("CWR"))    { modus = DSPMode::CWL;  return true; }
    if (m == QStringLiteral("AM"))     { modus = DSPMode::AM;   return true; }
    if (m == QStringLiteral("FM"))     { modus = DSPMode::FM;   return true; }
    if (m == QStringLiteral("PKTUSB")) { modus = DSPMode::DIGU; return true; }
    if (m == QStringLiteral("PKTLSB")) { modus = DSPMode::DIGL; return true; }
    return false;
}

QString CatRigBruecke::modusFuerGeraet(DSPMode modus)
{
    switch (modus) {
    case DSPMode::USB:  return QStringLiteral("USB");
    case DSPMode::LSB:  return QStringLiteral("LSB");
    case DSPMode::CWU:  return QStringLiteral("CW");
    case DSPMode::CWL:  return QStringLiteral("CWR");
    case DSPMode::AM:   return QStringLiteral("AM");
    case DSPMode::FM:   return QStringLiteral("FM");
    case DSPMode::DIGU: return QStringLiteral("PKTUSB");
    case DSPMode::DIGL: return QStringLiteral("PKTLSB");
    default:            return {};
    }
}

bool CatRigBruecke::fristLaeuft() const
{
    return m_befehlGesendet && m_seitBefehl.elapsed() < kFristNachBefehlMs;
}

void CatRigBruecke::vomGeraetFrequenz(qint64 hz)
{
    if (!m_scheibe || hz <= 0 || fristLaeuft()) { return; }
    if (std::llround(m_scheibe->frequency()) == hz) { return; }
    m_uebernimmtVomGeraet = true;
    m_scheibe->setFrequency(static_cast<double>(hz));
    m_uebernimmtVomGeraet = false;
}

void CatRigBruecke::vomGeraetModus(const QString& modus)
{
    if (!m_scheibe || fristLaeuft()) { return; }
    DSPMode m = DSPMode::USB;
    if (!modusAusGeraet(modus, m) || m_scheibe->dspMode() == m) { return; }
    m_uebernimmtVomGeraet = true;
    m_scheibe->setDspMode(m);
    m_uebernimmtVomGeraet = false;
}

void CatRigBruecke::vonScheibeFrequenz(double hz)
{
    if (!m_rig || m_uebernimmtVomGeraet) { return; }
    const qint64 ganz = std::llround(hz);
    if (ganz == m_rig->zfFrequenzHz()) { return; }
    m_rig->stimmeAbZf(ganz);
    m_befehlGesendet = true;
    m_seitBefehl.start();
}

void CatRigBruecke::vonScheibeModus(DSPMode modus)
{
    if (!m_rig || m_uebernimmtVomGeraet) { return; }
    const QString rigModus = modusFuerGeraet(modus);
    if (rigModus.isEmpty()) { return; }
    m_rig->stelleModus(rigModus);
    m_befehlGesendet = true;
    m_seitBefehl.start();
}

} // namespace Longpath
