#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/asr/AsrSegmenter.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (Longpath):
//   2026-08-23 — Portiert. Namensraum angepasst, sonst zeichengetreu.
//   2026-11-11 — Schwelle am Rauschboden (noiseFactor), Longpath-eigen;
//                Begruendung bei Config::noiseFactor. Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude.
//
#include <string>
#include <vector>

// Energy-based voice-activity segmenter for the ASR pipeline (RFC #4333).
//
// Whisper is a chunk model, not a streaming one, and running it on dead-carrier
// noise wastes cycles. This class turns a continuous 16 kHz mono stream into
// discrete speech utterances ("overs"): it opens a segment when short-term
// energy rises above a threshold and closes it after a hangover of silence (or
// at a hard maximum length), dropping segments too short to be speech. The
// closed segment's samples are then handed to the ASR backend.
//
// Deliberately whisper-free and allocation-simple so it can be unit-tested
// offline with synthetic audio and reused regardless of backend.

namespace Longpath {

class IVad;

class AsrSegmenter {
public:
    struct Config {
        int sampleRate = 16000;      // whisper's required rate
        int frameMs = 10;            // energy is evaluated per frame
        float speechRms = 0.010f;    // RMS above this = speech frame
        int minSpeechMs = 200;       // discard utterances shorter than this
        int hangoverMs = 300;        // trailing silence that closes a segment
        int maxSegmentMs = 20000;    // force-close cap (a very long over)

        // ── Schwelle am Rauschboden (Longpath, 2026-11-11) ─────────
        //
        // Gemessen an einem Pileup-Mitschnitt (NT6X, CQ WW SSB): der
        // Pegel liegt dort im Median bei 0,084, also nie unter der
        // festen Schwelle 0,010. 19 von 22 Abschnitten liefen bis zum
        // 20-s-Deckel und wurden mitten im Rufzeichen abgeschnitten,
        // weil auf einem belebten Band nie "Stille" ist, nur leiser.
        //
        // Ist noiseFactor > 0, gilt ein Rahmen erst dann als Sprache,
        // wenn er ueber noiseFactor x Rauschboden liegt (und weiterhin
        // ueber speechRms). Der Rauschboden ist das 20-%-Perzentil der
        // Rahmenpegel der letzten noiseWindowMs. Mit Faktor 2 schnitt
        // derselbe Mitschnitt 79 Abschnitte, Median 4,2 s, keiner am
        // Deckel — so lang wie ein Durchgang.
        //
        // 0 = aus: dann entscheidet speechRms allein, wie bei AetherSDR.
        float noiseFactor = 0.0f;
        int noiseWindowMs = 30000;
        // Optional Silero VAD model (.onnx). Empty = built-in energy VAD. The
        // worker (not the segmenter) consumes this to build the detector; carried
        // here because Config is the worker's construction bundle.
        std::string vadModelPath;
        // The speaker-embedding model (.onnx) is deliberately NOT carried here:
        // it is loaded at runtime via AsrEngine::setSpeakerModelPath() so a
        // labeling change never has to rebuild the engine (#4737). Only the
        // clustering threshold, which the worker's clusterer needs at
        // construction, stays in this bundle.
        float speakerThreshold = 0.50f; // cosine threshold for the same speaker
    };

    AsrSegmenter() : AsrSegmenter(Config{}) {}
    explicit AsrSegmenter(const Config& config);

    // Feed mono float samples in [-1, 1]. Returns any utterances that closed as
    // a result of this input (each a contiguous sample buffer). Usually empty.
    std::vector<std::vector<float>> feed(const float* samples, int count);

    // Close any in-progress utterance (end of stream / mode change). Returns it
    // if it qualifies as speech, otherwise empty.
    std::vector<std::vector<float>> flush();

    // Discard all buffered state without emitting.
    void reset();

    // Plug in a voice-activity detector (non-owning). When set, its isSpeech()
    // replaces the built-in energy threshold for the speech/silence decision; the
    // utterance state machine (hangover, min/max) is unchanged. Null = energy VAD.
    void setVad(IVad* vad) { m_vad = vad; }

    // Runtime-adjustable tuning (call on the segmenter's own thread):
    //  - maxSegmentMs: hard force-close cap ("decode buffer" size); a long over
    //    is force-decoded at this length even without a silence gap.
    //  - speechRms: energy threshold above which a frame counts as speech —
    //    lower = more sensitive VAD (picks up fainter/weaker signals).
    //  - hangoverMs: trailing silence that closes an utterance.
    void setMaxSegmentMs(int ms);
    void setSpeechRms(float rms);
    void setHangoverMs(int ms);
    int maxSegmentMs() const { return m_config.maxSegmentMs; }
    float speechRms() const { return m_config.speechRms; }
    int hangoverMs() const { return m_config.hangoverMs; }

    // Schwelle am Rauschboden (siehe Config::noiseFactor). 0 schaltet ab.
    void setNoiseFactor(float factor) { m_config.noiseFactor = factor; }
    float noiseFactor() const { return m_config.noiseFactor; }
    // Der zuletzt geschaetzte Rauschboden (RMS); 0, solange noch keine
    // Sekunde Ton gesehen wurde oder die Schaetzung abgeschaltet ist.
    float noiseFloor() const { return m_noiseFloor; }
    // Die Schwelle, die gerade gilt: max(speechRms, noiseFactor x Boden).
    float effectiveThreshold() const;

    bool inSpeech() const { return m_inSpeech; }

private:
    int framesToSamples(int ms) const;
    void closeSegment(std::vector<std::vector<float>>& out);
    // Die Zustandsmaschine fuer EINEN Rahmen (unveraendert aus dem Port,
    // nur herausgezogen, damit der Anlauf sie nachspielen kann).
    void processFrame(const float* frame, int n, bool speechFrame,
                      std::vector<std::vector<float>>& out);
    void trackNoiseFloor(float frameRms);
    void replayStartup(std::vector<std::vector<float>>& out);

    Config m_config;
    int m_frameSamples = 160;
    int m_minSpeechSamples = 0;
    int m_hangoverSamples = 0;
    int m_maxSegmentSamples = 0;

    IVad* m_vad = nullptr;          // optional learned VAD; null = energy threshold
    std::vector<float> m_frame;     // accumulates one frame for RMS
    std::vector<float> m_segment;   // the current utterance
    bool m_inSpeech = false;
    int m_trailingSilence = 0;      // samples of silence since last speech frame
    int m_speechSamples = 0;        // speech-only samples (excludes hangover), gates minSpeech

    // Rauschboden: Ring der Rahmenpegel ueber noiseWindowMs.
    std::vector<float> m_rmsHistory;
    int m_rmsHead = 0;
    int m_rmsCount = 0;
    int m_framesSinceEstimate = 0;
    bool m_floorEstimated = false;
    float m_noiseFloor = 0.0f;
    // Die erste Sekunde, aufgehoben bis der Boden bekannt ist.
    std::vector<float> m_startup;
};

} // namespace Longpath
