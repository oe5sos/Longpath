// =================================================================
// src/core/audio/IqRecorder.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Aufbau und Begruendungen stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/IqRecorder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Longpath {

void IqRecorder::setSampleRate(int hz)
{
    if (hz > 0) { m_rate = hz; }
}

bool IqRecorder::start(const QString& path, const IqRecordingInfo& info,
                       QString* error)
{
    if (m_writer.isOpen()) { return false; }

    const auto format = m_saveFloat32
        ? WavStreamWriter::Format::Float32Stereo
        : WavStreamWriter::Format::Pcm16Stereo;

    if (!m_writer.open(path, m_rate, format, /*dither=*/true, error)) {
        return false;
    }

    m_path = path;
    m_info = info;
    m_info.sampleRate = m_rate;
    if (!m_info.utcStart.isValid()) {
        m_info.utcStart = QDateTime::currentDateTimeUtc();
    }
    return true;
}

void IqRecorder::stop()
{
    if (!m_writer.isOpen()) { return; }

    m_info.seconds = recordedSeconds();
    m_writer.close();

    QJsonObject j;
    j.insert(QStringLiteral("utcStart"),
             m_info.utcStart.toString(Qt::ISODate));
    j.insert(QStringLiteral("frequency"), m_info.frequency);
    j.insert(QStringLiteral("mode"),      m_info.mode);
    j.insert(QStringLiteral("band"),      m_info.band);
    j.insert(QStringLiteral("sampleRate"), m_rate);
    j.insert(QStringLiteral("bitDepth"), m_saveFloat32 ? 32 : 16);
    j.insert(QStringLiteral("format"),
             m_saveFloat32 ? QStringLiteral("IEEE float32")
                           : QStringLiteral("PCM 16-bit, dithered"));
    j.insert(QStringLiteral("seconds"), m_info.seconds);
    j.insert(QStringLiteral("source"),
             QStringLiteral("raw I/Q, pre-demodulation"));
    j.insert(QStringLiteral("channels"),
             QStringLiteral("left = I, right = Q, normalized -1..+1"));

    QString jsonPath = m_path;
    if (jsonPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        jsonPath.chop(4);
    }
    jsonPath += QStringLiteral(".json");

    QFile f(jsonPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(j).toJson(QJsonDocument::Indented));
        f.close();
    }
}

void IqRecorder::feed(const float* interleavedIQ, int frames)
{
    if (!m_writer.isOpen()) { return; }
    m_writer.writeInterleaved(interleavedIQ, frames);
}

double IqRecorder::recordedSeconds() const
{
    if (m_rate <= 0) { return 0.0; }
    return static_cast<double>(m_writer.framesWritten()) / m_rate;
}

IqRecordingInfo readIqRecordingDescription(const QString& wavPath)
{
    IqRecordingInfo info;

    QString jsonPath = wavPath;
    if (jsonPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        jsonPath.chop(4);
    }
    jsonPath += QStringLiteral(".json");

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) { return info; }
    const QByteArray blob = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(blob, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return info;
    }

    const QJsonObject j = doc.object();
    info.utcStart   = QDateTime::fromString(
        j.value(QStringLiteral("utcStart")).toString(), Qt::ISODate);
    info.frequency  = j.value(QStringLiteral("frequency")).toString();
    info.mode       = j.value(QStringLiteral("mode")).toString();
    info.band       = j.value(QStringLiteral("band")).toString();
    info.sampleRate = j.value(QStringLiteral("sampleRate")).toInt();
    info.seconds    = j.value(QStringLiteral("seconds")).toDouble();
    return info;
}

} // namespace Longpath
