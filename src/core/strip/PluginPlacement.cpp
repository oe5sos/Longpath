// =================================================================
// src/core/strip/PluginPlacement.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe die Kopfdatei, warum hier geraten wird und
// warum das Raten sichtbar bleiben muss.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include "core/strip/PluginPlacement.h"

namespace Longpath::PluginPlacement {

namespace {

struct Rule { const char* needle; Group group; };

// Wortstaemme, nach Gruppe. Die Reihenfolge entscheidet: der erste
// Treffer gewinnt, also stehen die eindeutigen Namen vor den
// allgemeinen Woertern. „DeEsser" muss vor „Esser"... nein, vor
// „Dyn" stehen, und „Multiband" vor „Band".
constexpr Rule kRules[] = {
    // Eindeutige Namen zuerst -- die bekannten Verdaechtigen aus der
    // Liste, die dem Betreiber vorgelegt wurde.
    // Apples eigene Stimmfreistellung. Am laufenden Fenster gesehen:
    // sie landete unter „noch nicht eingeordnet", obwohl sie genau
    // das tut, wonach der Betreiber heute Vormittag gefragt hat
    // („die hintergrundgeraeusche werden viel zu viel aufgenommen,
    // es hallt vom raum").
    {"isolation",  Group::Clean},
    {"denoiser",   Group::Clean},
    {"rnnoise",    Group::Clean},
    {"noise",      Group::Clean},
    {"spectral",   Group::Clean},
    {"gate",       Group::Clean},
    {"expander",   Group::Clean},

    {"de-ess",     Group::DeEss},
    {"deess",      Group::DeEss},
    {"de ess",     Group::DeEss},
    {"sibil",      Group::DeEss},

    {"nova",       Group::Eq},
    {"slickeq",    Group::Eq},
    {"merica",     Group::Eq},
    {"pultec",     Group::Eq},
    {"equal",      Group::Eq},
    {"meq",        Group::Eq},
    {"filter",     Group::Eq},
    {"hipass",     Group::Eq},
    {"lowpass",    Group::Eq},
    {"highpass",   Group::Eq},
    {"bandpass",   Group::Eq},
    {"shelf",      Group::Eq},
    {"air ",       Group::Eq},
    {"eq",         Group::Eq},

    {"kotelnikov", Group::Compress},
    {"dc1a",       Group::Compress},
    {"mjuc",       Group::Compress},
    {"lala",       Group::Compress},
    {"fetish",     Group::Compress},
    {"la-2a",      Group::Compress},
    {"1176",       Group::Compress},
    {"pressure",   Group::Compress},
    {"compress",   Group::Compress},
    {"dynamics",   Group::Compress},
    {"leveler",    Group::Compress},
    {"levell",     Group::Compress},

    {"ivgi",       Group::Colour},
    {"boogex",     Group::Colour},
    {"satur",      Group::Colour},
    {"tube",       Group::Colour},
    {"tape",       Group::Colour},
    {"exciter",    Group::Colour},
    {"distort",    Group::Colour},
    {"overdrive",  Group::Colour},
    {"console",    Group::Colour},

    {"cliponly",   Group::Final},
    {"clip",       Group::Final},
    {"limiter",    Group::Final},
    {"maximi",     Group::Final},

    {"span",       Group::Measure},
    {"loudness",   Group::Measure},
    {"analy",      Group::Measure},
    {"meter",      Group::Measure},
    {"scope",      Group::Measure},
};

} // namespace

QString groupName(Group g)
{
    switch (g) {
    case Group::Clean:    return QStringLiteral("CLEAN UP");
    case Group::Eq:       return QStringLiteral("SHAPE");
    case Group::DeEss:    return QStringLiteral("SIBILANCE");
    case Group::Compress: return QStringLiteral("LEVEL");
    case Group::Colour:   return QStringLiteral("COLOUR");
    case Group::Final:    return QStringLiteral("LAST IN LINE");
    case Group::Measure:  return QStringLiteral("MEASURE ONLY");
    default:              return QStringLiteral("NOT SORTED YET");
    }
}

QString groupHint(Group g)
{
    switch (g) {
    case Group::Clean:
        return QStringLiteral(
            "Noise and room, before anything else sees them. Drop these "
            "in front of the Gate.");
    case Group::Eq:
        return QStringLiteral(
            "Shaping the voice. After the Gate, before the compressor — "
            "so the compressor reacts to the voice you shaped.");
    case Group::DeEss:
        return QStringLiteral(
            "Sibilance. After the EQ, because lifting the top is what "
            "usually creates it in the first place.");
    case Group::Compress:
        return QStringLiteral(
            "Evening out the level. After the de-esser, so it is not "
            "pumping on an S.");
    case Group::Colour:
        return QStringLiteral(
            "Saturation and harmonics. After the compressor, which is "
            "where it stays controlled.");
    case Group::Final:
        return QStringLiteral(
            "Clippers and limiters. After the strip's own limiter — but "
            "be aware anything that adds level there undoes the ceiling "
            "it just set.");
    case Group::Measure:
        return QStringLiteral(
            "These only look. Position does not matter much; put them "
            "where you want to measure.");
    default:
        return QStringLiteral(
            "Longpath could not tell what these are from their name. "
            "That is not a fault — it just means you decide where they "
            "go.");
    }
}

int suggestedPosition(Group g)
{
    switch (g) {
    case Group::Clean:    return 0;   // vor dem Gate
    case Group::Eq:       return 1;   // nach dem Gate
    case Group::DeEss:    return 2;   // nach dem Entzerrer
    case Group::Compress: return 3;   // nach dem De-Esser
    case Group::Colour:   return 4;   // nach dem Kompressor
    case Group::Final:    return StripChain::kStageCount;
    default:              return -1;
    }
}

Group guess(const QString& name, const QString& vendor)
{
    const QString hay = (name + QLatin1Char(' ') + vendor).toLower();
    for (const Rule& r : kRules) {
        if (hay.contains(QLatin1String(r.needle))) { return r.group; }
    }
    return Group::Unknown;
}

} // namespace Longpath::PluginPlacement
