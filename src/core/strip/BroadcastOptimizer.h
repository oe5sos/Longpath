#pragma once

// =================================================================
// src/core/strip/BroadcastOptimizer.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// One recording, four transmit bandwidths, four finished profiles.
//
// ── The job, in the operator's words ─────────────────────────────────
//
// "Speak for fifteen seconds with no settings as a baseline, have all
// of it analysed automatically and as carefully as possible to get the
// best possible profile. That can then be adjusted afterwards if
// wanted. The other profiles should fall out of it automatically."
//                                           — OE5SOS, 2026-10-09
//
// So: 2.3, 2.7, 3.5 and 4.0 kHz, each a complete chain, all four
// derived from the same measurement, none of them a preset somebody
// guessed in advance.
//
// ── Why the bandwidth changes the answer at all ──────────────────────
//
// It is tempting to think one good sound can be sent through any
// filter, and that a narrow channel simply gets less of it. That is
// wrong in both directions:
//
//   Too wide for the channel. Energy above the transmit filter does
//   not vanish politely. It reaches the compressor and the ALC first,
//   where it takes gain reduction with it — so a 4 kHz-shaped voice
//   sent through a 2.3 kHz filter is quieter AND duller than the same
//   voice shaped for 2.3 kHz. The operator pays twice for audio
//   nobody hears.
//
//   Too narrow for the channel. A 2.3 kHz shape in a 4 kHz channel
//   leaves the top one and a half octaves empty and the low end cut
//   for no reason. It sounds like a telephone on a band that had room
//   for a voice.
//
// The high-pass moves for the same reason, and it moves against the
// operator's own pitch rather than against a fixed number: the first
// harmonics of a 95 Hz voice and a 180 Hz voice sit an octave apart,
// so one high-pass cannot be right for both. That is what the new
// pitch measurement in VoiceAnalyzer is for.
//
// ── What this will not do ────────────────────────────────────────────
//
//   Boost. Same rule as VoiceAnalyzer::suggestEq and StripTuner: every
//   EQ move here is a cut, and the level comes back as make-up gain.
//   An optimiser that reaches a target by boosting is an optimiser
//   that amplifies whatever the microphone does worst.
//
//   Touch the live chain. plan() is a pure function of the
//   measurement; saveAsPresets() writes named presets and nothing
//   else. The chain only changes when the operator picks one and
//   apply() is called — which is the whole reason this can sit beside
//   the existing voice-check path instead of replacing it (operator,
//   2026-10-09: "build it parallel to our current audio setup and we
//   decide later, or we keep using the current one").
//
//   Switch the strip on. Nothing in core/strip/ ever does. The
//   operator decides when their transmit audio changes.
//
//   Promise. Every plan carries what it costs as well as what it
//   buys, and a plan that is not worth taking says so in `caveat`
//   rather than being quietly offered as an equal. A 4 kHz profile
//   built from a microphone that stops at 3 kHz is an empty promise,
//   and the measurement can see that.
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

// ── Density, the second axis ─────────────────────────────────────────
//
// Taken from Zeus, which is the benchmark here (operator, 2026-10-09:
// "Zeus with all its tools is surely the measure"). Zeus carries a
// single `TargetSpectralDensity` on every TX audio profile — an
// integer 0..100, default 55 (`Station.Engine.Hosting/
// TxFidelityPolicyStore.cs:DefaultTargetn`) — rather than exposing a
// ratio and a threshold. That is the better control and it is adopted
// wholesale: one number the operator can move, with the compressor
// derived from it instead of the other way round.
//
// The four named stops are Longpath's own placement on Zeus's scale.
// Zeus Link shows the same four words over its density row; the
// numbers behind them live in its client, which is not in the engine
// source here, so these are chosen to put Broadcast on Zeus's
// documented default and spread the rest evenly around it.
inline constexpr int kDensityNatural   = 40;   // Zeus shows 40
inline constexpr int kDensityBroadcast = 55;   // Zeus's own default
inline constexpr int kDensityDense     = 75;
inline constexpr int kDensityMaximum   = 92;

// The name for a density value — the nearest stop, so a hand-edited
// 58 still reads as "Broadcast" rather than as a bare number.
QString densityName(int density);

// What the compressor is aiming at: the crest factor the voice should
// still have AFTER processing, in dB. This is the whole density axis
// in one function, and it is exposed so it can be tested on numbers
// and drawn on a scale.
//
// At 0 the voice keeps its own dynamics. At 100 it is squeezed to
// about 6 dB, which is as far as speech goes before it stops sounding
// like a person. Nothing here clips.
double targetCrestDb(int density);

// Everything the optimiser decided for one width, and why.
//
// Kept as plain numbers rather than as a half-applied chain so the
// whole thing can be tested without a StripChain, drawn before it is
// applied, and shown side by side with its three siblings.
struct Plan {
    double  widthHz{2700.0};
    QString name;

    // ── The transmit filter the profile is FOR ───────────────────────
    //
    // Carried on the plan because Zeus carries it on the profile:
    // `TxAudioProfileDto` holds `LowCutHz` / `HighCutHz` alongside the
    // chain, so a named profile in Zeus is the whole answer — shaping
    // and channel together — not a shape that hopes the filter happens
    // to match. A "2.3 k profile" whose transmit filter is still at
    // 2.7 kHz is not a 2.3 k profile.
    //
    // These are reported, not applied: the transmit bandpass is WDSP's
    // and belongs to TransmitModel, which is deliberately out of reach
    // from core/strip/. apply() sets the chain; whoever offers the
    // plan to the operator is the one that may also move the filter,
    // visibly and on purpose.
    int     lowCutHz{100};
    int     highCutHz{2700};

    // Zeus's axis, 0..100. See kDensityBroadcast.
    int     density{kDensityBroadcast};

    // ── Dynamics, and nothing else ───────────────────────────────────
    //
    // No equaliser. Not a gentler one — none.
    //
    // This is the shape Zeus Link uses, read off its own panel on
    // 2026-10-09 while the operator sat at it. Each of its three
    // voice styles is three statements and not one of them is a
    // tone control:
    //
    //   NATURAL    Expander -40 dB (SNR 28 dB), trim +9.5 dB to
    //              -20 dBFS, AGC -22
    //   BROADCAST  Expander -38 dB (SNR 28 dB), trim +9.5 dB to
    //              -20 dBFS, wide to 2.9 kHz
    //   DX PUNCH   Expander -38 dB (SNR 28 dB), trim +9.5 dB to
    //              -20 dBFS, comp 4:1
    //
    // Expander threshold from the measured signal-to-noise, a level
    // trim onto a fixed target, and exactly ONE characteristic move.
    // The voice's own spectrum is left alone.
    //
    // The version before this one derived a high-pass, two shelves
    // and a presence bell per width, and the operator's verdict
    // after hearing it was "Das Original-Audio ist gut, nur
    // saemtliche Bearbeitungen sind eine Katastrophe". Every defect
    // found that day — a hum notch sitting on the fundamental, a
    // high-pass above it, two cuts stacked over the same octave, a
    // de-esser firing with nothing to de-ess — was a tone control
    // moving on evidence too thin to move it on.
    //
    // Here the width IS the characteristic move, which is exactly
    // what Zeus's BROADCAST does with "wide to 2.9 kHz".

    // Expander, not a gate: it leans on quiet passages instead of
    // slamming shut, and Zeus runs one in every style including the
    // gentlest. Threshold from the measured noise floor.
    bool    expanderOn{false};
    double  expanderThresholdDbFs{-45.0};

    // Wie tief der Expander zwischen den Woertern absenkt.
    //
    // Stand 2026-10-09 vom Betreiber: "die hintergrundgeraeusche
    // werden viel zu viel aufgenommen, es hallt vom raum, das sollte
    // generell bei mir weg kommen." Der erste Boden war -8 dB -- zu
    // zahm dafuer.
    //
    // Jetzt aus dem gemessenen Stoerabstand: je weiter der Raum unter
    // der Stimme liegt, desto tiefer darf abgesenkt werden, ohne dass
    // die Absenkung selbst hoerbar wird. Bei 34 dB Abstand sind das
    // 17 dB, und die Pausen werden still.
    //
    // Was das NICHT kann, und was ehrlich dazugehoert: Hall WAEHREND
    // des Sprechens. Ein Expander arbeitet am Pegel, und waehrend
    // gesprochen wird ist der oben -- die Raumfahne liegt dann unter
    // der Stimme und kommt mit durch. Dagegen hilft naeher ans
    // Mikrofon, eine Richtcharakteristik oder der Raum selbst, kein
    // Regler in dieser Kette.
    double  expanderFloorDb{-8.0};

    // ── Klang: wieder dabei, aber gedeckelt ──────────────────────────
    //
    // Nach dem Umbau auf Zeus' Form lag der Entzerrer ganz draussen.
    // Der Betreiber hat das am 2026-10-09 korrigiert, indem er auf
    // die Reiterreihe des Kanalzugs zeigte: "du siehst hier alles,
    // was die optimierung durchlaufen sollte." Die Optimierung soll
    // die ganze Kette durchlaufen, nicht nur deren Dynamik.
    //
    // Sie tut es jetzt wieder -- mit den drei Lehren desselben Tages
    // eingebaut, denn jeder Fehler davor sass genau hier:
    //
    //   KEIN Hochpass. Den macht der Kanalschnitt (50 bis 200 Hz je
    //   Breite). Zwei Stufen, die dieselbe Oktave beschneiden, waren
    //   die Haelfte von "hier ist kein Bass".
    //
    //   KEINE Brummkerben. Der Detektor haelt bei einer tiefen
    //   Stimme deren Grundton fuer Netzbrumm.
    //
    //   Eine VERSCHIEBUNG von der eigenen Balance aus, nicht ein Ziel,
    //   auf das gezogen wird. Das Spektrum einer Stimme faellt
    //   natuerlicherweise; das flachzubuegeln nimmt ihr den Koerper.
    //   Darum hoechstens wenige Dezibel, und das breite Profil
    //   bewegt unten gar nichts.
    double  lowShelfDb{0.0};
    double  lowShelfHz{250.0};
    double  presenceDb{0.0};
    double  highShelfDb{0.0};
    double  highShelfHz{2400.0};

    // The level trim, onto kTargetLevelDbFs. Shown as its own number
    // rather than hidden inside a make-up gain, because it is the
    // one change that is purely arithmetic and the operator should
    // be able to see it is not doing anything clever.
    double  trimDb{0.0};

    // De-esser, at the frequency the analysis found rather than at a
    // fixed one. Off when there is nothing to de-ess.
    bool    deEssOn{false};
    double  deEssHz{6000.0};
    double  deEssAmountDb{0.0};     // negative = reduction

    // Compressor. The ratio is solved for from the density target,
    // not chosen; see targetCrestDb().
    double  compRatio{1.0};
    double  compThresholdDbFs{-20.0};
    double  compMakeupDb{0.0};

    double  limiterCeilingDb{-1.0};

    // ── The phase rotator ────────────────────────────────────────────
    //
    // The one move that makes a voice stronger AND cleaner without
    // touching its tone, which is exactly what was asked for:
    // "meine stimme ... kräftiger, klarer und besser zu machen"
    // (operator, 2026-10-09).
    //
    // Speech is asymmetric — the waveform's positive and negative
    // peaks differ, often by several decibels — so the loudest peak
    // hits the limiter long before the average level has earned it.
    // A cascade of all-pass filters leaves the spectrum alone (every
    // frequency comes through at the same level) and only moves the
    // phases, which pulls the two peaks together. The headroom that
    // frees goes straight into average level.
    //
    // This is also what Zeus Link's own optimiser chose when it ran
    // on this operator's voice, and it reported the choice as
    // measured rather than assumed: "applied 1 bounded change: phase
    // rotator 474 Hz / 8 stages measured best".
    //
    // Longpath does not yet search for the best stage count per
    // voice — it takes the eight Zeus measured as best and says so.
    // Searching it needs the recording, which plan() does not have;
    // that is the next step, not a pretence here.
    int     phaseRotatorStages{8};

    // ── Die Farbstufen, als benannte Charaktere ──────────────────────
    //
    // "ziel ist es, broadcast oder voodoo zu haben. das maximale aus
    // allen sachen zu holen" (Betreiber, 2026-10-09).
    //
    // Beides gibt es im Haus bereits als Charakter: der Kompressor
    // kennt Gentle/Balanced/Contest/Voodoo, die Roehre
    // Clean/Warm/Full/Broadcast/Voodoo, der Begrenzer
    // Transparent/Safety/Broadcast/Loud. Ein Charakter ist ein Satz
    // zusammengehoeriger Werte mit einem Satz dazu, warum.
    //
    // Darum waehlt das Profil CHARAKTERE statt Zahlen. Was gemessen
    // werden kann -- Schwelle, Verhaeltnis, Trimm -- wird danach
    // ueberschrieben: der Charakter ist die Stimmung, die Messung die
    // Anpassung an diese Stimme. Eine Zahl, die ich fuer die Roehre
    // erfinde, waere beides nicht.
    QString compCharacter;
    QString tubeCharacter;      // leer = Stufe bleibt aus
    QString puduCharacter;      // leer = Stufe bleibt aus
    QString limiterCharacter;

    // ── The stages this profile deliberately forgets ─────────────────
    //
    // Operator, 2026-10-09: "for contest various filters have to be
    // forgotten; you'd use those from the 2.7 k profile upward." The
    // same stages NU9N's transmit-chain guide marks optional.
    bool    characterStages{true};   // tube, exciter, reverb
    QString forgotten;

    // ── What it is worth ─────────────────────────────────────────────
    //
    // An estimate, clearly labelled as one wherever it is shown. It is
    // the average-level gain the chain is expected to produce against
    // the unprocessed take: make-up plus the part of the crest factor
    // the compressor actually takes. It is NOT a measurement of the
    // result, and the only honest measurement of the result is the
    // next recording.
    double  densityGainDb{0.0};

    // ── Eine Zeile, nicht sieben ─────────────────────────────────────
    //
    // Was dieses Profil ist, in einem Satz. Betreiber, 2026-10-09:
    // "mich interessiert der ganze text nicht, es steht viel zu viel
    // in der beschreibung. kurz und knackig waere besser."
    //
    // Die Zahlen stehen ohnehin im Ablesegitter darueber; der
    // Fliesstext daneben war Doppelung mit Begruendung. Die
    // Begruendung ist nicht geloescht, sie ist in `notes` geblieben
    // und haengt als Hinweistext am Kasten -- einen Mauszeiger
    // entfernt statt dauerhaft im Weg.
    QString headline;

    // Die ausfuehrliche Fassung. Nicht mehr die Hauptanzeige.
    QStringList notes;

    // Empty when the plan is worth taking as it stands. Set when the
    // measurement says this width cannot deliver what its name claims
    // — a microphone that rolls off below the channel, a room too
    // noisy to open the top end. The plan is still produced, because
    // refusing to produce it would hide the reason.
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

// ── Putting one into the chain ───────────────────────────────────────
//
// Sets every stage the plan names and switches those stages on. Does
// NOT touch the master switch, and does not save anything. After this
// the operator can adjust from there, which is the point — the plan is
// a starting point that happens to be derived from their own voice
// rather than from somebody else's taste.
bool apply(const Plan& p, StripChain& chain);

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

// ── Writing all four out ─────────────────────────────────────────────
//
// Applies each plan to a scratch chain and saves it through
// StripSettings::saveUserPreset, so an optimiser profile and a
// hand-made one are the same kind of thing and appear in the same
// list. Returns how many were written.
//
// The caller's own chain is never touched: the four profiles exist to
// be chosen from, not to be imposed.
int saveAsPresets(const QVector<Plan>& plans);

} // namespace BroadcastOptimizer
} // namespace Longpath

// Carried through a Qt signal, and a QSignalSpy stores what it sees in
// a QVariant — which needs the type declared. Without this the spy
// records the emission but hands back an empty Plan, and a test that
// checks "did it ask with the SELECTED profile" would pass on a panel
// that always asked with the first one.
Q_DECLARE_METATYPE(Longpath::BroadcastOptimizer::Plan)
