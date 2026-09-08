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
// src/core/audio/QsoRecorder.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Aufbau und Begruendungen stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-09-02 — Streamend statt gesammelt (zeitlich unbegrenzt), von
//                 Martin Fischer, KI-gestuetzt ueber Anthropic Claude
//                 (Cowork). Begruendung im Header.
// =================================================================

#include "core/audio/QsoRecorder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace Longpath {

void QsoRecorder::setSampleRate(int hz)
{
    if (hz > 0) { m_rate = hz; }
}

bool QsoRecorder::start(const QString& wavPath, const QsoRecordingInfo& info,
                        QString* error)
{
    clear();
    m_path = wavPath;
    m_info = info;
    m_info.sampleRate = m_rate;
    if (!m_info.utcStart.isValid()) {
        m_info.utcStart = QDateTime::currentDateTimeUtc();
    }

    const auto fmt = m_saveFloat32 ? WavStreamWriter::Format::Float32Stereo
                                    : WavStreamWriter::Format::Pcm16Stereo;
    if (!m_writer.open(wavPath, m_rate, fmt, /*dither=*/true, error)) {
        return false;
    }

    m_recording = true;
    return true;
}

void QsoRecorder::stop()
{
    if (!m_recording) { return; }
    m_recording = false;

    // Am Ende die kuerzere Spur auf die laengere bringen: sonst bricht
    // die Datei mit der kuerzeren Spur ab, was beim Abspielen wie ein
    // Abbruch klingt. In der Praxis ist das fast immer die Sprechspur
    // (siehe Header), aber wer genau beim Loslassen der Taste stoppt,
    // kann auch kurz die andere Richtung erwischen — finalizeTail()
    // deckt beide ab.
    finalizeTail();
    m_info.seconds = recordedSeconds();

    const qint64 written = m_writer.framesWritten();
    m_writer.close();

    if (written == 0) {
        // Kam nichts an, bleibt keine leere Datei liegen — dieselbe
        // Zusicherung, die frueher save() gab, als es noch nichts zu
        // schreiben gab.
        QFile::remove(m_path);
        return;
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
    // Damit man in einem halben Jahr nicht raten muss, was in der
    // Datei steht.
    j.insert(QStringLiteral("bitDepth"), m_saveFloat32 ? 32 : 16);
    j.insert(QStringLiteral("format"),
             m_saveFloat32 ? QStringLiteral("IEEE float32")
                           : QStringLiteral("PCM 16-bit, dithered"));
    j.insert(QStringLiteral("seconds"), m_info.seconds);
    j.insert(QStringLiteral("tracks"),
             QStringLiteral("left = received, right = own microphone"));

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

void QsoRecorder::clear()
{
    m_rxPending.clear();
    m_txPending.clear();
    m_rxFedTotal = 0;
    m_txFedTotal = 0;
    m_info = QsoRecordingInfo{};
    m_path.clear();
    m_recording = false;
    if (m_writer.isOpen()) { m_writer.close(); }
}

double QsoRecorder::recordedSeconds() const
{
    if (m_rate <= 0) { return 0.0; }
    // Der Empfang ist die Uhr (siehe Header) — er laeuft dauernd, das
    // Mikrofon nur beim Senden. Was schon auf der Platte liegt, plus
    // was noch im Zwischenspeicher wartet.
    return static_cast<double>(m_rxFedTotal) / m_rate;
}

void QsoRecorder::flushAligned()
{
    const int n = std::min(m_rxPending.size(), m_txPending.size());
    if (n <= 0) { return; }

    // Verschachteln: links Empfang, rechts eigene Stimme.
    QVector<float> interleaved(n * 2);
    for (int i = 0; i < n; ++i) {
        interleaved[2 * i]     = m_rxPending[i];
        interleaved[2 * i + 1] = m_txPending[i];
    }
    m_writer.writeInterleaved(interleaved.constData(), n);

    m_rxPending.remove(0, n);
    m_txPending.remove(0, n);
}

void QsoRecorder::finalizeTail()
{
    if (m_rxPending.size() < m_txPending.size()) {
        m_rxPending.resize(m_txPending.size(), 0.0f);
    } else if (m_txPending.size() < m_rxPending.size()) {
        m_txPending.resize(m_rxPending.size(), 0.0f);
    }
    flushAligned();
}

void QsoRecorder::feedRx(const float* interleavedStereo, int frames)
{
    if (!m_recording || interleavedStereo == nullptr || frames <= 0) { return; }

    const int base = m_rxPending.size();
    m_rxPending.resize(base + frames);
    for (int i = 0; i < frames; ++i) {
        // Zwei Kanaele zu einem: eine QSO-Aufnahme ist keine
        // Musikproduktion, und der halbe Platz ist es wert.
        m_rxPending[base + i] = 0.5f * (interleavedStereo[2 * i]
                                        + interleavedStereo[2 * i + 1]);
    }
    m_rxFedTotal += frames;

    flushAligned();
}

void QsoRecorder::feedTx(const float* mono, int frames)
{
    if (!m_recording || mono == nullptr || frames <= 0) { return; }

    // VOR dem Anhaengen auffuellen — dieselbe Ausrichtung wie vor dem
    // Umbau auf Streamen, nur auf den Zwischenspeicher bezogen statt
    // auf die ganze Aufnahme: genau hier entscheidet sich, ob die
    // Aufnahme das Gespraech abbildet oder nur seine Bestandteile.
    if (m_txPending.size() < m_rxPending.size()) {
        m_txPending.resize(m_rxPending.size(), 0.0f);
    }

    const int base = m_txPending.size();
    m_txPending.resize(base + frames);
    std::copy_n(mono, frames, m_txPending.begin() + base);
    m_txFedTotal += frames;

    flushAligned();
}

// Der Gegenpart zu stop(). Siehe Header fuer die Herkunft.
QsoRecordingInfo readQsoDescription(const QString& wavPath)
{
    QsoRecordingInfo info;

    QString jsonPath = wavPath;
    if (jsonPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        jsonPath.chop(4);
        jsonPath += QStringLiteral(".json");
    }

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
    info.callsign   = j.value(QStringLiteral("callsign")).toString();
    info.sampleRate = j.value(QStringLiteral("sampleRate")).toInt();
    info.seconds    = j.value(QStringLiteral("seconds")).toDouble();
    return info;
}

} // namespace Longpath
