// src/gui/ComboStyle.h
#pragma once
#include <QComboBox>
#include "StyleConstants.h"

namespace Longpath {

inline void applyComboStyle(QComboBox* combo)
{
    combo->setFixedHeight(Style::kButtonH);
    // Glas & Tiefe (2026-09-18): dieselbe Bauform wie jedes Auswahlfeld
    // im Haus — erhaben wie ein Knopf, Liste als dunkles Glas mit dem
    // gedeckten Auswahlverlauf. Bis dahin war das Feld hier eine Mulde
    // und im Setup eine blaue Flaeche; jetzt gibt es eine Definition
    // (Style::kComboStyle), und die steht auch in der App-Basislinie.
    combo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
}

} // namespace Longpath
