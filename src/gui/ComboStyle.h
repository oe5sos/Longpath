// src/gui/ComboStyle.h
#pragma once
#include <QComboBox>
#include "StyleConstants.h"

namespace Longpath {

inline void applyComboStyle(QComboBox* combo)
{
    combo->setFixedHeight(Style::kButtonH);
    combo->setStyleSheet(QStringLiteral(
        // Das geschlossene Feld ist eine Mulde, keine Flaeche -- siehe
        // die Notiz bei kLineEditStyle. kButtonBg und kBorder liegen
        // vier Helligkeitsstufen auseinander; gegen kInsetBg steht der
        // Rand deutlich, und das Feld ist als Feld erkennbar.
        //
        // Die aufgeklappte Liste bleibt auf Knopfgrund: die ist eine
        // Auswahl und liegt UEBER der Oberflaeche, nicht darin.
        "QComboBox {"
        "  background: %5; color: %2;"
        "  border: 1px solid %3; border-radius: 6px;"
        "  padding: 2px 6px; font-size: 11px;"
        "}"
        "QComboBox::drop-down { border: none; width: 18px; }"
        // Reuse the spin-down arrow asset (shared with QSpinBox styling)
        // so combos pick up the same visual language as the rest of the
        // app's arrow controls.
        "QComboBox::down-arrow {"
        "  image: url(:/icons/spin-down.svg); width: 10px; height: 10px;"
        "}"
        // 2026-09-08: ohne selection-color blieb der gerade ausgewaehlte
        // Eintrag im aufgeklappten Dropdown praktisch unsichtbar --
        // Qt liess die Vordergrundfarbe dieser Zeile dem Systemstandard,
        // der gegen den erzwungenen Grund nicht trug. Betreiber-Meldung
        // "immer das ausgewaehlte ist unsichtbar" -- diese Funktion ist
        // die am breitesten geteilte der drei betroffenen Stellen
        // (siehe StyleConstants.h::kComboStyle/applyDarkPageStyle und
        // SpectrumOverlayPanel.cpp::OverlayColors::kPanelStyle).
        "QComboBox QAbstractItemView {"
        "  background: %1; color: %2;"
        "  selection-background-color: %4; selection-color: %2;"
        "  border: 1px solid %3;"
        "}"
        "QComboBox QAbstractItemView::item:selected {"
        "  background: %4; color: %2;"
        "}"
    ).arg(Style::kButtonBg, Style::kTextPrimary,
          Style::kBorder, Style::kAccent,
          Style::kInsetBg));
}

} // namespace Longpath
