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

#include "core/Sprache.h"

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
    case Group::Clean:    return Sprache::T(QStringLiteral("CLEAN UP"));
    case Group::Eq:       return Sprache::T(QStringLiteral("SHAPE"));
    case Group::DeEss:    return Sprache::T(QStringLiteral("SIBILANCE"));
    case Group::Compress: return Sprache::T(QStringLiteral("LEVEL"));
    case Group::Colour:   return Sprache::T(QStringLiteral("COLOUR"));
    case Group::Final:    return Sprache::T(QStringLiteral("LAST IN LINE"));
    case Group::Measure:  return Sprache::T(QStringLiteral("MEASURE ONLY"));
    default:              return Sprache::T(QStringLiteral("NOT SORTED YET"));
    }
}

QString groupHint(Group g)
{
    switch (g) {
    case Group::Clean:
        return Sprache::T(QStringLiteral(
            "Noise and room, before anything else sees them. Drop these "
            "in front of the Gate."));
    case Group::Eq:
        return Sprache::T(QStringLiteral(
            "Shaping the voice. After the Gate, before the compressor — "
            "so the compressor reacts to the voice you shaped."));
    case Group::DeEss:
        return Sprache::T(QStringLiteral(
            "Sibilance. After the EQ, because lifting the top is what "
            "usually creates it in the first place."));
    case Group::Compress:
        return Sprache::T(QStringLiteral(
            "Evening out the level. After the de-esser, so it is not "
            "pumping on an S."));
    case Group::Colour:
        return Sprache::T(QStringLiteral(
            "Saturation and harmonics. After the compressor, which is "
            "where it stays controlled."));
    case Group::Final:
        return Sprache::T(QStringLiteral(
            "Clippers and limiters. After the strip's own limiter — but "
            "be aware anything that adds level there undoes the ceiling "
            "it just set."));
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


// ── Was das Ding tut, in einem Satz ──────────────────────────────────
//
// Die Tabelle deckt Apples eigene Audio Units (die auf jedem Mac
// liegen und sonst die halbe Liste füllen) und die Stücke ab, die
// hier ausgesucht und geprüft wurden. Alles andere bekommt die
// Gruppenauskunft -- und die sagt dazu, dass sie geraten ist.
//
// Die Sätze nennen auch, was NICHT taugt. „AURogerBeep: ein Scherz
// aus der Funk-Ecke" ist eine nützlichere Auskunft als Schweigen,
// und „AUNetSend schickt Ton über das Netz, kein Effekt" bewahrt
// davor, es auszuprobieren.
namespace {

struct Bekannt { const char* teil; const char* satz; };

constexpr Bekannt kBekannt[] = {
    { "AUBandpass",
      "Apples Bandpass: lässt nur einen Bereich durch. Im Sendeweg selten nützlich — unser Kanalfilter oben macht das schon, und steiler." },
    { "AUDelay",
      "Apples Echo. Gehört nicht in einen Sendeweg: ein Echo auf der Stimme kostet Verständlichkeit und sonst nichts." },
    { "AUDistortion",
      "Apples Verzerrer. Als Effekt gedacht, nicht als Sättigung — unsere Röhre macht das Gewollte, das hier macht es hörbar kaputt." },
    { "AUDynamicsProcessor",
      "Apples Kompressor mit Expander. Brauchbar, aber unser Kompressor und unser Gate sind für Sprache eingerichtet und zeigen, was sie tun." },
    { "AUFilter",
      "Apples Mehrzweckfilter (Hoch-, Tief-, Glocke). Kann alles ein bisschen; für die Stimme ist der Entzerrer oben genauer." },
    { "AUGraphicEQ",
      "Apples Grafik-Entzerrer, 10 oder 31 feste Bänder. Dasselbe Prinzip wie unsere einfache Ansicht, ohne die Namen darüber." },
    { "AUHighShelfFilter",
      "Apples Höhenregler (Kuhschwanz). Hebt oder senkt alles oberhalb einer Frequenz." },
    { "AUHipass",
      "Apples Hochpass: nimmt die Tiefen weg. Gegen Rumpeln und Trittschall — aber unser Gate und der Kanal tun das schon." },
    { "AULowpass",
      "Apples Tiefpass: nimmt die Höhen weg." },
    { "AULowShelfFilter",
      "Apples Tiefenregler (Kuhschwanz). Hebt oder senkt alles unterhalb einer Frequenz." },
    { "AUMatrixReverb",
      "Apples Hall. Auf Sendung füllt Hall die Lücken zwischen den Wörtern — genau die braucht die Gegenseite, um dich vom Rauschen zu trennen." },
    { "AUMultibandCompressor",
      "Apples Mehrband-Kompressor: komprimiert Tiefen, Mitten und Höhen getrennt. Das kann etwas, was unsere Kette nicht hat — kräftig und schwer einzustellen." },
    { "AUNBandEQ",
      "Apples parametrischer Entzerrer mit frei wählbarer Bandzahl. Sauber gerechnet; die Oberfläche ist eine Zahlentabelle." },
    { "AUNetSend",
      "Schickt Ton über das Netz an einen anderen Rechner. Kein Effekt — in einem Sendeweg hat das nichts verloren." },
    { "AUNewPitch",
      "Verschiebt die Tonhöhe, ohne das Tempo zu ändern. Als Werkzeug interessant, auf Sendung eine Verfälschung deiner Stimme." },
    { "AUParametricEQ",
      "Apples einzelne parametrische Glocke. Für eine gezielte Korrektur brauchbar, für eine Kurve zu wenig." },
    { "AUPeakLimiter",
      "Apples Spitzenbegrenzer. Unser Begrenzer am Ende der Kette tut dasselbe und zeigt dabei, wieviel er arbeitet." },
    { "AUPitch",
      "Ältere Tonhöhenverschiebung von Apple. Siehe AUNewPitch — nichts für den Sendeweg." },
    { "AUReverb2",
      "Apples zweiter Hall. Dieselbe Warnung wie beim Hall oben: Lücken füllen kostet Lesbarkeit." },
    { "AURogerBeep",
      "Setzt einen Piepton ans Ende jeder Sprechpause. Ein Scherz aus der Funk-Ecke; technisch harmlos, auf dem Band eine Zumutung." },
    { "AURoundTripAAC",
      "Schickt den Ton durch eine AAC-Kodierung und zurück — zum Hören, was eine Kompression anrichtet. Ein Prüfwerkzeug, kein Effekt." },
    { "AUSampleDelay",
      "Verzögert um einzelne Abtastwerte. Für das Ausrichten zweier Wege gedacht, nicht für eine Stimme." },
    { "AUSoundIsolation",
      "Apples Stimmentrennung: trennt Sprache vom Rest. Dasselbe Ziel wie unsere Entrausch-Stufe — einen Vergleich wert." },
    { "TDR Nova",
      "Dynamischer Entzerrer: ein Band, das nur dann greift, wenn es zu laut wird. Genau das kann unsere Kette nicht — als De-Esser, der nur bei S-Lauten arbeitet, ist er hier das stärkste Stück." },
    { "TDR Kotelnikov",
      "Studio-Kompressor, sehr sauber und sehr durchsichtig. Kein Ersatz für unseren, sondern ein Maßstab zum Danebenhalten." },
    { "TDR VOS SlickEQ",
      "Entzerrer mit drei Bändern im Stil alter Mischpulte. Klingt angenehm, ist aber grob — für die gezielte Korrektur zu wenig Bänder." },
    { "Bertom Denoiser",
      "Entrauschen, spektral. Dieselbe Aufgabe wie unsere Entrausch-Stufe; der Vergleich der beiden sagt mehr als jedes Datenblatt." },
    { "ColourEQ",
      "Entzerrer von DDMF. Viele Bänder, eigene Oberfläche — eine ernsthafte Alternative zu unserem, wenn du sie kennst." },
    { "ZL Equalizer",
      "Entzerrer mit dynamischen Bändern und Gegenhören. Mächtig; die Einarbeitung ist kein Nachmittag." },
};

const Bekannt* finde(const QString& name)
{
    for (const Bekannt& b : kBekannt) {
        if (name.contains(QLatin1String(b.teil), Qt::CaseInsensitive)) {
            return &b;
        }
    }
    return nullptr;
}

} // namespace

bool istBekannt(const QString& name) { return finde(name) != nullptr; }

QString beschreibung(const QString& name, const QString& vendor)
{
    if (const Bekannt* b = finde(name)) {
        return QString::fromUtf8(b->satz);
    }
    // Kein erfundener Werbetext. Was wir haben, ist die geratene
    // Gruppe -- und dass sie geraten ist, steht dabei.
    const Group g = guess(name, vendor);
    if (g == Group::Unknown) {
        return QStringLiteral(
            "Unbekannt. Weder Name noch Hersteller verraten, was es tut "
            "— ausprobieren, und zwar mit dem Mithörer, nicht auf Sendung.");
    }
    return QStringLiteral("Vermutlich: %1. %2 (Die Zuordnung ist aus dem "
                          "Namen geraten.)")
        .arg(groupName(g), groupHint(g));
}

} // namespace Longpath::PluginPlacement
