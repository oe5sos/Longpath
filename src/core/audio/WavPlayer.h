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


// no-port-check: Nennt Thetis PlayFileViaPCAudio (clsAudioRecordPlayback.cs) als Vorbild
// fuer den zweiten Wiedergabeweg; NereusSDR-original. Siehe
// THETIS-PROVENANCE.md, Art 'reference'.

// =================================================================
// src/core/audio/WavPlayer.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Eine WAV-Datei ueber die Lautsprecher anhoeren.
//
// WOFUER: Nachhoeren gehoert zum Aufnehmen. Aus der Thetis-Durchsicht
// vom 2026-08-19 — Thetis hat zwei Wege, eine Datei abzuspielen:
// `PlayFileViaWDSP` (in den Sender, mit Tastung) und
// `PlayFileViaPCAudio` (nur an die Lautsprecher, zum Kontrollhoeren).
// Dies ist der zweite. Er braucht kein Funkgeraet und taste nichts.
//
// clsAudioRecordPlayback.cs:1862 [v2.10.3.15-5-g852bf0e]
//
// ── Warum noch ein Abspieler, und das ist eine Schuld ───────────────
//
// TxVoiceCheckDialog.cpp:632-658 macht dasselbe bereits inline: Format
// bauen, QBuffer, QAudioSink, bei IdleState aufhoeren. Es spielt aber
// EINKANALIG AUS DEM SPEICHER, nicht aus einer Datei, und es sitzt
// mitten in einem Dialog, der funktioniert. Ihn spaet nachts
// umzubauen, um sich eine Klasse zu sparen, ist der falsche Handel.
//
// Die beiden gehoeren zusammengelegt, sobald jemand Zeit dafuer hat;
// bis dahin steht dieser Hinweis hier, damit die Doppelung nicht
// unbemerkt bleibt. Dieselbe Schuld ist schon bei WavFile.h
// vermerkt (TargetFromFile.cpp liest ebenfalls WAV).
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Kein WDSP, kein Sender, keine Tastung. Der Ton geht an das
// Standard-Ausgabegeraet und sonst nirgendwohin.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QByteArray>
#include <QObject>
#include <QString>

class QAudioSink;
class QBuffer;

namespace Longpath {

class WavPlayer : public QObject
{
    Q_OBJECT

public:
    explicit WavPlayer(QObject* parent = nullptr);
    ~WavPlayer() override;

    // Spielt die Datei von vorn. Ein laufendes Stueck wird abgebrochen.
    // Gibt false zurueck, wenn die Datei nicht lesbar ist oder das
    // Ausgabegeraet das Format ablehnt; *error traegt dann den Grund.
    bool play(const QString& wavPath, QString* error = nullptr);

    void stop();
    bool isPlaying() const { return m_sink != nullptr; }

    // Was gerade laeuft — fuer eine Anzeige, die den Namen zeigen will.
    QString currentPath() const { return m_path; }

signals:
    void playingChanged(bool on);
    void finished();

private:
    void teardown();

    QAudioSink* m_sink{nullptr};
    QBuffer*    m_buffer{nullptr};
    QByteArray  m_pcm;
    QString     m_path;
};

} // namespace Longpath
