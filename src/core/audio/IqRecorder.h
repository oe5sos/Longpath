#pragma once

// =================================================================
// src/core/audio/IqRecorder.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, im Verhalten an Thetis angelehnt
// (clsAudioRecordPlayback.cs, MW0LGE — AudioRecordRxSource.
// ReceiverInputIQ, "pre"-Abgriff vor der Demodulation; design doc
// docs/architecture/phase3m-recording-design.md §4, §7.2).
//
// Rohe I/Q-Abtastwerte aufzeichnen — VOR WDSP, nicht der demodulierte
// Ton. Baugleich zu WavRecorder (siehe dort fuer die ausfuehrliche
// Begruendung des streamenden Schreibers), nur mit anderer Quelle und
// anderem Vorgabe-Format.
//
// ── Warum float32 als Vorgabe (anders als WavRecorder) ──────────────
//
// WavRecorder nimmt PCM16 mit Dither als Vorgabe, weil Sprachfunk in
// 16 Bit locker Platz hat und Dither dort ein reales Klangproblem
// loest (korrelierter Rundungsfehler, hoerbar). Fuer rohe I/Q gilt
// beides nicht: die Aufnahme ist fuer spaetere Auswertung gedacht
// (schwache Signale, Rauschboden-Analyse), nicht zum Anhoeren, und der
// Dynamikumfang, den 16 Bit kappen wuerden, ist genau das, was eine
// I/Q-Aufnahme wertvoll macht. Also float32 als Vorgabe; PCM16 bleibt
// ueber setSaveFloat32(false) erreichbar, fuer wer bewusst Platz will.
//
// ── Was hier NICHT drin ist ──────────────────────────────────────────
//
// Das Anschalten des Abgriffs (IqRecorderController) und die
// Dateibenennung/UI (bewusst offen gelassen — design doc §8 und
// CLAUDE.local.md "Technik Nereus, Design ich").
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QDateTime>
#include <QString>

#include "core/audio/WavFile.h"

namespace Longpath {

// Was neben der Aufnahme steht. Gleiches Feldset wie
// WavRecordingInfo, absichtlich getrennt gehalten: die beiden
// Aufnahmearten (demodulierter Ton vs. rohes I/Q) sind unterschiedlich
// genug, dass eine gemeinsame Struktur spaeter nur Verwirrung stiftet,
// wenn eine der beiden ein Feld braucht, das die andere nicht hat.
struct IqRecordingInfo {
    QDateTime utcStart;
    QString   frequency;   // "14.205.000" — DDC-Mittenfrequenz
    QString   mode;        // "LSB"
    QString   band;        // "20m"
    int       sampleRate{0};
    double    seconds{0.0};
};

// Gegenstueck zu IqRecorder::stop(). Bei Fehlern kommt ein leerer
// Datensatz zurueck (utcStart ungueltig).
IqRecordingInfo readIqRecordingDescription(const QString& wavPath);

class IqRecorder {
public:
    void setSampleRate(int hz);
    int  sampleRate() const { return m_rate; }

    // Oeffnet die Datei sofort (streamend, wie WavRecorder).
    bool start(const QString& path, const IqRecordingInfo& info,
              QString* error = nullptr);

    // Schliesst die Datei und schreibt die JSON-Beschreibung daneben.
    void stop();

    bool isRecording() const { return m_writer.isOpen(); }

    // I und Q verschachtelt (I,Q,I,Q…), wie RadioConnection::iqDataReceived
    // sie liefert — auf [-1, +1] normalisiert, unveraendert weitergereicht.
    void feed(const float* interleavedIQ, int frames);

    double recordedSeconds() const;

    // Vorgabe true (anders als WavRecorder) — Begruendung im Header.
    void setSaveFloat32(bool on) { m_saveFloat32 = on; }
    bool saveFloat32() const { return m_saveFloat32; }

    const IqRecordingInfo& info() const { return m_info; }
    const QString&         path() const { return m_path; }

    // Fuer Tests: die genaue Rahmenzahl statt der daraus gerechneten
    // Sekunden.
    qint64 framesWritten() const { return m_writer.framesWritten(); }

private:
    WavStreamWriter  m_writer;
    IqRecordingInfo  m_info;
    QString          m_path;
    int              m_rate{48000};
    bool             m_saveFloat32{true};
};

} // namespace Longpath
