#pragma once

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
// =================================================================

#include <QString>
#include <QVector>

namespace NereusSDR {

struct WavData {
    QVector<float> samples;      // einkanalig, -1..+1
    int            sampleRate{0};
    bool           ok{false};
};

// Liest eine WAV-Datei als einkanalige float-Folge.
// Bei Fehlern ist ok == false und *error (falls gegeben) gesetzt.
WavData readWavMono(const QString& path, QString* error = nullptr);

// Schreibt einkanaliges float32-WAV. Dasselbe Format, das
// TxAudioRecorder::saveWav erzeugt — wer eine Aufnahme von dort
// weiterreicht, bekommt hier keine Ueberraschung.
bool writeWavMono(const QString& path, const QVector<float>& samples,
                  int sampleRate, QString* error = nullptr);

// Dauer in Sekunden, ohne die Datei ganz zu lesen wenn moeglich.
// Gibt 0 zurueck, wenn die Datei nicht lesbar ist.
double wavDurationSeconds(const QString& path);

} // namespace NereusSDR
