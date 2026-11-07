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
#include "core/TransverterSetup.h"
#include "core/Ts590Cat.h"
#include "models/SliceModel.h"

#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Longpath {

CatRigBruecke::CatRigBruecke(CompanionRigController* rig, SliceModel* scheibe,
                             QObject* parent)
    : QObject(parent)
    , m_rig(rig)
    , m_scheibe(scheibe)
{
    m_filterZug = new QTimer(this);
    m_filterZug->setSingleShot(true);
    m_filterZug->setInterval(kFilterZugMs);
    connect(m_filterZug, &QTimer::timeout, this, &CatRigBruecke::schickeFilter);

    // Die Antwort auf das Nachlesen kommt noch in der Frist und zaehlt dort
    // nicht. Ohne diesen Abgleich bliebe die Anzeige auf dem verlangten Wert
    // stehen, obwohl das Geraet auf seine Tabelle gerundet hat.
    m_filterAbgleich = new QTimer(this);
    m_filterAbgleich->setSingleShot(true);
    m_filterAbgleich->setInterval(kFristNachBefehlMs + 50);
    connect(m_filterAbgleich, &QTimer::timeout, this, &CatRigBruecke::vomGeraetFilter);

    if (!rig || !scheibe) { return; }

    connect(rig, &CompanionRigController::zfFrequenzGeaendert,
            this, &CatRigBruecke::vomGeraetFrequenz);
    connect(rig, &CompanionRigController::modusGeaendert,
            this, &CatRigBruecke::vomGeraetModus);
    connect(scheibe, &SliceModel::frequencyChanged,
            this, &CatRigBruecke::vonScheibeFrequenz);
    connect(scheibe, &SliceModel::dspModeChanged,
            this, &CatRigBruecke::vonScheibeModus);
    connect(rig, &CompanionRigController::catZustandGeaendert,
            this, &CatRigBruecke::vomGeraetFilter);
    connect(scheibe, &SliceModel::filterChanged,
            this, &CatRigBruecke::vonScheibeFilter);

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
    return m_befehlGesendet && m_seitBefehl.elapsed() < m_fristMs;
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

// Filtergrenzen gibt es am Geraet als Abstand vom Traeger. Die Scheibe rechnet
// vorzeichenbehaftet: in LSB liegt das Filter unter dem Traeger.
void CatRigBruecke::vomGeraetFilter()
{
    if (!m_rig || !m_scheibe || fristLaeuft()) { return; }
    const Ts590Cat::Zustand& z = m_rig->catZustand();

    const DSPMode modus = m_scheibe->dspMode();
    if (modus == DSPMode::CWU || modus == DSPMode::CWL) {
        // In CW kennt das Geraet nur die Breite. Die Lage bleibt der Scheibe.
        if (z.cwBreiteHz <= 0) { return; }
        const int mitte = (m_scheibe->filterLow() + m_scheibe->filterHigh()) / 2;
        const int unten = mitte - z.cwBreiteHz / 2;
        const int oben = unten + z.cwBreiteHz;
        if (m_scheibe->filterLow() == unten && m_scheibe->filterHigh() == oben) { return; }
        m_uebernimmtVomGeraet = true;
        m_scheibe->setFilter(unten, oben);
        m_uebernimmtVomGeraet = false;
        return;
    }
    if (z.lowCutHz < 0 || z.highCutHz < 0) { return; }
    int low = z.lowCutHz;
    int high = z.highCutHz;
    if (modus == DSPMode::LSB) {
        low = -z.highCutHz;
        high = -z.lowCutHz;
    } else if (modus != DSPMode::USB) {
        return;   // nur SSB: die Tabellen der anderen Betriebsarten weichen ab
    }
    if (m_scheibe->filterLow() == low && m_scheibe->filterHigh() == high) { return; }
    m_uebernimmtVomGeraet = true;
    m_scheibe->setFilter(low, high);
    m_uebernimmtVomGeraet = false;
}

void CatRigBruecke::vonScheibeFilter(int low, int high)
{
    if (!m_rig || !m_scheibe || m_uebernimmtVomGeraet) { return; }
    const DSPMode modus = m_scheibe->dspMode();
    if (modus != DSPMode::USB && modus != DSPMode::LSB
        && modus != DSPMode::CWU && modus != DSPMode::CWL) {
        return;
    }
    m_filterBreiteHz = std::abs(high - low);
    const bool unten = modus == DSPMode::LSB || modus == DSPMode::CWL;
    m_filterVonHz = unten ? -high : low;
    m_filterBisHz = unten ? -low : high;
    // Die Filtertasten halten die Mitte fest. Bei einem schmaleren Filter
    // liegt eine Kante dann jenseits des Traegers, das kennt das Geraet
    // nicht. Es zaehlt die Breite: am Geraet gesehen wurde aus 2.9k sonst
    // 2400 Hz.
    if (m_filterVonHz < 0 && m_rig) {
        m_filterVonHz = std::max(0, m_rig->catZustand().lowCutHz);
        m_filterBisHz = m_filterVonHz + m_filterBreiteHz;
    }
    m_befehlGesendet = true;
    m_seitBefehl.start();
    m_filterZug->start();
}

void CatRigBruecke::schickeFilter()
{
    if (!m_rig) { return; }
    const Ts590Cat::Zustand& z = m_rig->catZustand();
    const DSPMode modus = m_scheibe ? m_scheibe->dspMode() : DSPMode::USB;
    if (modus == DSPMode::CWU || modus == DSPMode::CWL) {
        if (Ts590Cat::naechsteCwBreite(m_filterBreiteHz) != z.cwBreiteHz) {
            m_rig->stelle(Ts590Cat::stelleCwBreite(m_filterBreiteHz));
        }
    } else {
        if (Ts590Cat::naechsterLowCut(m_filterVonHz) != z.lowCutHz) {
            m_rig->stelle(Ts590Cat::stelleLowCut(m_filterVonHz));
        }
        if (Ts590Cat::naechsterHighCut(m_filterBisHz) != z.highCutHz) {
            m_rig->stelle(Ts590Cat::stelleHighCut(m_filterBisHz));
        }
    }
    m_befehlGesendet = true;
    m_seitBefehl.start();
    m_filterAbgleich->start();
}

void CatRigBruecke::vonScheibeFrequenz(double hz)
{
    if (!m_rig || m_uebernimmtVomGeraet) { return; }
    const qint64 ganz = std::llround(hz);
    // Bandtaste oder Eingabe auf 2 m: das Geraet kennt nur die ZF, und die
    // Scheibe gehoert auf dieselbe Frequenz wie das Geraet. Am 590 ohne
    // Konverter gefunden: 144,300 MHz kam dort nie an.
    const qint64 zf = TransverterSetup::current().ifFrequencyHz(ganz);
    if (zf != ganz) {
        QTimer::singleShot(0, this, [this, zf]() {
            if (!m_scheibe) { return; }
            m_uebernimmtVomGeraet = true;
            m_scheibe->setFrequency(static_cast<double>(zf));
            m_uebernimmtVomGeraet = false;
        });
    }
    if (zf == m_rig->zfFrequenzHz()) { return; }
    m_rig->stimmeAbZf(zf);
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
