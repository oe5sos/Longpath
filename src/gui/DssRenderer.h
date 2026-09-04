#pragma once

// =================================================================
// src/gui/DssRenderer.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Ported from AetherSDR `src/gui/DssRenderer.{h,cpp}` (AetherSDR
//   31b29583). AetherSDR is licensed under the GNU General Public
//   License v3. NereusSDR is also GPLv3. Attribution follows GPLv3 §5
//   requirements.
//
// ── Was hier NICHT mitkam ─────────────────────────────────────────────
//
// AetherSDRs DssRenderer traegt zusaetzlich: die GPU-Mesh-Zubehoer
// (rowDataRing/headRing/generation() usw. fuer einen QRhi-Hoehenkarten-
// Shader), die "supplemental"-Kanaele fuer FLEX-eigene Wasserfall-
// Kacheln, die von der FFT-Ansicht abweichen (NereusSDR hat keine
// solche native Kachel-Quelle), Umprojektion beim Schwenken/Zoomen
// (reprojectFrequencyFrame — hier faengt ein Bandwechsel die Historie
// stattdessen einfach neu ein) und eine eigene tiefe Scrollback-Historie
// (die laeuft in NereusSDR ueber WaterfallHistoryBuffer). Das ist Absicht
// fuer die erste, CPU-gemalte Fassung: ein QImage durch die bestehende
// Overlay-Pipeline zeichnen braucht keinen neuen Shader. Ein GPU-
// Hoehenkarten-Pfad kann spaeter denselben Ringpuffer weiterverwenden.
//
// =================================================================
// ── Was hier ANDERS ist ────────────────────────────────────────────────
//
// AetherSDRs CPU-Fassung malt mit QPainter: je Zeile 767 Trapeze und 767
// Linien, hinten nach vorn, jeder Pixel bis zu 96-mal uebermalt. Gemessen
// (MacBook Air, Apple Silicon, -O3, 2026-09-03): 0,6 s je Bild bei
// 1400x450 px, 1,3 s bei Retina 2800x900 px -- in AetherSDR selbst nur
// der Notpfad, wenn das GPU-Mesh nicht angelegt werden kann. rebuild()
// rastert deshalb direkt: Zeilen von vorn nach hinten gegen einen
// Horizont je Spalte, jeder Vorhangpixel genau einmal geschrieben; nur
// die Kantenglaettung des Kamms wird gesammelt und am Ende in
// Malreihenfolge gemischt. Das Bild ist dasselbe (Geometrie- und
// Farbformeln unveraendert, Kamm-Stiftbreite als Deckung nachgebildet),
// gemessen 7-10 ms je Bild statt 0,6-1,3 s. Siehe DssRenderer.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-03 — Ported (reduced scope, CPU-only) in C++20/Qt6 for
//                 NereusSDR by Martin Fischer (OE5SOS), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-03 — rebuild() rasterises directly (horizon algorithm, crest
//                 anti-aliasing as coverage) instead of QPainter polygons;
//                 same picture, ~100x faster. Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QColor>
#include <QImage>
#include <QSize>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

namespace Longpath {

// ─── Stacked-trace spectrum stream surface ──────────────────────────────
//
// Renders a perspective stacked-trace spectrum stream: a rolling history of
// FFT rows composed back-to-front (painter's algorithm -- that is the
// picture; the implementation walks front to back, see below) as a receding
// trapezoid. The newest trace spans the full width across the front; older
// traces recede into a narrower, higher trapezoid. Each ridge is filled
// down to the plot floor so nearer traces occlude farther ones. Fill
// colour follows amplitude via an injected palette and dims with depth for
// atmospheric perspective; a bright per-amplitude line tops each ridge.
//
// The rendered surface is cached in a QImage and rebuilt ONLY when a new
// row arrives, the target size changes, or the amplitude mapping / palette
// changes. The rebuild is a direct rasteriser (see rebuild() in the .cpp):
// traces are walked front to back against a per-column horizon so every
// curtain pixel is written exactly once, with the crest's anti-aliasing
// composited afterwards -- about ten milliseconds even at Retina
// panadapter sizes, where the QPainter painting it replaces needed well
// over a second. The image composites through the existing QRhi overlay
// pipeline (no new shaders). The renderer is standalone and knows nothing
// about SpectrumWidget or QRhi.
class DssRenderer
{
public:
    static constexpr int kVisibleRows = 96;    // front → back display depth
    static constexpr int kTransitionRows = 8;  // headroom, mirrors AetherSDR
    static constexpr int kRows = kVisibleRows + kTransitionRows;
    static constexpr int kCols = 768;  // resampled columns per row

    // Perspective geometry of the surface. Shared constants so a future
    // GPU height-map path can apply the SAME formulas from a uniform block
    // — single source of truth, CPU and GPU can't drift apart.
    static constexpr float kBackWidthFrac     = 0.60f;  // back row width / front
    static constexpr float kDepthSpanFrac     = 0.58f;  // baseline rise to the back
    static constexpr float kFrontMaxRidgeFrac = 0.46f;  // front ridge height / plot H
    static constexpr float kHaze              = 0.16f;  // fade toward bg with depth

    // Maps a dBm value to an RGB colour using the host's panadapter palette.
    using PaletteFn = std::function<QRgb(float dbm)>;

    // Push one freshly-decoded FFT row (any bin count, dBm, already at
    // display resolution). Peak-preserving downsample to kCols, median-of-3
    // impulse rejection, and a light temporal blend, then store it as the
    // newest (front) trace.
    void pushRow(const QVector<float>& binsDbm);

    void invalidate() { m_dirty = true; }
    bool hasData() const { return m_count > 0; }
    void clear();

    int visibleRowCount() const { return std::min(m_count, kVisibleRows); }

    // Return the cached surface sized to px. The plot region (everything
    // above the bottom scaleStripPx) is painted opaque over bgFill; the
    // scale strip is left transparent so the host can composite a scale on
    // top.
    //
    // Ridge HEIGHT is anchored to the noise floor: a column maps to
    // strength = clamp((dbm - floorDbm) / rangeDb, 0, 1), so floorDbm sits
    // at the baseline (≈0 height) and floorDbm+rangeDb reaches the full
    // ridge. Colour comes from palette(dbm), independent of height.
    // paletteToken lets the host signal palette changes without us
    // inspecting them. Rebuilds only when something relevant changed.
    // zCurve (<1) lifts the floor→signal band so weak signal still shows
    // some relief instead of collapsing flat at the baseline.
    const QImage& image(const QSize& px, int scaleStripPx,
                        float floorDbm, float rangeDb, float zCurve,
                        const PaletteFn& palette, quint64 paletteToken,
                        const QColor& bgFill);

private:
    void rebuild(const QSize& px, int scaleStripPx, float floorDbm,
                 float rangeDb, float zCurve, const PaletteFn& palette,
                 const QColor& bgFill);

    // Returns the dBm row at the given age (0 = newest/front).
    const std::array<float, kCols>& rowAt(int age) const;

    // Circular store: m_head indexes the newest row.
    std::array<std::array<float, kCols>, kRows> m_rows{};
    int  m_head  = 0;   // index of the newest row
    int  m_count = 0;   // number of valid rows (0..kRows)
    bool m_dirty = true;

    // Last two RAW (pre-smoothing) resampled rows, for temporal median-of-3
    // impulse rejection of broadband interference bursts.
    std::array<float, kCols> m_rawPrev1{};
    std::array<float, kCols> m_rawPrev2{};
    int m_rawHistCount = 0;

    // Crest anti-aliasing that cannot be applied in place while walking
    // front to back: a partially covered rim pixel ABOVE its own curtain has
    // to blend over traces that are drawn later (they lie behind). Collected
    // during rebuild(), composited at its end, kept as a member only so the
    // capacity survives between frames.
    struct PartialCrest {
        int     x;
        int     y;
        quint32 rgba;
        float   cov;
    };
    std::vector<PartialCrest> m_partials;
    bool m_rebuilding = false;   // re-entrancy guard for image() (see rebuild)

    // Cache + the parameters it was built for (rebuild on any change).
    QImage  m_cache;
    QSize   m_cacheSize;
    int     m_cacheScaleStrip   = -1;
    float   m_cacheFloor        = 0.0f;
    float   m_cacheRange        = 0.0f;
    float   m_cacheZCurve       = 0.0f;
    quint64 m_cachePaletteToken = ~0ull;
};

}  // namespace Longpath
