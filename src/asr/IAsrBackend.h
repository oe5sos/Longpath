#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/asr/IAsrBackend.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
//   2026-08-23 — Portiert. Namensraum angepasst, sonst zeichengetreu.
//
// ── Warum NUR dieser Teil von ASR ───────────────────────────────────
//
// AetherSDRs ASR-Zweig haengt an whisper.cpp, sherpa-onnx und
// onnxruntime — fremde Bibliotheken samt Modelldateien von mehreren
// hundert Megabyte. Das ist keine Portierung, das ist eine
// Bauentscheidung, und die gehoert dem Betreiber.
//
// Diese Dateien sind der Teil, der OHNE all das auskommt:
//   IAsrBackend, IVad  — reine Schnittstellen
//   AsrSegmenter       — zerlegt den Strom in Sprechabschnitte, mit
//                        eingebauter Energieerkennung (Silero ist
//                        optional und hier nicht dabei)
//   RemoteAsrBackend   — schickt einen Abschnitt per HTTP an einen
//                        Dienst und bekommt Text zurueck. Braucht nur
//                        Qt.
//   AsrTapPolicy       — welche Scheibe abgehoert wird
//
// Damit ist Spracherkennung vollstaendig benutzbar, sobald irgendwo
// ein Dienst laeuft — ein oertlicher whisper.cpp-Server genuegt. Die
// eingebauten Erkenner koennen spaeter dazukommen, ohne dass hier
// etwas umgebaut werden muss: IAsrBackend ist genau dafuer da.
//
#include <QString>

#include <vector>

// Backend-agnostic ASR inference interface (RFC #4333). AsrEngine drives this
// on its worker thread and never depends on a concrete engine. WhisperAsrBackend
// is the production implementor; tests inject a deterministic fake so the
// engine's threading/segmentation logic can be verified without a model.
//
// All calls happen on the worker thread; implementations need not be
// thread-safe.

namespace Longpath {

// One transcription result: the recognized text plus a confidence in [0, 1]
// (1 = most confident). Confidence drives the panel's color-coding, mirroring
// the CW decoder's cost-based coloring.
struct AsrTranscript {
    QString text;
    float confidence = 0.0f;
};

class IAsrBackend {
public:
    virtual ~IAsrBackend() = default;

    // Load a model from disk. Returns false and sets *error (if non-null) on
    // failure. May be called again to switch models.
    virtual bool load(const QString& modelPath, QString* error) = 0;

    virtual bool isLoaded() const = 0;

    // Transcribe one utterance of 16 kHz mono float samples in [-1, 1]. Returns
    // the recognized text + confidence (text empty for non-speech). Sets *error
    // on failure.
    virtual AsrTranscript transcribe(const std::vector<float>& pcm16k, QString* error) = 0;

    // Release the loaded model. Called before destruction; idempotent.
    virtual void unload() = 0;
};

} // namespace Longpath
