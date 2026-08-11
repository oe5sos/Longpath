#pragma once

// =================================================================
// src/gui/applets/eq/EqPalette.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// One table, mapping every colour AetherSDR's equaliser widgets used
// onto NereusSDR's own.
//
// ── Why a table and not five files of edits ──────────────────────────
//
// The equaliser widgets in this directory are AetherSDR's, ported
// verbatim. EqHost already exists so that the borrowed code can talk to
// NereusSDR's parts without being rewritten — the port stays comparable
// against upstream, and a future re-sync is a copy rather than a merge.
//
// Repainting it threatens exactly that. Scattering `Style::kAccent`
// through five borrowed files makes the next re-sync a hand merge of
// hundreds of small differences.
//
// So the same trick again: the colours get NAMES here, the borrowed
// files use the names, and the mapping is one reviewable table. A
// re-sync re-applies one substitution instead of reconstructing a
// hundred decisions.
//
// ── What was already NereusSDR's ─────────────────────────────────────
//
// Most of it, and by accident of ancestry: NereusSDR's palette descends
// from AetherSDR (see docs/attribution/aethersdr-reconciliation.md), so
// seven of the curve widget's nine colours were already byte-identical
// to NereusSDR tokens. Those simply became the tokens.
//
// What is genuinely new is the band palette. Eight hues that say which
// handle belongs to which curve when eight filters overlap — hue is the
// only thing doing that job, so it could not be dropped, and NereusSDR
// had only four accents to spend. Three were added to StyleConstants.
//
// ── Two literals that used to be argued for ──────────────────────────
//
// #506070 (a scale label) and #08121d (a handle outline) were left as
// literals in an earlier pass, on the reasoning that a colour chosen to
// sit between two others stops working when snapped to one of them.
// That reasoning is sound and it does not survive the instruction:
// everything is to look like NereusSDR. They are kTextScale and
// kPanelBg now, and the difference is a few percent of luminance.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/StyleConstants.h"

#include <QColor>

#include <array>

namespace NereusSDR::EqPalette {

inline QColor of(const char* token) { return QColor(QLatin1String(token)); }

// ── Surfaces ─────────────────────────────────────────────────────────
inline QColor pageBg()    { return of(Style::kAppBg); }        // 0f0f1a
inline QColor panelBg()   { return of(Style::kPanelBg); }      // 0a0a18
inline QColor insetBg()   { return of(Style::kInsetBg); }      // 0a0a18
inline QColor buttonBg()  { return of(Style::kButtonBg); }     // 1a2a3a
inline QColor buttonHi()  { return of(Style::kButtonHover); }  // 203040
inline QColor border()    { return of(Style::kBorderSubtle); } // 203040
inline QColor borderIn()  { return of(Style::kInsetBorder); }  // 1e2e3e

// ── Text ─────────────────────────────────────────────────────────────
inline QColor textBright(){ return of(Style::kTextPrimary); }   // c8d8e8
inline QColor textDim()   { return of(Style::kTextSecondary); } // 8090a0
inline QColor textScale() { return of(Style::kTextScale); }     // 607080
inline QColor textOff()   { return of(Style::kTextInactive); }  // 405060
inline QColor title()     { return of(Style::kTitleText); }     // 8aa8c0

// ── Meaning ──────────────────────────────────────────────────────────
inline QColor accent()    { return of(Style::kAccent); }        // 00b4d8
inline QColor good()      { return of(Style::kGreenText); }     // 00ff88
inline QColor goodDim()   { return of(Style::kGreenBorder); }   // 00a060
inline QColor warn()      { return of(Style::kAmberText); }     // ffb800
inline QColor warnDim()   { return of(Style::kAmberWarn); }     // ddbb00
inline QColor warnBg()    { return of(Style::kAmberBg); }       // 604000
inline QColor bad()       { return of(Style::kRedBorder); }     // ff4444
inline QColor badBg()     { return of(Style::kRedBg); }         // cc2222

// ── One band from another ────────────────────────────────────────────
//
// Grey at both ends: those slots hold the high-pass and low-pass
// slopes, which are shapes rather than bumps and should not compete
// with the shaping bands for attention. Interpolated beyond eight.
inline const std::array<QColor, 8>& bands()
{
    static const std::array<QColor, 8> p = {
        of(Style::kEqBand0), of(Style::kEqBand1),
        of(Style::kEqBand2), of(Style::kEqBand3),
        of(Style::kEqBand4), of(Style::kEqBand5),
        of(Style::kEqBand6), of(Style::kEqBand7),
    };
    return p;
}

// ── The analyser fill behind the curve ───────────────────────────────
//
// A gradient, so it needs three stops rather than one token. Built from
// the accent so it follows if NereusSDR ever repaints, instead of the
// three unrelated blues AetherSDR had.
inline QColor analyserTop()
{
    QColor c = accent();
    c.setAlpha(140);
    return c;
}
inline QColor analyserMid()
{
    QColor c = accent();
    c = c.darker(160);
    c.setAlpha(70);
    return c;
}
inline QColor analyserBottom()
{
    QColor c = accent();
    c = c.darker(260);
    c.setAlpha(0);
    return c;
}

// The peak-hold line over the analyser: bright, neutral, and not one of
// the band hues — it is a measurement, not a filter.
inline QColor analyserPeak()
{
    QColor c = textBright();
    c.setAlpha(210);
    return c;
}

// ── The mode strip along the bottom of the curve ─────────────────────
//
// AetherSDR coloured these from its panadapter band plan. NereusSDR has
// its own band-plan colours elsewhere; these follow the accents so the
// strip reads as part of this widget rather than as a quotation from
// another one.
inline QColor modeSsb()   { return of(Style::kAmberText); }
inline QColor modeEssb()  { return of(Style::kAccent); }
inline QColor modeWide()  { return of(Style::kRedBorder); }

} // namespace NereusSDR::EqPalette
