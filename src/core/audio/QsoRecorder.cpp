// =================================================================
// src/core/audio/QsoRecorder.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Aufbau und Begruendungen stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/QsoRecorder.h"

#include "core/audio/WavFile.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace NereusSDR {

void QsoRecorder::setSampleRate(int hz)
{
    if (hz > 0) { m_rate = hz; }
}

void QsoRecorder::start(const QsoRecordingInfo& info)
{
    clear();
    m_info = info;
    m_info.sampleRate = m_rate;
    if (!m_info.utcStart.isValid()) {
        m_info.utcStart = QDateTime::currentDateTimeUtc();
    }
    m_recording = true;
}

void QsoRecorder::stop()
{
    m_recording = false;
    // Am Ende die Sprechspur auf die Empfangsspur bringen: sonst ist die
    // Datei so lang wie die laengere von beiden und die kuerzere endet
    // vorzeitig, was beim Abspielen wie ein Abbruch klingt.
    padTxToRx();
    m_info.seconds = recordedSeconds();
}

void QsoRecorder::clear()
{
    m_rx.clear();
    m_tx.clear();
    m_info = QsoRecordingInfo{};
    m_recording = false;
}

double QsoRecorder::recordedSeconds() const
{
    if (m_rate <= 0) { return 0.0; }
    return static_cast<double>(std::max(m_rx.size(), m_tx.size())) / m_rate;
}

void QsoRecorder::padTxToRx()
{
    // Der Empfang ist die Uhr: er laeuft dauernd, das Mikrofon nur beim
    // Senden. Ohne dieses Auffuellen staende die eigene Stimme am
    // ANFANG der Aufnahme statt an ihrer Stelle im Gespraech.
    if (m_tx.size() < m_rx.size()) {
        m_tx.resize(m_rx.size(), 0.0f);
    }
}

void QsoRecorder::feedRx(const float* interleavedStereo, int frames)
{
    if (!m_recording || interleavedStereo == nullptr || frames <= 0) { return; }

    const int cap = kMaxMinutes * 60 * m_rate;
    if (m_rx.size() >= cap) { return; }

    const int room = std::min(frames, cap - static_cast<int>(m_rx.size()));
    const int base = m_rx.size();
    m_rx.resize(base + room);
    for (int i = 0; i < room; ++i) {
        // Zwei Kanaele zu einem: eine QSO-Aufnahme ist keine
        // Musikproduktion, und der halbe Platz ist es wert.
        m_rx[base + i] = 0.5f * (interleavedStereo[2 * i]
                                 + interleavedStereo[2 * i + 1]);
    }
}

void QsoRecorder::feedTx(const float* mono, int frames)
{
    if (!m_recording || mono == nullptr || frames <= 0) { return; }

    // VOR dem Anhaengen auffuellen — siehe padTxToRx(). Genau hier
    // entscheidet sich, ob die Aufnahme das Gespraech abbildet oder nur
    // seine Bestandteile.
    padTxToRx();

    const int cap = kMaxMinutes * 60 * m_rate;
    if (m_tx.size() >= cap) { return; }

    const int room = std::min(frames, cap - static_cast<int>(m_tx.size()));
    const int base = m_tx.size();
    m_tx.resize(base + room);
    std::copy_n(mono, room, m_tx.begin() + base);
}

bool QsoRecorder::save(const QString& wavPath, QString* error) const
{
    const int frames = std::max(m_rx.size(), m_tx.size());
    if (frames == 0) {
        if (error) { *error = QStringLiteral("nothing recorded"); }
        return false;
    }

    // Verschachteln: links Empfang, rechts eigene Stimme.
    QVector<float> stereo(frames * 2);
    for (int i = 0; i < frames; ++i) {
        stereo[2 * i]     = i < m_rx.size() ? m_rx[i] : 0.0f;
        stereo[2 * i + 1] = i < m_tx.size() ? m_tx[i] : 0.0f;
    }

    if (!writeWavStereo(wavPath, stereo, m_rate, error)) {
        return false;
    }

    // Die Beschreibung daneben, wie Thetis sie fuehrt: eine Aufnahme
    // ohne Frequenz, Modus und Zeit ist in einem halben Jahr nur noch
    // eine Datei mit Rauschen darauf.
    QJsonObject j;
    j.insert(QStringLiteral("utcStart"),
             m_info.utcStart.toString(Qt::ISODate));
    j.insert(QStringLiteral("frequency"), m_info.frequency);
    j.insert(QStringLiteral("mode"),      m_info.mode);
    j.insert(QStringLiteral("band"),      m_info.band);
    j.insert(QStringLiteral("callsign"),  m_info.callsign);
    j.insert(QStringLiteral("sampleRate"), m_rate);
    j.insert(QStringLiteral("seconds"),
             static_cast<double>(frames) / std::max(1, m_rate));
    j.insert(QStringLiteral("tracks"),
             QStringLiteral("left = received, right = own microphone"));

    QString jsonPath = wavPath;
    if (jsonPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        jsonPath.chop(4);
    }
    jsonPath += QStringLiteral(".json");

    QFile f(jsonPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(j).toJson(QJsonDocument::Indented));
        f.close();
    }
    // Eine fehlende Beschreibung ist kein Grund, die AUFNAHME als
    // gescheitert zu melden: der Ton ist das Wertvolle.

    return true;
}

} // namespace NereusSDR
