// no-port-check: Longpath-original file; Thetis console.cs references are
//   inline doc comments only — logic delegates to SliceModel::presetsForMode
//   which already carries full Thetis attribution.
// =================================================================
// src/models/FilterPresetStore.cpp  (Longpath)
// =================================================================
//
// Longpath-original file. See FilterPresetStore.h for full header.
//
// =================================================================
// Modification history (Longpath):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
//   2026-09-27 -- followCwPitch (Martin Fischer, OE5SOS, AI-assisted via
//                 Anthropic Claude). See FilterPresetStore.h.
//   2026-09-28 -- default names are the Thetis button names
//                 (SliceModel::presetNamesForMode) instead of "F1".."F10"
//                 (Martin Fischer, OE5SOS, AI-assisted via Anthropic
//                 Claude).
//   2026-09-28 -- nameForEdges / defaultNameForEdges (Martin Fischer,
//                 OE5SOS, AI-assisted via Anthropic Claude).
// =================================================================

#include "FilterPresetStore.h"
#include "SliceModel.h"
#include "core/AppSettings.h"

#include <QList>
#include <optional>

namespace Longpath {

// ── Construction ──────────────────────────────────────────────────────────────

FilterPresetStore::FilterPresetStore(QObject* parent)
    : QObject(parent)
{
}

// ── Static helpers ────────────────────────────────────────────────────────────

QString FilterPresetStore::slotKey(DSPMode mode, int slot)
{
    // e.g. "filters/USB/3"
    return QStringLiteral("filters/%1/%2").arg(SliceModel::modeName(mode)).arg(slot);
}

QString FilterPresetStore::modePrefix(DSPMode mode)
{
    // e.g. "filters/USB"
    return QStringLiteral("filters/%1").arg(SliceModel::modeName(mode));
}

void FilterPresetStore::persistPreset(DSPMode mode, int slot, const FilterPreset& p)
{
    auto& s = AppSettings::instance();
    const QString prefix = slotKey(mode, slot);
    s.setValue(prefix + QStringLiteral("/name"), p.name);
    s.setValue(prefix + QStringLiteral("/low"),  QString::number(p.low));
    s.setValue(prefix + QStringLiteral("/high"), QString::number(p.high));
}

void FilterPresetStore::clearSlot(DSPMode mode, int slot)
{
    auto& s = AppSettings::instance();
    const QString prefix = slotKey(mode, slot);
    s.remove(prefix + QStringLiteral("/name"));
    s.remove(prefix + QStringLiteral("/low"));
    s.remove(prefix + QStringLiteral("/high"));
}

void FilterPresetStore::clearMode(DSPMode mode)
{
    // Clear all 10 possible slots for this mode.
    // (We always use at most 10 slots, matching Thetis F1..F10.)
    for (int slot = 0; slot < 10; ++slot) {
        clearSlot(mode, slot);
    }
}

std::optional<FilterPreset> FilterPresetStore::loadSlot(DSPMode mode, int slot)
{
    auto& s = AppSettings::instance();
    const QString prefix = slotKey(mode, slot);
    const QString nameKey = prefix + QStringLiteral("/name");
    const QString lowKey  = prefix + QStringLiteral("/low");
    const QString highKey = prefix + QStringLiteral("/high");

    // A slot is considered overridden only if all three keys are present.
    if (!s.contains(nameKey) || !s.contains(lowKey) || !s.contains(highKey)) {
        return std::nullopt;
    }

    FilterPreset p;
    p.name = s.value(nameKey).toString();
    bool lowOk = false, highOk = false;
    p.low  = s.value(lowKey).toString().toInt(&lowOk);
    p.high = s.value(highKey).toString().toInt(&highOk);
    if (!lowOk || !highOk) {
        return std::nullopt;
    }
    return p;
}

// ── defaultPreset ─────────────────────────────────────────────────────────────

// Default preset names: the Thetis button names of the slot ("5.0k",
// "1.0k", "800" …), from SliceModel::presetNamesForMode. F1..F10 are
// Thetis's slot identifiers (enum Filter), not what the button says —
// until 2026-09-28 this returned "F1".."F10" and claimed Thetis did too.
static QString defaultPresetName(DSPMode mode, int slot)
{
    const QStringList names = SliceModel::presetNamesForMode(mode);
    if (slot >= 0 && slot < names.size()) {
        return names.at(slot);
    }
    // slot is 0-based; out of range for this mode — the slot identifier
    return QStringLiteral("F%1").arg(slot + 1);
}

FilterPreset FilterPresetStore::defaultPreset(DSPMode mode, int slot)
{
    // Delegate to SliceModel::presetsForMode — the Thetis-verbatim
    // port (console.cs:5118-5515 [@852bf0e]).
    const auto pairs = SliceModel::presetsForMode(mode);
    FilterPreset p;
    p.name = defaultPresetName(mode, slot);
    if (slot >= 0 && slot < pairs.size()) {
        p.low  = pairs[slot].first;
        p.high = pairs[slot].second;
    } else {
        // Slot out of range for this mode — return a passthrough.
        p.low  = 100;
        p.high = 3000;
    }
    return p;
}

// ── nameForEdges ──────────────────────────────────────────────────────────────

// Thetis beschriftet den laufenden Filter mit dem Namen des gewaehlten
// Platzes: panelFilter.Text = "Filter - " + ...GetName(f)
// (console.cs:7721 [@852bf0e], RX2 :38256). Genaue Gleichheit, keine
// Toleranz: die Vorgaben und der laufende Durchlass werden mit derselben
// Rechnung verschoben (CW-Tonhoehe), sie bleiben also deckungsgleich.
QString FilterPresetStore::nameForEdges(DSPMode mode, int low, int high) const
{
    for (const FilterPreset& p : presetsForMode(mode)) {
        if (p.low == low && p.high == high) {
            return p.name;
        }
    }
    return QString();
}

QString FilterPresetStore::defaultNameForEdges(DSPMode mode, int low, int high)
{
    const auto edges = SliceModel::presetsForMode(mode);
    const QStringList names = SliceModel::presetNamesForMode(mode);
    for (int i = 0; i < edges.size() && i < names.size(); ++i) {
        if (edges[i].first == low && edges[i].second == high) {
            return names.at(i);
        }
    }
    return QString();
}

// ── presetsForMode ────────────────────────────────────────────────────────────

QList<FilterPreset> FilterPresetStore::presetsForMode(DSPMode mode) const
{
    const auto defaults = SliceModel::presetsForMode(mode);
    const int count = qMin(defaults.size(), 10);

    QList<FilterPreset> result;
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        auto override = loadSlot(mode, i);
        if (override.has_value()) {
            result.append(override.value());
        } else {
            FilterPreset p;
            p.name = defaultPresetName(mode, i);
            p.low  = defaults[i].first;
            p.high = defaults[i].second;
            result.append(p);
        }
    }
    return result;
}

// ── setPreset ─────────────────────────────────────────────────────────────────

void FilterPresetStore::setPreset(DSPMode mode, int slot, const FilterPreset& p)
{
    const auto defaults = SliceModel::presetsForMode(mode);
    if (slot < 0 || slot >= defaults.size()) {
        return;
    }
    persistPreset(mode, slot, p);
    emit presetsChanged(mode);
}

// ── setPresetsForMode ─────────────────────────────────────────────────────────

void FilterPresetStore::setPresetsForMode(DSPMode mode, const QList<FilterPreset>& presets)
{
    // Clear all existing overrides for this mode first, then persist each new preset.
    clearMode(mode);
    const int count = qMin(presets.size(), 10);
    for (int i = 0; i < count; ++i) {
        persistPreset(mode, i, presets[i]);
    }
    emit presetsChanged(mode);
}

// ── resetPreset ───────────────────────────────────────────────────────────────

void FilterPresetStore::resetPreset(DSPMode mode, int slot)
{
    const auto defaults = SliceModel::presetsForMode(mode);
    if (slot < 0 || slot >= defaults.size()) {
        return;
    }
    clearSlot(mode, slot);
    emit presetsChanged(mode);
}

// ── resetMode ─────────────────────────────────────────────────────────────────

void FilterPresetStore::resetMode(DSPMode mode)
{
    clearMode(mode);
    emit presetsChanged(mode);
}

// ── resetAll ──────────────────────────────────────────────────────────────────

void FilterPresetStore::resetAll()
{
    // Iterate every DSPMode value (LSB..DRM = 0..11).
    static constexpr DSPMode kAllModes[] = {
        DSPMode::LSB, DSPMode::USB, DSPMode::DSB,
        DSPMode::CWL, DSPMode::CWU, DSPMode::FM,
        DSPMode::AM,  DSPMode::DIGU, DSPMode::SPEC,
        DSPMode::DIGL, DSPMode::SAM, DSPMode::DRM
    };
    for (DSPMode m : kAllModes) {
        clearMode(m);
        emit presetsChanged(m);
    }
}

// ── followCwPitch ─────────────────────────────────────────────────────────────

void FilterPresetStore::followCwPitch(int pitchHz)
{
    for (DSPMode m : {DSPMode::CWL, DSPMode::CWU}) {
        for (int slot = 0; slot < 10; ++slot) {
            auto override = loadSlot(m, slot);
            if (!override.has_value()) {
                continue;
            }
            FilterPreset p = override.value();
            SliceModel::centreCwFilterOnPitch(p.low, p.high, m, pitchHz);
            persistPreset(m, slot, p);
        }
        emit presetsChanged(m);
    }
}

} // namespace Longpath
