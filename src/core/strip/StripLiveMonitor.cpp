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
#include <QThread>

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

    // Ein eigener Faden, und zwar ein hoch eingestufter.
    //
    // Ohne ihn haengt die Antwortzeit des Mithoerens an allem, was
    // das Bedienfenster sonst noch tut -- Kurven zeichnen, Zeiger
    // nachziehen, Zeitgeber alle 100 ms. Mit 5 ms Vorrat am Ausgang
    // waere das ein Knacken je Zeichenvorgang. Hier schlaeft der
    // Faden zwischen zwei Mikrofonbloecken und tut sonst nichts.
    //
    // Das Objekt selbst zieht mit um: start/stop/resync/setVolume
    // reichen deshalb nur weiter, statt selbst zu arbeiten.
    m_thread = new QThread;
    m_thread->setObjectName(QStringLiteral("StripLiveMonitor"));
    moveToThread(m_thread);
    m_thread->start(QThread::TimeCriticalPriority);
}

StripLiveMonitor::~StripLiveMonitor()
{
    // Erst anhalten -- und zwar DORT, wo die Geraete aufgemacht
    // wurden; ein QAudioSource gehoert seinem Faden. Dann den Faden
    // beenden und auf ihn warten, bevor irgendetwas hier zerfaellt.
    if (m_thread != nullptr) {
        QMetaObject::invokeMethod(this, "doStop", Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    } else {
        teardown();
    }
}

void StripLiveMonitor::start()
{
    QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
}

void StripLiveMonitor::stop()
{
    QMetaObject::invokeMethod(this, "doStop", Qt::QueuedConnection);
}

void StripLiveMonitor::resync()
{
    QMetaObject::invokeMethod(this, "doResync", Qt::QueuedConnection);
}

void StripLiveMonitor::setVolume(double v)
{
    QMetaObject::invokeMethod(this, "doSetVolume", Qt::QueuedConnection,
                              Q_ARG(double, v));
}

void StripLiveMonitor::doStart()
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

    m_blockFrames = blockFramesFor(m_preset);

    m_src = new QAudioSource(in, inFmt, this);
    m_src->setBufferSize(m_blockFrames * m_inChannels
                         * bytesPerSample(inFmt.sampleFormat()) * 2);
    m_sink = new QAudioSink(out, outFmt, this);
    // Grosszuegig, damit write() nie abschneidet -- siehe kSinkBlocks.
    m_sink->setBufferSize(m_blockFrames * m_outBytesPerFrame * kSinkBlocks);
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

    // Mit Vorrat anfangen statt mit nichts.
    //
    // Ohne das ist der Ausgang in dem Augenblick leer, in dem er zu
    // spielen beginnt, und laeuft erst einmal unter -- das war das
    // erste Knacken jedes Starts, und es hatte mit der Puffergroesse
    // gar nichts zu tun.
    {
        const int priming = m_blockFrames * m_outBytesPerFrame
                          * (kTargetBlocks - 1);
        const std::vector<char> stille(size_t(std::max(0, priming)), 0);
        if (!stille.empty()) {
            m_sinkIo->write(stille.data(), qint64(stille.size()));
        }
    }

    // Der eigene Anteil: ein Eingangsblock plus der Vorrat am Ausgang.
    // Bei 128 Rahmen und zwei Bloecken Vorrat sind das 8 ms. Darunter
    // kommt man mit Qts Audioklassen nicht mehr weit -- der naechste
    // Schritt waere CoreAudio von Hand, mit zwei AUHAL-Einheiten und
    // einem Ring dazwischen; dann waeren auch die Zahlen der Geraete
    // selbst (kAudioDevicePropertyLatency, SafetyOffset) abzulesen,
    // statt sie als „kommt noch dazu" zu umschreiben.
    const double perBlockMs = 1000.0 * m_blockFrames / double(m_inRate);
    m_latencyMs.store(perBlockMs * (1 + kTargetBlocks),
                      std::memory_order_release);

    m_running.store(true, std::memory_order_release);
    qCInfo(lcAudio) << "strip live monitor:" << in.description()
                    << "->" << out.description()
                    << "rate" << m_inRate
                    << "block" << m_blockFrames
                    << "own latency ~" << m_latencyMs.load() << "ms";
    emit started();
}

void StripLiveMonitor::doStop()
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

void StripLiveMonitor::doResync()
{
    // Parameter, keine Zustaende. Siehe die Kopfdatei: dafuer sind die
    // Atomaren mit Versionszaehler in jeder Stufe da, und das Fenster
    // schreibt auf demselben Weg auch in die Sendekette.
    StripSettings::restore(*m_chain);
    m_chain->setEnabled(true);
}

void StripLiveMonitor::doSetVolume(double v)
{
    m_volume = std::clamp(v, 0.0, 1.0);
    if (m_sink) { m_sink->setVolume(float(m_volume)); }
}

void StripLiveMonitor::setLatencyPreset(Latency l)
{
    QMetaObject::invokeMethod(this, "doSetLatencyPreset", Qt::QueuedConnection,
                              Q_ARG(int, int(l)));
}

void StripLiveMonitor::doSetLatencyPreset(int l)
{
    const Latency want = static_cast<Latency>(std::clamp(l, 0, 2));
    if (want == m_preset) { return; }
    m_preset = want;
    if (m_running.load(std::memory_order_acquire)) {
        doStop();
        doStart();
    }
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
    // ── Zwei Uhren, die nicht dieselbe sind ─────────────────────────
    //
    // Am Geraet abgelesen (Protokoll, 2026-10-09 21:20):
    //
    //     strip live monitor: "MacBook Air-Mikrofon"
    //                      -> "Teufel Gaming Headset"
    //
    // Zwei verschiedene Geraete, beide mit 48000 angeschrieben, jedes
    // mit seinem eigenen Quarz. Ein Auseinanderlaufen ist damit nicht
    // die Ausnahme, sondern der Normalfall, und es MUSS ausgeglichen
    // werden. Die Frage ist nur, ob man es hoert.
    //
    // Die erste Fassung glich in GANZEN Bloecken aus: bei Ueberlauf
    // einen Block von 256 Rahmen wegwerfen, bei Unterlauf eine halbe
    // Blocklaenge Stille nachlegen. Das sind 5,3 bzw. 2,7 Millisekunden
    // Loch, alle paar Sekunden, immer wieder -- eine Modulation, und
    // genau so klang es auch.
    //
    // Jetzt je Durchgang hoechstens EIN Abtastwert: einer zu viel wird
    // weggelassen, einer zu wenig wird verdoppelt. Bei rund 200
    // Durchgaengen in der Sekunde reicht das fuer ein Vielfaches
    // jedes realen Uhrenversatzes, und ein einzelner Wert von 48000
    // ist nichts, was ein Ohr als Ereignis wahrnimmt.
    //
    // Die Totzone ist breit: korrigiert wird erst, wenn der Vorrat um
    // mehr als einen halben Block von seinem Soll abweicht. Ohne sie
    // korrigierte man dauernd gegen das eigene Rauschen.
    const int queued     = m_sink->bufferSize() - m_sink->bytesFree();
    const int blockBytes = m_blockFrames * m_outBytesPerFrame;
    const int wanted     = blockBytes * kTargetBlocks;
    const int slack      = blockBytes / 2;

    if (queued > wanted + slack && m_mono.size() > 1) {
        // Zu viel Vorrat: der Eingang ist schneller. Einen Wert
        // weglassen, und zwar den leisesten im Block -- dort faellt
        // ein fehlender Schritt am wenigsten auf.
        size_t quietest = 0;
        float  lowest = std::fabs(m_mono[0]);
        for (size_t i = 1; i < m_mono.size(); ++i) {
            const float a = std::fabs(m_mono[i]);
            if (a < lowest) { lowest = a; quietest = i; }
        }
        m_mono.erase(m_mono.begin() + qsizetype(quietest));
        m_overruns.fetch_add(1, std::memory_order_acq_rel);
    } else if (queued < wanted - slack && !m_mono.empty()) {
        // Zu wenig Vorrat: den leisesten Wert verdoppeln.
        size_t quietest = 0;
        float  lowest = std::fabs(m_mono[0]);
        for (size_t i = 1; i < m_mono.size(); ++i) {
            const float a = std::fabs(m_mono[i]);
            if (a < lowest) { lowest = a; quietest = i; }
        }
        m_mono.insert(m_mono.begin() + qsizetype(quietest),
                      m_mono[quietest]);
        m_underruns.fetch_add(1, std::memory_order_acq_rel);
    }

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
