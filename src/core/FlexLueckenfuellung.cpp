// =================================================================
// src/core/FlexLueckenfuellung.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und die Begruendung: siehe FlexLueckenfuellung.h.
// Ported from AetherSDR src/core/PacketLossConcealment.{h,cpp} at
// d58e2b8a (GPLv3, primary author Jeremy [KK7GWY]) — project-level
// citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexLueckenfuellung.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Longpath::FlexVita {

namespace {

// From AetherSDR PacketLossConcealment.cpp:11-14 [@d58e2b8a] — und die
// Begruendung im Wortlaut der Vorlage: „M_PI is a POSIX extension;
// MSVC only exposes it when _USE_MATH_DEFINES is set before <cmath>.
// Define our own portable constant so this translation unit doesn't
// depend on either path." Gilt fuer Longpath genauso: Windows baut mit
// MSVC (siehe longpath-windows-msvc-vs-mingw).
constexpr float kPi = 3.14159265358979323846f;

constexpr int kKanaele = 2;

}  // namespace

// From AetherSDR PacketLossConcealment.cpp:19-80 [@d58e2b8a] —
// applyConcealmentFade(), Schritt fuer Schritt.
QVector<float> fuelleLuecke(QVector<float> ton, Lueckenzustand& z, bool an)
{
    const int neueRahmen = ton.size() / kKanaele;
    if (neueRahmen <= 0) {
        z.offeneLuecken = 0;
        return ton;
    }

    const float* ein = ton.constData();

    const bool fuellen = an && z.offeneLuecken > 0 && z.letzteRahmen > 0;
    if (!fuellen) {
        z.randL = ein[(neueRahmen - 1) * kKanaele];
        z.randR = ein[(neueRahmen - 1) * kKanaele + 1];
        z.letzteRahmen   = neueRahmen;
        z.offeneLuecken  = 0;
        return ton;
    }

    const int fuellRahmen = z.offeneLuecken * z.letzteRahmen;
    QVector<float> aus((fuellRahmen + neueRahmen) * kKanaele);
    float* dst = aus.data();

    // Kosinus-Blende vom gemerkten Rand auf null (~2 ms).
    const int runter = std::min(kBlendRahmen, fuellRahmen);
    for (int i = 0; i < runter; ++i) {
        const float w = 0.5f * (1.0f + std::cos(kPi * float(i) / float(runter)));
        dst[i * kKanaele]     = z.randL * w;
        dst[i * kKanaele + 1] = z.randR * w;
    }
    if (fuellRahmen > runter) {
        std::memset(dst + runter * kKanaele, 0,
                    std::size_t(fuellRahmen - runter) * kKanaele
                        * sizeof(float));
    }

    // Kosinus-Blende hinauf in die ersten ~2 ms des neuen Pakets.
    const int hoch = std::min(kBlendRahmen, neueRahmen);
    for (int i = 0; i < hoch; ++i) {
        const float w = 0.5f * (1.0f - std::cos(kPi * float(i) / float(hoch)));
        dst[(fuellRahmen + i) * kKanaele] =
            ein[i * kKanaele] * w;
        dst[(fuellRahmen + i) * kKanaele + 1] =
            ein[i * kKanaele + 1] * w;
    }
    if (neueRahmen > hoch) {
        std::memcpy(dst + (fuellRahmen + hoch) * kKanaele,
                    ein + hoch * kKanaele,
                    std::size_t(neueRahmen - hoch) * kKanaele * sizeof(float));
    }

    z.randL = ein[(neueRahmen - 1) * kKanaele];
    z.randR = ein[(neueRahmen - 1) * kKanaele + 1];
    z.letzteRahmen  = neueRahmen;
    z.offeneLuecken = 0;
    return aus;
}

}  // namespace Longpath::FlexVita
