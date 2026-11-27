// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port.
//
//   2026-11-27 — Neu angelegt ("Nachhoeren").
//
// Haelt fest, was der Kiwi waehrend einer eigenen Aussendung liefert, und
// gibt es nach dem Loslassen stueckweise wieder heraus. Der Ton vom Kiwi
// kommt um die Laufzeit verzoegert an; waehrend des Sprechens stoert er,
// danach hoert man sich in Ruhe.
//
// Reine Daten, keine Uhr und kein Tonweg: der Aufrufer bestimmt, wann
// begonnen, geschlossen und abgeholt wird.

#pragma once

#include <QByteArray>

namespace Longpath {

class KiwiReplayBuffer {
public:
    // Kiwi-Ton wie in KiwiSdrManager::decodedAudioReady: 24 kHz, stereo,
    // Float.
    static constexpr int kBytesPerMs = 24000 / 1000 * 2 * 4;
    // Laenger als eine Minute: es bleibt das Ende.
    static constexpr int kMaxMs = 60000;

    // Taste unten: alles Alte weg, mitschneiden.
    void begin();
    // Nimmt nur an, solange mitgeschnitten wird.
    void feed(const QByteArray& pcm24kStereoFloat);
    // Mitschnitt schliessen und zum Abspielen bereitlegen. skipLeadingMs ist
    // die Laufzeit: so viel vom Anfang liegt noch VOR der Sendung.
    void finish(int skipLeadingMs);
    // Naechstes Stueck, hoechstens ms lang. Leer: fertig.
    QByteArray nextChunk(int ms);
    void clear();

    bool isRecording() const { return m_recording; }
    bool isPlaying() const { return m_playing; }
    // Laenge dessen, was noch im Puffer liegt.
    int durationMs() const
    { return int((m_data.size() - m_readPos) / kBytesPerMs); }

private:
    QByteArray m_data;
    qsizetype m_readPos{0};
    bool m_recording{false};
    bool m_playing{false};
};

} // namespace Longpath
