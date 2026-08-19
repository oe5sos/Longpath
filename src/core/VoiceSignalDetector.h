#pragma once

// =================================================================
// src/core/VoiceSignalDetector.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/VoiceSignalDetector.h at 0cd4559
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// Dies ist die QUELLE des „S-Verlaufs" — der Teil, den die
// Merkmalsliste vom 2026-08-19 zunaechst fuer einen Anzeigestreifen
// gehalten hat. Er ist keiner: die erkannten Signale werden bei
// AetherSDR als Marken durch dieselbe Spot-Zeichnung gereicht, die wir
// seit 3J-2 haben. Der Aufwand liegt hier, im Erkennen.
//
// Reine Funktion ueber FFT-Werte: keine Zustandshaltung, kein Qt-Widget,
// kein Funkgeraet noetig. Genau deshalb ist sie zuerst dran — sie laesst
// sich am Schreibtisch pruefen, mit erfundenen Werten.
//
// NICHT portiert ist AetherSDRs SignalClassifier (ONNX-Runtime,
// 2-Klassen-Softmax Sprache gegen Traeger). Grund, gemessen statt
// vermutet: der Detektor ruft ihn nirgends auf (`grep classify(` in
// VoiceSignalDetector.cpp findet nichts), und im AetherSDR-Baum liegt
// keine einzige .onnx-Datei. Das Modell existiert dort also selbst
// nicht. Eine schwere neue Abhaengigkeit fuer eine Funktion zu holen,
// die auch beim Vorbild nichts tut, waere Aufwand ohne Wirkung.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; LogManager.h → core/LogCategories.h.
//                 Erkennungslogik, Konstanten und Kommentare unveraendert.
// =================================================================

#include <QVector>
#include <QString>
#include <QPair>

namespace NereusSDR {

// A voice-bandwidth SSB signal detected in a single FFT frame.
struct DetectedVoiceSignal {
    double  freqMhz;   // carrier edge frequency (left for USB, right for LSB)
    float   peakDbm;   // loudest bin within the region
    QString mode;      // "USB" or "LSB"
    double  widthHz;   // detected bandwidth (useful for QRM notch width)
};

// Returns true if a band-plan segment label designates a voice allocation
// (PHONE, SSB, USB, AM, FM/RPT).
bool isVoiceSegmentLabel(const QString& label);

// Scan FFT bins (in dBm) for contiguous regions ≥1.8 kHz wide and ≥6 dB above
// the stable noise floor.  Each detected region is capped at 2.7 kHz (one SSB
// channel); wider regions are split and the overflow is emitted as a second
// marker only if it also has a qualifying peak.
//
// voiceRangesMhz: if non-empty, only scan bins whose frequency falls inside one
// of these {lowMhz, highMhz} ranges (derived from the active band plan's voice
// segments).  Pass an empty vector to scan the full pan bandwidth.
//
// rollingNoiseFloorDbm: caller-supplied stable noise floor (e.g. EMA across
// recent frames).  When > -500 dBm it is used directly; otherwise the function
// falls back to the per-frame 10th-percentile estimate.
//
// sliceMode: "USB" or "LSB" from the active slice.  When supplied, it overrides
// the internal energy-asymmetry heuristic so markers always match the operator's
// current mode.  Pass an empty string to use the heuristic (e.g. for AM/FM pans).
//
// Returns one entry per detected signal (primary + optional secondary per region).
QVector<DetectedVoiceSignal> detectVoiceSignals(
    const QVector<float>& binsDbm,
    double centerMhz,
    double bandwidthMhz,
    const QVector<QPair<double, double>>& voiceRangesMhz = {},
    float rollingNoiseFloorDbm = -1000.0f,
    const QString& sliceMode = {});

// Format a peak-dBm value as an S-meter label, rounded UP to the next unit.
// Scale: S9 = -73 dBm, 6 dB/S-unit.  Examples: -85 → "S8", -63 → "S9+10".
//
// NereusSDR-Notiz zum Beispiel in der Zeile darueber: „-85 → S8" ist
// FALSCH, und zwar schon im Vorbild. Auf der genannten Skala liegt S8
// bei -79 dBm und S7 bei -85; der Code liefert richtig S7, nur sein
// eigenes Beispiel widerspricht ihm. Der Kommentar bleibt wortgleich
// stehen, weil er zum portierten Bestand gehoert — diese Notiz steht
// daneben, damit niemand die Skala nach dem Beispiel „korrigiert".
// tests/tst_voice_signal_detector.cpp haelt beide Faelle fest.
QString sLabel(float dbm);

} // namespace NereusSDR
