// =================================================================
// src/gui/applets/eq/ClientEqIconRow.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/ClientEqIconRow.h at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Ported at the bench's request to reproduce AetherSDR's equaliser
// exactly — its display and its behaviour — rather than to approximate
// it. The DSP it drives, core/strip/ClientEq, was already a verbatim
// port of the same upstream, so the pair are back together.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; include paths rebased onto
//                 core/strip/ and gui/applets/eq/. Behaviour unchanged.
// =================================================================

#pragma once

#include "core/strip/ClientEq.h"
#include <QWidget>

class QHBoxLayout;

namespace NereusSDR {

class EqHost;

// Top-of-editor row: one small icon button per active band showing the
// filter-type curve shape (HP slope, low shelf, bell, high shelf, LP slope)
// in the band's palette color. Click cycles type forward; shift-click
// cycles backward. Right-click exposes the explicit type menu (handled
// by the canvas's existing context menu — icons route through the same
// AudioEngine mutation path).
//
// The row reflows when activeBandCount() changes; selected band gets a
// bright ring. Inactive trailing slots are hidden (not shown greyed).
class ClientEqIconRow : public QWidget {
    Q_OBJECT

public:
    explicit ClientEqIconRow(QWidget* parent = nullptr);

    void setEq(ClientEq* eq);
    void setAudioEngine(EqHost* engine);  // for persistence on edit

signals:
    void bandSelected(int idx);

public slots:
    // Called when the ClientEq instance changes on the audio side (add /
    // delete band, type change via context menu, path switch). Rebuilds
    // the icon row to match the current state.
    void refresh();
    void setSelectedBand(int idx);

private:
    class IconButton;  // defined in the .cpp

    void rebuild();

    ClientEq*    m_eq{nullptr};
    EqHost* m_audio{nullptr};
    QHBoxLayout* m_layout{nullptr};
    int          m_selectedBand{-1};
};

} // namespace NereusSDR
