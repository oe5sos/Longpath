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


// no-port-check: Nennt Thetis clsAudioRecordPlayback.cs (MW0LGE) als Vorbild fuer Umfang
// und Beschreibungsdatei; der WAV-Leser und -Schreiber sind
// NereusSDR-original. Siehe THETIS-PROVENANCE.md, Art 'reference'.

// =================================================================
// src/core/audio/WavFile.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// WAV lesen und schreiben, einkanalig, als float.
//
// WOFUER: der Sprachspeicher (10 Ansagen, aufnehmen und senden) und
// die QSO-Aufnahme brauchen beides. Thetis fuehrt dafuer
// clsAudioRecordPlayback.cs (4042 Zeilen, MW0LGE) mit WAV, MP3 und
// JSON-Beschreibung; hier steht nur der Teil, den wir zuerst brauchen.
//
// WARUM NOCH EIN PARSER, und das ist eine Schuld, keine Entscheidung:
// src/core/strip/TargetFromFile.cpp liest bereits WAV — aber es gibt
// ein LTAS-Spektrum zurueck, keine Abtastwerte, und es sitzt mitten im
// Sprachbearbeitungs-Pfad. Ihn spaet am Tag umzubauen hiesse, einen
// laufenden DSP-Weg fuer eine Bequemlichkeit anzufassen. Die beiden
// gehoeren zusammengelegt, sobald jemand Zeit dafuer hat; bis dahin
// steht dieser Hinweis hier, damit die Doppelung nicht unbemerkt
// bleibt.
//
// WAS GELESEN WIRD: PCM 8/16/24/32 und IEEE-float32, ein oder zwei
// Kanaele. Zwei Kanaele werden gemittelt — ein Sprachspeicher ist
// einkanalig, und die Alternative (den linken nehmen) verliert die
// Haelfte des Signals, wenn jemand rechts eingesprochen hat.
//
// WAVE_FORMAT_EXTENSIBLE wird aufgeloest: dort steht das eigentliche
// Format in einer GUID, und ein Leser, der nur auf formatTag schaut,
// haelt eine gewoehnliche 24-Bit-Aufnahme fuer unbekannt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-08-25 — WavStreamWriter ergaenzt (Phase 3M Recording): die
//                 obigen writeWav*-Funktionen brauchen die ganze
//                 Aufnahme im Speicher, bevor sie schreiben koennen.
//                 Eine "off the air"-Aufnahme soll aber laufen
//                 duerfen, ohne vorher zu wissen wie lange — daher
//                 ein Schreiber, der Block fuer Block direkt auf die
//                 Platte schreibt und den WAV-Kopf beim Schliessen
//                 nachtraegt. Von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude (Cowork).
// =================================================================

#include <QFile>
#include <QString>
#include <QVector>

#include <random>

namespace Longpath {

struct WavData {
    QVector<float> samples;      // einkanalig, -1..+1
    int            sampleRate{0};
    bool           ok{false};
};

// Liest eine WAV-Datei als einkanalige float-Folge.
// Bei Fehlern ist ok == false und *error (falls gegeben) gesetzt.
WavData readWavMono(const QString& path, QString* error = nullptr);

// Zweikanalig, verschachtelt (L,R,L,R…), OHNE zu mitteln — Gegenstueck
// zu writeWavStereo(). readWavMono() waere hier falsch: es mittelt die
// Kanaele zusammen, und das zerstoert das Signal, wenn die beiden
// Kanaele UNTERSCHIEDLICHE Dinge tragen statt derselben Stimme von
// zwei Mikrofonen — z.B. rohes I/Q (IqRecorder, Phase 3M-B: links = I,
// rechts = Q) oder die QSO-Aufnahme (links = Gehoertes, rechts =
// Gesagtes). Eine einkanalige Datei wird verdoppelt (L=R) zurueckgegeben,
// damit ein Aufrufer nicht zwischen ein- und zweikanalig unterscheiden
// muss.
struct WavStereoData {
    QVector<float> interleaved;  // L,R,L,R… (oder I,Q,I,Q…), -1..+1
    int            sampleRate{0};
    bool           ok{false};
};
WavStereoData readWavStereo(const QString& path, QString* error = nullptr);

// Schreibt einkanaliges float32-WAV. Dasselbe Format, das
// TxAudioRecorder::saveWav erzeugt — wer eine Aufnahme von dort
// weiterreicht, bekommt hier keine Ueberraschung.
bool writeWavMono(const QString& path, const QVector<float>& samples,
                  int sampleRate, QString* error = nullptr);

// Zweikanalig, verschachtelt (L,R,L,R …). Fuer die QSO-Aufnahme:
// links, was ankam, rechts, was man selbst gesagt hat.
bool writeWavStereo(const QString& path, const QVector<float>& interleaved,
                    int sampleRate, QString* error = nullptr);

// Dasselbe als 16-Bit-PCM, mit Dither.
//
// AUS DER THETIS-DURCHSICHT vom 2026-08-19: Thetis kann float32, PCM
// 32 / 24 / 16 / 8 und hat einen eigenen Schalter `DitherEnabled`
// (clsAudioRecordPlayback.cs:66 + :215 [@852bf0e]).
//
// WARUM 16 BIT FUER EIN QSO REICHT: eine halbe Stunde Stereo in float32
// sind 690 MB, als PCM16 noch 173 MB. Der Dynamikumfang eines
// Sprach-QSOs liegt bei vielleicht 50 dB; 16 Bit tragen 96.
//
// WARUM DITHER NICHT WEGGELASSEN WIRD: beim Kuerzen auf 16 Bit wird der
// Rundungsfehler mit dem Signal KORRELIERT — bei leisen Stellen hoert
// man das als Rasseln, das mit dem Ton mitgeht, nicht als Rauschen.
// Ein halbes Bit Zufall vorher macht daraus gleichmaessiges Rauschen
// weit unter der Hoergrenze. Dreieckverteilt (zwei Zufallszahlen
// addiert), weil gleichverteilter Dither die Modulation nur
// halbherzig aufloest.
bool writeWavStereo16(const QString& path, const QVector<float>& interleaved,
                      int sampleRate, bool dither = true,
                      QString* error = nullptr);

// Dauer in Sekunden, ohne die Datei ganz zu lesen wenn moeglich.
// Gibt 0 zurueck, wenn die Datei nicht lesbar ist.
double wavDurationSeconds(const QString& path);

// Zweikanaliges WAV, Block fuer Block auf die Platte geschrieben statt
// im Speicher gesammelt. Fuer Aufnahmen ohne bekannte Laenge im
// Voraus — siehe Begruendung oben in der Aenderungsgeschichte.
//
// Ablauf: open(), dann writeInterleaved() beliebig oft, dann close().
// Der RIFF- und der data-Blockkopf tragen beim Schreiben noch die
// Laenge 0; close() traegt die wahren Groessen nach. Bricht das
// Programm vorher ab, bleibt eine WAV-Datei mit Laenge 0 im Kopf
// stehen — von den meisten Abspielprogrammen dennoch lesbar, weil sie
// selbst bis zum Dateiende lesen, aber technisch nicht spezifikations-
// treu. Das ist der Preis fuer "kann beliebig lange laufen"; wer eine
// robustere Loesung braucht, muss den Kopf periodisch nachtragen.
class WavStreamWriter {
public:
    enum class Format {
        Float32Stereo,  // 32-Bit IEEE-Float, verschachtelt, wie writeWavStereo()
        Pcm16Stereo,    // 16-Bit PCM mit Dither, wie writeWavStereo16()
    };

    WavStreamWriter() = default;
    ~WavStreamWriter();

    // Nicht kopierbar — der Dateizugriff gehoert genau einer Stelle.
    WavStreamWriter(const WavStreamWriter&) = delete;
    WavStreamWriter& operator=(const WavStreamWriter&) = delete;

    bool open(const QString& path, int sampleRate, Format format,
             bool dither = true, QString* error = nullptr);

    // `interleaved` hat frames*2 Werte (L,R,L,R…). Gibt false zurueck,
    // wenn die Datei nicht offen ist oder das Schreiben fehlschlaegt —
    // der Aufrufer soll das als Aufnahmeabbruch behandeln, nicht als
    // stillen Datenverlust.
    bool writeInterleaved(const float* interleaved, int frames);

    // Traegt RIFF- und data-Groesse nach und schliesst die Datei.
    // Gefahrlos mehrfach aufzurufen; nach dem ersten Mal ist isOpen()
    // false und weitere Aufrufe tun nichts.
    void close();

    bool  isOpen() const { return m_file.isOpen(); }
    qint64 framesWritten() const { return m_framesWritten; }

private:
    QFile          m_file;
    Format         m_format{Format::Pcm16Stereo};
    int            m_sampleRate{0};
    bool           m_dither{true};
    qint64         m_dataBytes{0};
    qint64         m_framesWritten{0};
    std::mt19937   m_rng;
};

} // namespace Longpath
