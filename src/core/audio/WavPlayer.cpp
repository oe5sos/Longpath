// =================================================================
// src/core/audio/WavPlayer.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung und die vermerkte Doppelung stehen
// im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/WavPlayer.h"

#include "core/audio/WavFile.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QMediaDevices>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace Longpath {

WavPlayer::WavPlayer(QObject* parent) : QObject(parent) {}

WavPlayer::~WavPlayer() { teardown(); }

bool WavPlayer::play(const QString& wavPath, QString* error)
{
    stop();

    // Ueber readWavMono: der Abspieler ist zum KONTROLLHOEREN da, und
    // dafuer will man beide Spuren zugleich hoeren — die Gegenstation
    // und sich selbst. Getrennt anhoeren kann man sie in jedem
    // Audioprogramm; hier geht es darum, ob das QSO drauf ist.
    QString err;
    const WavData data = readWavMono(wavPath, &err);
    if (!data.ok) {
        if (error) { *error = err; }
        return false;
    }
    if (data.samples.isEmpty()) {
        if (error) { *error = QStringLiteral("the file holds no audio"); }
        return false;
    }

    // Int16, weil jedes Ausgabegeraet das nimmt. Float32 lehnen manche
    // ab, und ein Abspieler, der auf halben Geraeten schweigt, ist
    // schlimmer als einer, der ein Bit Genauigkeit kostet.
    m_pcm.resize(data.samples.size() * 2);
    for (int i = 0; i < data.samples.size(); ++i) {
        const int v = static_cast<int>(
            std::lround(data.samples[i] * 32767.0f));
        const qint16 s16 = static_cast<qint16>(std::clamp(v, -32768, 32767));
        qToLittleEndian(s16, reinterpret_cast<uchar*>(m_pcm.data()) + i * 2);
    }

    QAudioFormat fmt;
    fmt.setSampleRate(data.sampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(fmt)) {
        if (error) {
            *error = QStringLiteral(
                "the output device refused %1 Hz mono").arg(data.sampleRate);
        }
        return false;
    }

    m_buffer = new QBuffer(&m_pcm, this);
    m_buffer->open(QIODevice::ReadOnly);
    m_sink = new QAudioSink(dev, fmt, this);
    m_path = wavPath;

    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State s) {
        // IdleState heisst „durchgelaufen", nicht „Fehler". Beide enden
        // hier gleich: aufraeumen und Bescheid sagen.
        if (s == QAudio::IdleState || s == QAudio::StoppedState) {
            const bool was = m_sink != nullptr;
            teardown();
            if (was) { emit playingChanged(false); emit finished(); }
        }
    });

    m_sink->start(m_buffer);
    emit playingChanged(true);
    return true;
}

void WavPlayer::stop()
{
    if (!m_sink) { return; }
    teardown();
    emit playingChanged(false);
}

void WavPlayer::teardown()
{
    // Reihenfolge: erst die Meldungen abklemmen, dann anhalten. Sonst
    // ruft stop() den eigenen stateChanged-Empfaenger auf, waehrend
    // dieser gerade aufraeumt.
    if (m_sink) {
        QObject::disconnect(m_sink, nullptr, this, nullptr);
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    if (m_buffer) {
        m_buffer->close();
        m_buffer->deleteLater();
        m_buffer = nullptr;
    }
    m_path.clear();
}

} // namespace Longpath
