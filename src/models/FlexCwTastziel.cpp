// =================================================================
// src/models/FlexCwTastziel.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: FlexCwTastziel.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "models/FlexCwTastziel.h"

#include "core/WdspTypes.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCoreApplication>

namespace Longpath {

QString FlexCwTastziel::tastBefehl(bool unten)
{
    return unten ? QStringLiteral("cw key immediate 1")
                 : QStringLiteral("cw key immediate 0");
}

bool FlexCwTastziel::bereit(QString* warum) const
{
    const auto nein = [warum](const char* text) {
        if (warum != nullptr) {
            *warum = QCoreApplication::translate("FlexCwTastziel", text);
        }
        return false;
    };
    if (m_radio == nullptr || !m_radio->istFlexVerbunden()) {
        return nein(QT_TRANSLATE_NOOP("FlexCwTastziel", "no FlexRadio is connected."));
    }
    // Die Scheibe, die sendet -- nicht die, die gerade bedient wird.
    const SliceModel* scheibe = m_radio->txBoundSlice();
    if (scheibe == nullptr) { scheibe = m_radio->activeSlice(); }
    if (scheibe == nullptr) {
        return nein(QT_TRANSLATE_NOOP("FlexCwTastziel", "there is no transmit slice."));
    }
    const DSPMode art = scheibe->dspMode();
    if (art != DSPMode::CWL && art != DSPMode::CWU) {
        return nein(QT_TRANSLATE_NOOP("FlexCwTastziel", "the transmit slice is not in CW."));
    }
    if (m_radio->transmitModel().isTune()) {
        return nein(QT_TRANSLATE_NOOP("FlexCwTastziel", "the radio is tuning."));
    }
    return true;
}

bool FlexCwTastziel::sendet() const
{
    return m_radio != nullptr
           && darfTasten(m_radio->istFlexVerbunden(), m_radio->mox(), m_radio->flexGetastet());
}

bool FlexCwTastziel::freigabeSteht() const
{
    return m_radio != nullptr && m_radio->mox();
}

void FlexCwTastziel::fordereSenden(bool an)
{
    if (m_radio == nullptr) { return; }
    m_radio->setMox(an);
}

void FlexCwTastziel::taste(bool unten)
{
    if (m_radio == nullptr || !m_radio->istFlexVerbunden()) { return; }
    // Hinunter nur, waehrend die Maschine sendet. Hinauf immer: ein
    // `cw key 0` zu viel schadet nie, eines zu wenig laesst den Traeger
    // stehen.
    if (unten && !sendet()) { return; }
    m_radio->sendeFlexBefehl(tastBefehl(unten));
}

} // namespace Longpath
