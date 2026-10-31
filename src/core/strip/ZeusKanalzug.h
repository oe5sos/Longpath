#pragma once

// =================================================================
// src/core/strip/ZeusKanalzug.h  (Longpath)
// =================================================================
//
// Longpath-original. Traegt Zeus' Audio-Suite in unsere Kette.
//
// ── Warum es das gibt ────────────────────────────────────────────────
//
// Am 2026-10-31 stand in diesem Projekt noch, Zeus' Kanalzug sei nicht
// zu lesen: seine Stufen steckten in `nativePluginStates`, einem
// Beutel „zeusId -> {settingKey -> jsonValue}", und die Schluessel
// seien nicht offengelegt. Das stimmte, solange niemand eine echte
// Antwort gesehen hatte.
//
// Eine laufende Maschine (Zeus 2.0.x, „Explore without a radio", KEIN
// Funkgeraet noetig) sagt etwas anderes. Die Schluessel stehen im
// Klartext, mit der Einheit im Namen:
//
//     input-strip   param:hpf-hz 150   param:lpf-hz 3400
//                   param:rotator-stages 8   param:rotator-hz 340
//     noise-gate    param:threshold-db -45   param:attack-ms 2
//                   param:hold-ms 100   param:release-ms 250
//                   param:range-db -15   param:ratio 2   param:mode 1
//     compressor    param:threshold-db -18   param:ratio 3
//                   param:knee-db 6   param:makeup-db 0
//     limiter       param:ceiling-db -1   param:lookahead-ms 3
//     native-chain  order = input-strip,denoise,noise-gate,de-esser,eq,
//                   broadcast-agc,compressor,mbcomp,punch,saturation,
//                   exciter,bass,reverb,limiter
//
// Fuenfzehn Stufen, und zehn davon sind Stufe fuer Stufe unsere. Das
// ist kein Zufall: beide Ketten stammen von AetherSDR ab, und darum
// bedeutet `param:threshold-db` bei Zeus dasselbe wie `thresholdDb`
// bei uns. Auch die Bandarten des Entzerrers zaehlen gleich
// (0 Glocke, 1 tiefe Flanke, 2 hohe Flanke).
//
// ── Was NICHT abgebildet wird, und warum ─────────────────────────────
//
//   mbcomp (Mehrband-Kompressor, vier Baender), punch (Anschlag) und
//   broadcast-agc gibt es in Longpath nicht. Sie werden GEMELDET, und
//   zwar mit ihren Werten — nicht in eine aehnlich klingende Stufe
//   hineingebogen. Ein Vierband-Kompressor ist kein Einband-Kompressor
//   mit anderen Zahlen.
//
//   VST-Einschuebe (`vstPluginStates`) tragen base64-Zustaende fremder
//   Plugins. Sie sind nicht lesbar und auch nicht gemeint.
//
//   Was Zeus NICHT hat: unseren Clipper. Er wird ausgeschaltet, wie
//   AetherPreset es mit den Stufen tut, die Aether nicht kennt — ein
//   eingelesenes Profil darf nichts mitlaufen lassen, das der
//   Betreiber in der Vorlage nie gehoert hat.
//
// Der Hauptschalter der Kette bleibt unberuehrt. Dieselbe Regel wie
// bei AetherPreset und StripSettings::applyBuiltIn: ein Profil
// entscheidet, WIE die Kette klingt, nicht OB sie im Sendeweg steht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QString>
#include <QStringList>

class QJsonObject;

namespace Longpath {

class StripChain;

namespace ZeusKanalzug {

struct Ergebnis {
    bool        ok{false};
    QString     name;
    QString     fehler;
    /// Was nicht 1:1 uebernommen werden konnte — mit Werten, damit der
    /// Betreiber es von Hand nachstellen kann, wenn er will.
    QStringList hinweise;
    /// Wie viele Stufen wirklich gesetzt wurden.
    int         stufen{0};
};

/// Ein Zeus-Profil (ein Element aus `profiles`) in die Kette.
///
/// Erwartet das GANZE Profilobjekt, nicht nur `nativePluginStates` —
/// der Name und die Kettenreihenfolge stehen daneben.
Ergebnis anwenden(const QJsonObject& profil, StripChain& kette);

/// Ob dieses Profil ueberhaupt eine Audio-Suite traegt. Die vier
/// Werksprofile tun es; ein Profil, das nur den WDSP-Teil fuehrt,
/// nicht.
bool traegtKanalzug(const QJsonObject& profil);

} // namespace ZeusKanalzug
} // namespace Longpath
