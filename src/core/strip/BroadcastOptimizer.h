#pragma once

// =================================================================
// src/core/strip/BroadcastOptimizer.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Vier Sendekanaele, die Messung dazu -- und sonst nichts.
//
// ── Was dieses Stueck einmal war, und warum es das nicht mehr ist ────
//
// Es begann als Optimierer: eine Messung hinein, vier vollstaendig
// hergeleitete Ketten heraus (Hochpass, Kuhschwaenze, Kompressor,
// Expander, Charaktere). Einen Tag lang, am 2026-10-09, wurde das
// gegen die Stimme des Betreibers gefahren und jedes Mal verworfen:
//
//   "Das Original-Audio ist gut, nur saemtliche Bearbeitungen sind
//    eine Katastrophe." · "hier ist kein Bass, hier fehlt es einfach
//    an allem." · "also besser wird das audio nicht."
//
// Fuenf echte Fehler kamen dabei heraus und sind behoben (sie stehen
// im Entwurfsblatt), aber der Befund darunter blieb: eine
// hergeleitete Kette klang nie besser als die, die er sich selbst
// eingestellt hatte. Sein Schluss, und damit die Entscheidung:
//
//   "ich glaube, wir lassen das automatische verbessern."
//
// ── Was geblieben ist ────────────────────────────────────────────────
//
// Die drei Dinge, die sich an demselben Tag als tragfaehig erwiesen
// haben:
//
//   DER KANAL. Vier Breiten mit den Zahlen des Betreibers: Contest
//   200-2500, dann 150-2700, 100-3200, und breit 50-4050. Unten liegt
//   der groessere Teil des hoerbaren Unterschieds -- 50 gegen 200 Hz
//   sind zwei Oktaven Koerper.
//
//   DAS HOEREN. applyChannelFilter() bildet den Sendeweg auf einem
//   Puffer nach, damit "wie klingt meine Einstellung bei 2,3 k" eine
//   Frage ist, die man beantworten kann, ohne zu senden.
//
//   DIE MESSUNG. Was VoiceAnalyzer findet, wird berichtet und nicht
//   angewandt: Grundton, Stoerabstand, Zischfrequenz, der Punkt, an
//   dem das Mikrofon aufhoert. Beobachtungen, aus denen der Bediener
//   seine Schluesse zieht.
//
// Und saveCurrentAsProfile(): die von Hand eingestellte Kette unter
// dem Namen einer Breite ablegen. Das ist die ganze Automatik, die
// uebrig ist, und sie erfindet nichts.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/VoiceAnalyzer.h"

#include <QString>
#include <QStringList>
#include <QMetaType>
#include <QVector>

namespace Longpath {

class StripChain;

namespace BroadcastOptimizer {

// ── The four widths ──────────────────────────────────────────────────
//
// Asked for by name. They are not arbitrary: 2.3 kHz is what a crowded
// receiver is set to, 2.7 kHz is the everyday default most of the band
// is listening on, 3.5 kHz is as wide as is polite when the band is
// quiet, and 4.0 kHz is the one that only pays off when the other
// operator has opened their filter to match.
inline constexpr int kWidthCount = 4;
inline constexpr double kWidthHz[kWidthCount] = {
    2300.0, 2700.0, 3200.0, 4000.0,
};

// What every profile trims the voice onto, in dBFS. Zeus's own
// target, read off its panel: "trim +9.5 dB to -20 dBFS".
inline constexpr double kTargetLevelDbFs = -20.0;

// The name a profile is saved and listed under. Short, sorts in width
// order, and says where it came from.
QString presetName(double widthHz);

// Was ueber eine Breite zu sagen ist: ihr Kanal und was die Messung
// dazu beobachtet hat. Keine Einstellungen -- die macht der Bediener
// von Hand, siehe saveCurrentAsProfile().
struct Plan {
    double  widthHz{2700.0};
    QString name;
    QString headline;

    // ── Der Kanal ────────────────────────────────────────────────────
    //
    // Die Zahlen des Betreibers, 2026-10-09: "ein dx oder contest
    // profil ist im bereich 200-2500(2700), ein 4 k profil ist von ca
    // 50-4050." Unten liegt der groessere Teil des Unterschieds.
    int     lowCutHz{150};
    int     highCutHz{2700};

    // Was die Messung ueber diese Breite sagt -- Beobachtungen, keine
    // Einstellungen. Eine Zeile oben, die Einzelheiten darunter.
    QStringList notes;

    // Gesetzt, wenn die Messung sagt, dass diese Breite nicht haelt,
    // was ihr Name verspricht: ein Mikrofon, das vorher aufhoert.
    QString caveat;
};

// ── The whole job ────────────────────────────────────────────────────
//
// Pure. Takes the measurement, returns four plans in kWidthHz order.
// An invalid or clipped measurement returns an empty vector rather
// than four confident plans built on a spectrum that is not real —
// the same refusal StripTuner makes, for the same reason.
QVector<Plan> plan(const VoiceAnalysis& a);

// The one to start from, out of the four: the plan for the width
// closest to what the transmitter is actually set to. Returns an index
// into plan()'s result, or -1 when there are no plans.
int bestIndex(const QVector<Plan>& plans, double currentTxWidthHz);

// ── Das Kanalfilter, zum Hoeren ──────────────────────────────────────
//
// Bildet das Sendefilter des Plans auf einem Puffer nach: Hochpass auf
// lowCutHz, Tiefpass zweiter Ordnung ZWEIMAL auf highCutHz.
//
// Es steht hier und nicht im Fenster, weil es sonst DSP waere, an die
// kein Pruefstand herankommt -- und die Frage "klingt 4,0 k wirklich
// breiter als 2,7 k" ist genau die, die ein Pruefstand beantworten
// muss statt ein Ohr. Ohne diese zwei Filter ist die Vorschau eines
// schmalen Profils die Vorschau von etwas, das so nie gesendet wird.
void applyChannelFilter(float* samples, int frames, int sampleRate,
                        const Plan& p);

// ── Das Profil speichern ─────────────────────────────────────────────
//
// Nimmt die Kette, wie sie GERADE ist -- von Hand eingestellt -- und
// legt sie unter dem Namen dieser Breite ab. Das ist die ganze
// Automatik, die geblieben ist, und sie erfindet nichts: sie haelt
// fest, was der Bediener selbst gemacht hat.
//
// Betreiber, 2026-10-09: "ich glaube, wir lassen das automatische
// verbessern" -- nachdem ein Tag Herleitung von Ketten aus der
// Messung nichts ergeben hat, was besser klang als seine eigene
// Einstellung. Was die Messung weiter beitraegt, steht in `notes`:
// Beobachtungen, keine Eingriffe.
bool saveCurrentAsProfile(const Plan& p, const StripChain& chain);

} // namespace BroadcastOptimizer
} // namespace Longpath

// Carried through a Qt signal, and a QSignalSpy stores what it sees in
// a QVariant — which needs the type declared. Without this the spy
// records the emission but hands back an empty Plan, and a test that
// checks "did it ask with the SELECTED profile" would pass on a panel
// that always asked with the first one.
Q_DECLARE_METATYPE(Longpath::BroadcastOptimizer::Plan)
