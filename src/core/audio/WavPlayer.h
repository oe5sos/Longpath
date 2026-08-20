#pragma once

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

namespace NereusSDR {

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

} // namespace NereusSDR
