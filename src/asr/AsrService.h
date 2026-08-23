#pragma once

// =================================================================
// src/asr/AsrService.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Der Motor zwischen Ton und Text ─────────────────────────────────
//
// AetherSDR hat dafuer AsrEngine. Die haengt an SileroVad,
// SpeakerEmbedder und SpeakerClusterer — alle drei ueber onnxruntime,
// und die haben wir nicht. Diese Fassung macht dasselbe mit dem, was
// da ist:
//
//   Abgriff (AudioTapRing, 48 kHz Stereo)
//     -> auf 16 kHz Mono umtasten (core/Resampler)
//     -> AsrSegmenter (eingebaute Energieerkennung)
//     -> abgeschlossener Abschnitt an IAsrBackend, auf EIGENEM Faden
//     -> Text als Signal
//
// ── Warum ein eigener Faden ─────────────────────────────────────────
//
// Der HTTP-Weg zum Erkennungsdienst blockiert, bis die Antwort da
// ist — bei einem langen Sprechabschnitt sind das leicht ein paar
// Sekunden. Im Hauptfaden waere die Oberflaeche so lange starr. Der
// Faden hier tut nichts anderes als warten, und das darf er.
//
// ── Was NICHT hier ist ──────────────────────────────────────────────
//
// Sprechertrennung. Aether erkennt, WER spricht, und faerbt danach.
// Das braucht Einbettungsmodelle ueber onnxruntime. Wenn der Betreiber
// die eines Tages will, kommt es hinzu — die Schnittstelle IAsrBackend
// ist dafuer da, und der Zerleger bleibt unveraendert.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "asr/AsrSegmenter.h"
#include "asr/IAsrBackend.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class QThread;
class QTimer;

namespace Longpath {

class AudioTapRing;
class Resampler;

class AsrService : public QObject {
    Q_OBJECT

public:
    explicit AsrService(QObject* parent = nullptr);
    ~AsrService() override;

    /// Den Erkenner setzen. Uebernimmt den Besitz. Vor dem Start rufen.
    void setBackend(std::unique_ptr<IAsrBackend> backend);

    /// Die Quelle: ein Ring, den der Tonfaden fuellt, und seine Rate.
    void setSource(AudioTapRing* ring, int sourceRateHz);

    void start();
    void stop();
    bool isRunning() const { return m_running; }

    AsrSegmenter& segmenter() { return m_segmenter; }

    /// Nur fuer Pruefungen: einen Block direkt einspeisen, ohne Ring
    /// und ohne Zeitgeber.
    void feedForTest(const float* stereo, int frames, int rateHz);

signals:
    /// Ein Abschnitt wurde erkannt. `confidence` in [0,1].
    void transcript(const QString& text, float confidence);
    /// Der Erkenner hat sich beschwert. Einmal je Ursache, nicht je
    /// Abschnitt — sonst fuellt ein abgeschalteter Dienst das Protokoll.
    void failed(const QString& reason);
    void listeningChanged(bool listening);

private:
    void drainRing();
    void feedSamples(const float* stereo, int frames, int rateHz);
    void dispatch(std::vector<float>&& utterance);

    AsrSegmenter m_segmenter;
    std::unique_ptr<IAsrBackend> m_backend;
    std::unique_ptr<Resampler> m_resampler;
    int m_sourceRate{48000};

    AudioTapRing* m_ring{nullptr};
    QTimer* m_pump{nullptr};
    QThread* m_worker{nullptr};
    class AsrWorker* m_workerObj{nullptr};

    bool m_running{false};
    bool m_listening{false};
    QString m_lastFailure;
};

} // namespace Longpath
