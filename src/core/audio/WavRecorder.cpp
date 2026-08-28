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

// =================================================================
// src/core/audio/WavRecorder.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Aufbau und Begruendungen stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/WavRecorder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Longpath {

void WavRecorder::setSampleRate(int hz)
{
    if (hz > 0) { m_rate = hz; }
}

bool WavRecorder::start(const QString& path, const WavRecordingInfo& info,
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

void WavRecorder::stop()
{
    if (!m_writer.isOpen()) { return; }

    m_info.seconds = recordedSeconds();
    m_writer.close();

    // Die Beschreibung daneben, wie Thetis sie fuehrt und wie
    // QsoRecorder es fuer die QSO-Aufnahme tut: eine Aufnahme ohne
    // Frequenz, Modus und Zeit ist in einem halben Jahr nur noch eine
    // Datei mit Rauschen darauf.
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
    j.insert(QStringLiteral("source"), QStringLiteral("off the air"));

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
    // Eine fehlende Beschreibung ist kein Grund, die AUFNAHME als
    // gescheitert zu melden: der Ton ist das Wertvolle.
}

void WavRecorder::feed(const float* interleavedStereo, int frames)
{
    if (!m_writer.isOpen()) { return; }
    m_writer.writeInterleaved(interleavedStereo, frames);
}

double WavRecorder::recordedSeconds() const
{
    if (m_rate <= 0) { return 0.0; }
    return static_cast<double>(m_writer.framesWritten()) / m_rate;
}

WavRecordingInfo readWavRecordingDescription(const QString& wavPath)
{
    WavRecordingInfo info;

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
