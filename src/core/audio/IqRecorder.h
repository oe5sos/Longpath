#pragma once

// Ported from Thetis sources:
//   Project Files/Source/Console/clsAudioRecordPlayback.cs, original licence
//   from Thetis source is included below
//
// Thetis v2.10.3.15 (@852bf0e). Diese Datei ist NereusSDR/Longpath-original
// und uebernimmt KEINEN C#-Code; sie leitet Verhalten und Feldauswahl aus
// der oben genannten Quelle ab. Der Kopf steht hier trotzdem vollstaendig,
// weil die Herkunftstabelle sie fuehrt und weil eine Nennung mehr niemandem
// schadet — eine fehlende dagegen schon.

/*  clsAudioRecordPlayback.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//


// no-port-check: Verhalten an Thetis clsAudioRecordPlayback.cs angelehnt (Feldauswahl der
// Beschreibung, RxSource-Abgriffspunkt); KEIN Zeilenport, kein C#-Code
// uebernommen. Herkunft steht als Zeile 'reference' in THETIS-PROVENANCE.md.

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
