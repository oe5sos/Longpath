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
// src/core/audio/WavRecorder.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, im Verhalten an Thetis angelehnt
// (clsAudioRecordPlayback.cs, MW0LGE — AudioRecordRxSource.
// ReceiverOutputAudio und die JSON-Beschreibung neben der WAV;
// design doc docs/architecture/phase3m-recording-design.md §3).
//
// "Off the air"-Aufnahme: nur was ankommt, keine eigene Stimme. Das
// ist der Unterschied zu QsoRecorder — dort gehoeren Empfang UND
// Mikrofon zusammen in eine Spur-synchrone Datei; hier gibt es nur
// eine Quelle, also keine Ausrichtungsfrage und kein RAM-Limit noetig.
//
// ── Warum streamend statt gesammelt (anders als QsoRecorder) ────────
//
// QsoRecorder sammelt im Speicher und deckelt bei 30 Minuten, weil ein
// QSO-Mitschnitt naturgemaess kurz ist. Eine "off the air"-Aufnahme
// dagegen kann ein ganzer Contest-Abend oder ein unbeaufsichtigter
// Zeitplan sein — ein Speicher-Deckel waere hier keine Sicherheits-
// grenze, sondern ein stiller Abbruch mitten in der Aufnahme. Also
// schreibt WavRecorder ueber WavStreamWriter direkt auf die Platte,
// Block fuer Block, ohne die ganze Aufnahme je im Speicher zu halten.
//
// ── Was hier NICHT drin ist ─────────────────────────────────────────
//
// Das Anschalten des Abgriffs (WavRecorderController) und die
// Dateibenennung/UI (bewusst offen gelassen — siehe design doc §8 und
// CLAUDE.local.md "Technik Nereus, Design ich"). Diese Klasse bekommt
// Abtastwerte gereicht und weiss nicht, woher.
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

// Was neben der Aufnahme steht. Feldauswahl aus Thetis
// RecordingDetails (clsAudioRecordPlayback.cs:88-108 [@852bf0e]),
// ohne die MP3- und Gegenstations-Felder — die gehoeren zu einem QSO,
// nicht zu einem allgemeinen Mitschnitt.
struct WavRecordingInfo {
    QDateTime utcStart;
    QString   frequency;   // "14.205.000"
    QString   mode;        // "LSB"
    QString   band;        // "20m"
    int       sampleRate{0};
    double    seconds{0.0};
};

// Die Beschreibung neben einer Aufnahme wieder einlesen. Gegenstueck
// zu WavRecorder::stop(), das sie schreibt. Bei Fehlern kommt ein
// leerer Datensatz zurueck (utcStart ungueltig) — eine Aufnahme ohne
// Beschreibung ist kein Fehler, nur aermer.
WavRecordingInfo readWavRecordingDescription(const QString& wavPath);

class WavRecorder {
public:
    void setSampleRate(int hz);
    int  sampleRate() const { return m_rate; }

    // Oeffnet die Datei sofort (streamend, siehe Header) und merkt
    // sich die Beschreibung fuer die JSON-Datei, die stop() schreibt.
    bool start(const QString& path, const WavRecordingInfo& info,
              QString* error = nullptr);

    // Schliesst die Datei (WAV-Kopf wird nachgetragen, siehe
    // WavStreamWriter::close()) und schreibt die JSON-Beschreibung
    // daneben.
    void stop();

    bool isRecording() const { return m_writer.isOpen(); }

    // Empfangston, verschachtelt Stereo, wie AudioEngine ihn fuehrt.
    // Wird unveraendert weitergereicht — anders als QsoRecorder, das
    // auf einen Kanal mischt: hier soll die Aufnahme genau das
    // enthalten, was auch am Lautsprecher ankaeme.
    void feed(const float* interleavedStereo, int frames);

    double recordedSeconds() const;

    // 16 Bit mit Dither, es sei denn jemand will ausdruecklich float32.
    // Vorgabe wie bei QsoRecorder, aus demselben Grund: eine Stunde
    // Stereo sind in float32 1,4 GB, in PCM16 noch 345 MB, und der
    // Dynamikumfang von Sprachfunk liegt weit unter dem, was 16 Bit
    // tragen.
    void setSaveFloat32(bool on) { m_saveFloat32 = on; }
    bool saveFloat32() const { return m_saveFloat32; }

    const WavRecordingInfo& info() const { return m_info; }
    const QString&          path() const { return m_path; }

    // Fuer Tests: die genaue Rahmenzahl statt der daraus gerechneten
    // Sekunden.
    qint64 framesWritten() const { return m_writer.framesWritten(); }

private:
    WavStreamWriter  m_writer;
    WavRecordingInfo m_info;
    QString          m_path;
    int              m_rate{48000};
    bool             m_saveFloat32{false};
};

} // namespace Longpath
