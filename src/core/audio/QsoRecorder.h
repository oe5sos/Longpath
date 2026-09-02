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
// Beschreibung, Stereo-Spurenaufteilung); KEIN Zeilenport, kein C#-Code
// uebernommen. Herkunft steht als Zeile 'reference' in THETIS-PROVENANCE.md.

// =================================================================
// src/core/audio/QsoRecorder.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, im Verhalten an Thetis angelehnt
// (clsAudioRecordPlayback.cs, MW0LGE — AudioRecordRxSource /
// AudioRecordTxSource und die JSON-Beschreibung neben der WAV).
//
// Ein QSO aufnehmen: was ankommt UND was man selbst sagt.
//
// Ansage des Betreibers (2026-08-19): „wichtig ist, dass man sich
// selbst aber auch die andere station hört!"
//
// ── Eine Datei, zwei Spuren ─────────────────────────────────────────
//
// Links die Gegenstation, rechts die eigene Stimme. Thetis schreibt je
// Quelle eine eigene Datei; eine gemeinsame Stereodatei haelt beide
// dagegen SYNCHRON, und genau darum geht es bei einem QSO — wer wann
// was gesagt hat. Zwei Dateien laufen beim Abspielen auseinander,
// sobald jemand eine davon schneidet.
//
// Wer die Spuren getrennt braucht, trennt sie in jedem Audioprogramm
// mit zwei Klicks. Wer sie synchron braucht, bekommt sie aus zwei
// Dateien nie wieder zusammen.
//
// ── Die eigentliche Schwierigkeit: die Ausrichtung ──────────────────
//
// Der Empfang laeuft dauernd, das Mikrofon nur beim Senden. Wer beide
// Spuren einfach hintereinander anhaengt und am Ende die kuerzere
// auffuellt, bekommt eine Aufnahme, in der die eigene Stimme GANZ AM
// ANFANG steht — egal wann sie gesprochen wurde.
//
// Deshalb ist der EMPFANG DIE UHR: kommt Mikrofonton, wird die
// Sprechspur vorher mit Stille bis zum aktuellen Empfangsstand
// aufgefuellt. Dann liegt die eigene Stimme dort, wo sie hingehoert.
//
// ── Was hier NICHT drin ist ─────────────────────────────────────────
//
// Das Anschalten der Abgriffe. Diese Klasse bekommt Abtastwerte
// gereicht und weiss nicht, woher — so laesst sie sich ohne Funkgeraet,
// ohne Audiogeraet und ohne WDSP pruefen.
//
// ── Zeitlich unbegrenzt (2026-09-02) ─────────────────────────────────
//
// Bis hierhin sammelte diese Klasse beide Spuren im Arbeitsspeicher und
// schrieb die Datei erst bei stop(), gedeckelt bei kMaxMinutes (30) —
// der Deckel war der Schutz gegen eine vergessene Aufnahme, die den
// Speicher auffrisst. Ansage des Betreibers (2026-09-02): der Recorder
// soll RX UND TX aufnehmen (das tat er schon) und zeitlich unbegrenzt
// laufen. Ein Deckel und "unbegrenzt" schliessen sich aus, wenn man
// weiter im Speicher sammelt.
//
// Also schreibt diese Klasse jetzt wie WavRecorder (WavRecorder.h,
// 2026-08-25 fuer genau dasselbe Problem gebaut) blockweise direkt auf
// die Platte, ueber denselben WavStreamWriter — der Speicherbedarf
// bleibt damit UNABHAENGIG von der Aufnahmedauer. Was bleibt, ist ein
// kleiner Zwischenspeicher je Spur (typisch eine Abholrunde, siehe
// QsoRecorderController::kDrainMs), der die RX-ist-die-Uhr-Ausrichtung
// aus dem Header oben umsetzt, bevor ein Block feststeht und auf die
// Platte geht. Der Schutz gegen "vergessen laufen lassen" ist jetzt
// QsoRecorderController's Speicherplatz-Wache (Thetis OkToRecord /
// onRecordSpaceTimer, clsAudioRecordPlayback.cs:630-692 [@852bf0e]) —
// prozentual zur Plattengroesse, nicht an eine Dauer gebunden.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-09-02 — Streamend statt gesammelt (zeitlich unbegrenzt), von
//                 Martin Fischer, KI-gestuetzt ueber Anthropic Claude
//                 (Cowork). Begruendung oben.
// =================================================================

#include <QDateTime>
#include <QString>
#include <QVector>

#include "core/audio/WavFile.h"

namespace Longpath {

// Was neben der Aufnahme steht. Feldauswahl aus Thetis
// RecordingDetails (clsAudioRecordPlayback.cs:88-108) — ohne die
// MP3-Felder, weil wir kein MP3 schreiben.
struct QsoRecordingInfo {
    QDateTime utcStart;
    QString   frequency;   // „14.205.000"
    QString   mode;        // „LSB"
    QString   band;        // „20m"
    QString   callsign;    // Gegenstation, falls bekannt
    int       sampleRate{0};
    double    seconds{0.0};
};

// Die Beschreibung neben einer Aufnahme wieder einlesen.
//
// Aus der Thetis-Durchsicht vom 2026-08-19 (`GetJSONDetailsFromFile`,
// clsAudioRecordPlayback.cs:1102 [@852bf0e]): Thetis liest
// seine Beschreibungen zurueck, wir haben sie bisher nur geschrieben.
// Der Unterschied steht in der Liste der Aufnahmen — „19.08. 18:02 ·
// DL1ABC · 14.205.000 · LSB" statt „qso-20260819-180213".
//
// `wavPath` darf auf die WAV oder direkt auf die JSON zeigen. Bei
// Fehlern kommt ein leerer Datensatz zurueck (utcStart ungueltig); eine
// Aufnahme ohne Beschreibung ist kein Fehler, nur aermer.
QsoRecordingInfo readQsoDescription(const QString& wavPath);

class QsoRecorder
{
public:
    void setSampleRate(int hz);
    int  sampleRate() const { return m_rate; }

    // Oeffnet die Datei sofort (streamend, siehe Header) und merkt sich
    // die Beschreibung fuer die JSON-Datei, die stop() schreibt.
    bool start(const QString& wavPath, const QsoRecordingInfo& info,
              QString* error = nullptr);
    // Traegt die Sprechspur auf den letzten Empfangsstand nach (siehe
    // Header, "die eigentliche Schwierigkeit"), schreibt den Rest,
    // schliesst die Datei und legt die JSON-Beschreibung daneben. Kam
    // nichts an, wird die leere Datei wieder geloescht statt eine
    // Aufnahme ohne Ton zu hinterlassen.
    void stop();
    bool isRecording() const { return m_recording; }

    // Empfangston, verschachtelt Stereo, wie AudioEngine ihn fuehrt.
    // Wird auf einen Kanal gemischt: eine Aufnahme des QSOs ist keine
    // Musikproduktion, und die Haelfte des Platzes ist es wert.
    void feedRx(const float* interleavedStereo, int frames);

    // Mikrofonton, einkanalig, wie der Sendeabgriff ihn liefert.
    void feedTx(const float* mono, int frames);

    // Seit Aufnahmebeginn insgesamt hereingekommen — nicht die Groesse
    // des kleinen Zwischenspeichers, der laufend auf die Platte geht.
    qint64 rxFrames() const { return m_rxFedTotal; }
    qint64 txFrames() const { return m_txFedTotal; }
    double recordedSeconds() const;

    // Fuer den Fall, dass jemand die Aufnahme weiterverarbeiten will
    // und keine Rundung im Weg haben moechte. Vorgabe ist 16 Bit — der
    // Dynamikumfang eines Sprach-QSOs liegt weit unter dem, was 16 Bit
    // tragen, und halbiert nebenbei die Dateigrosse. Vor start() setzen;
    // eine laufende Aufnahme wechselt das Format nicht mehr.
    void setSaveFloat32(bool on) { m_saveFloat32 = on; }
    bool saveFloat32() const { return m_saveFloat32; }

    void clear();

    const QsoRecordingInfo& info() const { return m_info; }
    const QString&          path() const { return m_path; }

private:
    void flushAligned();
    void finalizeTail();

    // Kleiner Zwischenspeicher: noch nicht auf die Platte geschrieben,
    // weil die Gegenspur noch nicht so weit ist (siehe Header). Bleibt
    // in der Praxis bei rund einer Abholrunde — siehe QsoRecorder.cpp.
    QVector<float>   m_rxPending;
    QVector<float>   m_txPending;
    qint64           m_rxFedTotal{0};
    qint64           m_txFedTotal{0};

    WavStreamWriter  m_writer;
    QsoRecordingInfo m_info;
    QString          m_path;
    int              m_rate{48000};
    bool             m_recording{false};
    bool             m_saveFloat32{false};
};

} // namespace Longpath
