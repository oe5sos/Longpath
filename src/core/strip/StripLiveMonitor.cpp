// =================================================================
// src/core/strip/StripLiveMonitor.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Warum dieser Weg und nicht der ueber die
// Sendepumpe: siehe StripLiveMonitor.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include "core/strip/StripLiveMonitor.h"

#include "core/LogCategories.h"
#include "core/strip/MicSpectrum.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <QAudioSink>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Longpath {

namespace {

constexpr int kRateHz = 48000;

// Die Kette rechnet in kleinen Haeppchen -- dieselben 64 Rahmen, die
// auch die Sendepumpe nimmt, damit Gate-Vorausschau und Kompressor
// sich genau so verhalten wie auf Sendung.
constexpr int kChainBlock = 64;

int bytesPerSample(QAudioFormat::SampleFormat f)
{
    switch (f) {
    case QAudioFormat::UInt8: return 1;
    case QAudioFormat::Int16: return 2;
    case QAudioFormat::Int32: return 4;
    case QAudioFormat::Float: return 4;
    default:                  return 0;
    }
}

} // namespace

StripLiveMonitor::StripLiveMonitor(QObject* parent)
    : QObject(parent)
    , m_chain(std::make_unique<StripChain>())
{
    m_chain->prepare(double(kRateHz));
    StripSettings::restore(*m_chain);
    m_chain->setEnabled(true);
}

StripLiveMonitor::~StripLiveMonitor()
{
    teardown();
}

void StripLiveMonitor::start()
{
    if (m_running.load(std::memory_order_acquire)) { return; }

    const QAudioDevice in  = QMediaDevices::defaultAudioInput();
    const QAudioDevice out = QMediaDevices::defaultAudioOutput();
    if (in.isNull()) {
        emit failed(QStringLiteral("No microphone this computer can open."));
        return;
    }
    if (out.isNull()) {
        emit failed(QStringLiteral("No sound output this computer can open."));
        return;
    }

    // Eingang: Gleitkomma, einkanalig, Senderate -- in dieser
    // Reihenfolge erfragt, weil jede Umrechnung weniger eine
    // Fehlerquelle weniger ist. Was das Geraet nicht hergibt, wird
    // hier umgerechnet, nicht erraten.
    QAudioFormat inFmt;
    inFmt.setSampleRate(kRateHz);
    inFmt.setChannelCount(1);
    inFmt.setSampleFormat(QAudioFormat::Float);
    if (!in.isFormatSupported(inFmt)) { inFmt.setSampleFormat(QAudioFormat::Int16); }
    if (!in.isFormatSupported(inFmt)) { inFmt = in.preferredFormat(); }

    QAudioFormat outFmt;
    outFmt.setSampleRate(inFmt.sampleRate());
    outFmt.setChannelCount(2);
    outFmt.setSampleFormat(QAudioFormat::Float);
    if (!out.isFormatSupported(outFmt)) { outFmt.setSampleFormat(QAudioFormat::Int16); }
    if (!out.isFormatSupported(outFmt)) {
        outFmt = out.preferredFormat();
        outFmt.setSampleRate(inFmt.sampleRate());
        if (!out.isFormatSupported(outFmt)) { outFmt = out.preferredFormat(); }
    }

    // Verschiedene Raten auf beiden Seiten koennte man umrechnen; hier
    // wird stattdessen abgelehnt. Ein Mithoeren, das die Tonhoehe
    // verschiebt, ist schlimmer als keines, und es faellt beim
    // Beurteilen einer Stimme nicht sofort auf.
    if (outFmt.sampleRate() != inFmt.sampleRate()) {
        emit failed(QStringLiteral(
            "Microphone and speaker insist on different sample rates "
            "(%1 and %2 Hz). Set both to the same rate in Audio MIDI "
            "Setup and try again.")
            .arg(inFmt.sampleRate()).arg(outFmt.sampleRate()));
        return;
    }

    m_inRate     = inFmt.sampleRate();
    m_inChannels = std::max(1, inFmt.channelCount());
    m_inFormat   = int(inFmt.sampleFormat());
    m_outChannels = std::max(1, outFmt.channelCount());
    m_outFormat   = int(outFmt.sampleFormat());
    m_outBytesPerFrame = m_outChannels * bytesPerSample(outFmt.sampleFormat());
    if (m_outBytesPerFrame <= 0 || bytesPerSample(inFmt.sampleFormat()) <= 0) {
        emit failed(QStringLiteral("Unsupported sample format."));
        return;
    }

    m_chain->prepare(double(m_inRate));
    StripSettings::restore(*m_chain);
    m_chain->setEnabled(true);

    m_src = new QAudioSource(in, inFmt, this);
    m_src->setBufferSize(kBlockFrames * m_inChannels
                         * bytesPerSample(inFmt.sampleFormat()) * 2);
    m_sink = new QAudioSink(out, outFmt, this);
    m_sink->setBufferSize(kBlockFrames * m_outBytesPerFrame * kTargetBlocks);
    m_sink->setVolume(float(m_volume));

    m_sinkIo = m_sink->start();            // Schiebebetrieb: wir fuettern
    if (!m_sinkIo) {
        teardown();
        emit failed(QStringLiteral("The sound output would not open."));
        return;
    }
    m_srcIo = m_src->start();
    if (!m_srcIo) {
        teardown();
        emit failed(QStringLiteral("The microphone would not open."));
        return;
    }
    connect(m_srcIo, &QIODevice::readyRead, this, &StripLiveMonitor::pump);

    m_carry.clear();
    m_underruns.store(0, std::memory_order_release);
    m_overruns.store(0, std::memory_order_release);

    // Der eigene Anteil: ein Eingangsblock plus der Vorrat am Ausgang.
    const double perBlockMs = 1000.0 * kBlockFrames / double(m_inRate);
    m_latencyMs.store(perBlockMs * (1 + kTargetBlocks),
                      std::memory_order_release);

    m_running.store(true, std::memory_order_release);
    qCInfo(lcAudio) << "strip live monitor:" << in.description()
                    << "->" << out.description()
                    << "rate" << m_inRate
                    << "block" << kBlockFrames
                    << "own latency ~" << m_latencyMs.load() << "ms";
    emit started();
}

void StripLiveMonitor::stop()
{
    if (!m_running.load(std::memory_order_acquire)) { return; }
    teardown();
    m_running.store(false, std::memory_order_release);
    emit stopped();
}

void StripLiveMonitor::teardown()
{
    if (m_srcIo) {
        QObject::disconnect(m_srcIo, nullptr, this, nullptr);
        m_srcIo = nullptr;
    }
    if (m_src)  { m_src->stop();  m_src->deleteLater();  m_src = nullptr; }
    if (m_sink) { m_sink->stop(); m_sink->deleteLater(); m_sink = nullptr; }
    m_sinkIo = nullptr;
}

void StripLiveMonitor::resync()
{
    // Parameter, keine Zustaende. Siehe die Kopfdatei: dafuer sind die
    // Atomaren mit Versionszaehler in jeder Stufe da, und das Fenster
    // schreibt auf demselben Weg auch in die Sendekette.
    StripSettings::restore(*m_chain);
    m_chain->setEnabled(true);
}

void StripLiveMonitor::setVolume(double v)
{
    m_volume = std::clamp(v, 0.0, 1.0);
    if (m_sink) { m_sink->setVolume(float(m_volume)); }
}

void StripLiveMonitor::setStripBypassed(bool bypassed)
{
    m_bypass.store(bypassed, std::memory_order_release);
}

void StripLiveMonitor::pump()
{
    if (!m_srcIo || !m_sinkIo || !m_sink) { return; }

    const auto inFmt = static_cast<QAudioFormat::SampleFormat>(m_inFormat);
    const int inBps  = bytesPerSample(inFmt);
    const QByteArray chunk = m_srcIo->readAll();
    if (chunk.isEmpty() && m_carry.empty()) { return; }

    // ── Hereinkommendes auf einkanaliges Gleitkomma bringen ─────────
    const int frames = (inBps > 0 && m_inChannels > 0)
                     ? int(chunk.size()) / (inBps * m_inChannels) : 0;
    m_mono.assign(m_carry.begin(), m_carry.end());
    m_carry.clear();
    m_mono.reserve(m_mono.size() + size_t(frames));

    const char* raw = chunk.constData();
    for (int f = 0; f < frames; ++f) {
        double sum = 0.0;
        for (int c = 0; c < m_inChannels; ++c) {
            const char* at = raw + size_t(f * m_inChannels + c) * size_t(inBps);
            switch (inFmt) {
            case QAudioFormat::Float: {
                float v; std::memcpy(&v, at, sizeof(v)); sum += double(v); break;
            }
            case QAudioFormat::Int16: {
                qint16 v; std::memcpy(&v, at, sizeof(v)); sum += v / 32768.0; break;
            }
            case QAudioFormat::Int32: {
                qint32 v; std::memcpy(&v, at, sizeof(v));
                sum += v / 2147483648.0; break;
            }
            case QAudioFormat::UInt8: {
                sum += (double(quint8(*at)) - 128.0) / 128.0; break;
            }
            default: break;
            }
        }
        m_mono.push_back(float(sum / m_inChannels));
    }

    // Nur ganze Kettenbloecke rechnen; der Rest wartet auf den
    // naechsten Weckruf. Ein halber Block durch einen Kompressor
    // gejagt ist kein Fehler, aber ein unnoetiger Unterschied zur
    // Sendekette, und Unterschiede sind hier genau das, was stoert.
    const size_t usable = (m_mono.size() / kChainBlock) * kChainBlock;
    if (usable < m_mono.size()) {
        m_carry.assign(m_mono.begin() + qsizetype(usable), m_mono.end());
        m_mono.resize(usable);
    }
    if (m_mono.empty()) { return; }

    // Erst der Kurve geben, dann rechnen: die Entzerreranzeige will
    // das Signal, wie es hereinkommt. Siehe setSpectrumSink().
    if (m_spectrum != nullptr) {
        m_spectrum->feed(m_mono.data(), int(m_mono.size()));
    }

    if (!m_bypass.load(std::memory_order_acquire)) {
        for (size_t at = 0; at + kChainBlock <= m_mono.size(); at += kChainBlock) {
            m_chain->processMono(m_mono.data() + at, kChainBlock);
        }
    }

    // ── Hinaus ───────────────────────────────────────────────────────
    //
    // Zwei Uhren, die nicht dieselbe sind: laeuft der Vorrat voll, wird
    // ein Block weggeworfen, laeuft er leer, legt die Soundkarte von
    // sich aus Stille nach. Beides wird gezaehlt, damit man es sagen
    // kann, statt es zu ahnen.
    const int queued = m_sink->bufferSize() - m_sink->bytesFree();
    const int maxQueued = kBlockFrames * m_outBytesPerFrame * kTargetBlocks;
    if (queued > maxQueued) {
        m_overruns.fetch_add(1, std::memory_order_acq_rel);
        return;
    }
    if (queued <= 0) { m_underruns.fetch_add(1, std::memory_order_acq_rel); }

    const auto outFmt = static_cast<QAudioFormat::SampleFormat>(m_outFormat);
    m_outBytes.resize(m_mono.size() * size_t(m_outBytesPerFrame));
    char* dst = m_outBytes.data();
    for (float v : m_mono) {
        const float s = std::clamp(v, -1.0f, 1.0f);
        for (int c = 0; c < m_outChannels; ++c) {
            switch (outFmt) {
            case QAudioFormat::Float: {
                std::memcpy(dst, &s, sizeof(s)); dst += sizeof(s); break;
            }
            case QAudioFormat::Int16: {
                const auto q = qint16(std::lround(s * 32767.0f));
                std::memcpy(dst, &q, sizeof(q)); dst += sizeof(q); break;
            }
            case QAudioFormat::Int32: {
                const auto q = qint32(std::lround(double(s) * 2147483647.0));
                std::memcpy(dst, &q, sizeof(q)); dst += sizeof(q); break;
            }
            case QAudioFormat::UInt8: {
                *dst++ = char(quint8(std::lround(s * 127.0f) + 128)); break;
            }
            default: break;
            }
        }
    }
    m_sinkIo->write(m_outBytes.data(), qint64(m_outBytes.size()));
}

} // namespace Longpath
