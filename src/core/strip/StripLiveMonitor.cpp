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
#include "core/strip/AudioPlugin.h"
#include "core/strip/StripSettings.h"

#include <QAudioSink>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QStringList>
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
    , m_inRing(std::make_unique<MicSpectrum>())
    , m_outRing(std::make_unique<MicSpectrum>())
{
    m_chain->prepare(double(kRateHz));
    StripSettings::restore(*m_chain);
    m_chain->setEnabled(true);
    m_inRing->setSampleRate(kRateHz);
    m_outRing->setSampleRate(kRateHz);

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

    // Gewuenschtes Geraet, sonst die Systemvorgabe. Eine Kennung,
    // die es nicht mehr gibt (Geraet abgezogen), faellt still auf
    // die Vorgabe zurueck -- besser als gar kein Mithoeren.
    const auto finde = [](const QList<QAudioDevice>& liste,
                          const QByteArray& id, const QAudioDevice& vorgabe) {
        if (id.isEmpty()) { return vorgabe; }
        for (const QAudioDevice& d : liste) {
            if (d.id() == id) { return d; }
        }
        return vorgabe;
    };
    const QAudioDevice in  = finde(QMediaDevices::audioInputs(), m_wunschIn,
                                   QMediaDevices::defaultAudioInput());
    const QAudioDevice out = finde(QMediaDevices::audioOutputs(), m_wunschOut,
                                   QMediaDevices::defaultAudioOutput());
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
    // ── Einen Mehrkanalwandler NICHT zusammenlegen lassen ────────
    //
    // Gemessen am 2026-10-14: am Apollo Twin (sechzehn Eingaenge)
    // nahm Qt die Bitte um einen Kanal an, und CoreAudio legte die
    // sechzehn selbst zusammen -- Protokoll „channels 1". In Longpath
    // kamen dabei -50 dBFS an, waehrend der Vorverstaerker sauber
    // ausgesteuert war. Fuenfzehn stille Eingaenge in einer Mittelung
    // kosten 20*log10(16) = 24 dB, und wer sie zieht, ist gleichgueltig.
    //
    // Also holt sich der Mithoerer das Geraet in seiner eigenen
    // Kanalzahl und sucht sich den Eingang selbst aus. Zwei Kanaele
    // bleiben wie bisher: ein Stereomikrofon gehoert zusammengelegt.
    const int nativ = in.maximumChannelCount();
    if (nativ > 2) { inFmt.setChannelCount(nativ); }
    if (!in.isFormatSupported(inFmt)) { inFmt.setSampleFormat(QAudioFormat::Int16); }
    if (!in.isFormatSupported(inFmt)) {
        inFmt.setSampleFormat(QAudioFormat::Float);
        inFmt.setChannelCount(1);
        if (!in.isFormatSupported(inFmt)) {
            inFmt.setSampleFormat(QAudioFormat::Int16);
        }
    }
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

    m_inName     = in.description();
    m_outName    = out.description();
    m_inRate     = inFmt.sampleRate();
    m_inRing->setSampleRate(m_inRate);
    m_outRing->setSampleRate(m_inRate);
    m_band.prepare(m_inRate);
    m_gleichLaut.prepare(double(m_inRate));
    m_kanalBegrenzer.prepare(m_inRate);
    m_kanalBegrenzer.setEnabled(true);
    m_inChannels = std::max(1, inFmt.channelCount());
    // Ein neues Geraet, eine neue Suche. Sonst zeigt die Wahl von der
    // vorigen Soundkarte auf einen Kanal, den diese nicht hat.
    m_autoPick = 0;
    m_stilleSek = 0;
    m_kanalSpitze.assign(size_t(m_inChannels), 0.0f);
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
    // Grosszuegig, und das kostet NICHTS an Verzoegerung: der Eingang
    // wird bei jedem Weckruf vollstaendig leergelesen, der Puffer ist
    // also nur die Reserve fuer den Fall, dass ein Weckruf spaet
    // kommt. Zwei Bloecke -- gut zehn Millisekunden -- waren dafuer
    // zu knapp; was in dieser Zeit nicht abgeholt wird, ist beim
    // Eingang unwiederbringlich weg und klingt wie Kratzen. Die
    // Verzoegerung macht allein der Vorrat am Ausgang.
    // qsizetype VORNE, nicht am Ende: `setBufferSize` nimmt qsizetype,
    // aber die vier Faktoren sind `int`, also rechnet C++ erst in int
    // und wandelt DANACH um. CodeQL meldet das als
    // cpp/integer-multiplication-cast-to-long (hoch) und hat recht im
    // Grundsatz -- hier sind es real ein paar tausend, aber die Regel
    // kostet nichts und das Tor steht sonst zu.
    m_src->setBufferSize(qsizetype(m_blockFrames) * m_inChannels
                         * bytesPerSample(inFmt.sampleFormat())
                         * kSinkBlocks);
    m_sink = new QAudioSink(out, outFmt, this);
    // Grosszuegig, damit write() nie abschneidet -- siehe kSinkBlocks.
    m_sink->setBufferSize(qsizetype(m_blockFrames) * m_outBytesPerFrame
                          * kSinkBlocks);
    m_fade = 0.0f;                 // jeder Start blendet ein
    m_hotBlocks = 0;
    m_feedbackCut.store(false, std::memory_order_release);
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
    m_ratio = 1.0;
    m_readPos = 0.0;
    m_lastOut = 0.0f;
    m_underruns.store(0, std::memory_order_release);
    m_overruns.store(0, std::memory_order_release);

    // Mit Vorrat anfangen statt mit nichts.
    //
    // Ohne das ist der Ausgang in dem Augenblick leer, in dem er zu
    // spielen beginnt, und laeuft erst einmal unter -- das war das
    // erste Knacken jedes Starts, und es hatte mit der Puffergroesse
    // gar nichts zu tun.
    {
        // Auf den vollen Sollwert anfuellen, nicht einen Block
        // darunter. Mit einem darunter steht der Vorrat von Anfang an
        // unter der Korrekturschwelle, und die Korrektur laeuft
        // sofort los und hoert nie auf.
        const int priming = m_blockFrames * m_outBytesPerFrame
                          * kTargetBlocks;
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
    // ── Und die Kette zaehlt mit ─────────────────────────────────
    //
    // Hier stand nur der eigene Puffer: ein Eingangsblock plus der
    // Vorrat am Ausgang. Das Entrauschen bringt aber 1440
    // Abtastwerte mit, also 30 ms -- mehr als das Dreifache. Die
    // Zeile im Fenster meldete „rund 11 ms" und es waren 41. Wer
    // sich selbst hoert und die Verzoegerung als unangenehm
    // empfindet, sucht sie dann an der falschen Stelle.
    const double perBlockMs = 1000.0 * m_blockFrames / double(m_inRate);
    m_latencyMs.store(perBlockMs * (1 + kTargetBlocks),
                      std::memory_order_release);

    m_running.store(true, std::memory_order_release);
    qCInfo(lcAudio) << "strip live monitor:" << in.description()
                    << "->" << out.description()
                    << "rate" << m_inRate
                    << "block" << m_blockFrames
                    << "channels" << m_inChannels
                    << "pick" << (m_inChannels > 2
                                      ? (m_inPick.load(
                                             std::memory_order_relaxed) >= 0
                                             ? m_inPick.load(
                                                 std::memory_order_relaxed) + 1
                                             : m_autoPick + 1)
                                      : 0)
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
    if (m_mirror != nullptr) {
        // Direkt von der lebenden Kette. Das ist der Weg, der eine
        // Aenderung WAEHREND des Ziehens hoerbar macht.
        StripSettings::copyParameters(*m_mirror, *m_chain);

        // Die Einschuebe nur jeden zehnten Durchgang: ein Plugin
        // laedt man nicht zehnmal in der Sekunde, und seinen Zustand
        // abzufragen kostet jedes Mal eine Serialisierung.
        if (++m_mirrorTick >= 10) {
            m_mirrorTick = 0;
            for (int i = 0; i < StripChain::kInsertCount; ++i) {
                const AudioPlugin* src = m_mirror->insertPlugin(i);
                AudioPlugin* mine = m_chain->insertPlugin(i);
                m_chain->setInsertPosition(i, m_mirror->insertPosition(i));
                m_chain->setInsertEnabled(i, m_mirror->insertEnabled(i));
                if (src == nullptr) {
                    if (mine != nullptr) { m_chain->setInsertPlugin(i, nullptr); }
                    continue;
                }
                const QString id = src->descriptor().identifier;
                if (mine == nullptr || mine->descriptor().identifier != id) {
                    std::unique_ptr<AudioPlugin> loaded = PluginHost::load(id);
                    if (!loaded) { continue; }
                    loaded->restoreState(src->saveState());
                    m_chain->setInsertPlugin(i, std::move(loaded));
                } else {
                    m_chain->applyInsertState(i, src->saveState());
                }
            }
        }
    } else {
        StripSettings::restore(*m_chain);
    }
    // Dem Ueberbrueckungsschalter folgen, nicht blind einschalten --
    // siehe setStripBypassed().
    m_chain->setEnabled(!m_bypass.load(std::memory_order_acquire));
}

void StripLiveMonitor::doSetVolume(double v)
{
    // ── Bis 4, nicht bis 1 ───────────────────────────────────────
    //
    // Die Kappung bei 1,0 hiess: der Mithoerweg konnte nur DAEMPFEN.
    // Wer seine Kette am Kopfhoerer beurteilt und sie zu leise
    // findet, hatte keinen Weg, das zu aendern -- und bei einer
    // Vorgabe von 0,7 hoerte er sie grundsaetzlich 3 dB unter dem,
    // was die Kette liefert. Als Antwort auf „mir geht die Kraft als
    // Vorverstaerker ab" ist das die Haelfte der Geschichte.
    //
    // Vier ist +12 dB. Darueber faengt QAudioSink selbst an zu
    // schneiden, und lauter als die eigene Kette soll der Mithoerweg
    // ohnehin nicht machen: er zeigt, was sie tut, er macht es nicht.
    m_volume = std::clamp(v, 0.0, 4.0);
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
    if (m_bypass.load(std::memory_order_acquire) == bypassed) { return; }
    m_bypass.store(bypassed, std::memory_order_release);
    // Den Hauptschalter der eigenen Kette mitfuehren. Nicht wegen der
    // Rechnung -- applyStrip() fragt m_bypass ohnehin --, sondern
    // wegen der ANZEIGE: ohne Funkgeraet lesen die Kachelreihe und
    // die Pegelbalken diese Kette, und eine Kette, die als
    // „eingeschaltet" dasteht, waehrend sie ueberbrueckt wird, ist
    // eine Falschaussage an genau der Stelle, an der man hinsieht.
    m_chain->setEnabled(!bypassed);
    qCInfo(lcAudio) << "strip live monitor:"
                    << (bypassed ? "bypassed (raw microphone)"
                                 : "through the strip");
}

double StripLiveMonitor::latencyMs() const
{
    // Der eigene Puffer PLUS die Stufen mit Verzoegerungsleitung.
    // Hier stand nur der Puffer, und die Zeile im Fenster meldete
    // „rund 11 ms", waehrend es mit eingeschaltetem Entrauschen 41
    // waren. Wer sich selbst hoert und die Verzoegerung unangenehm
    // findet, sucht sie dann an der falschen Stelle.
    const double eigen = m_latencyMs.load(std::memory_order_acquire);
    const int rate = m_inRate > 0 ? m_inRate : 48000;
    const double kette = m_chain
        ? 1000.0 * m_chain->nr().latencySamples() / double(rate) : 0.0;
    return eigen + kette;
}

void StripLiveMonitor::setInputDeviceId(const QByteArray& id)
{
    m_wunschIn = id;
}

void StripLiveMonitor::setOutputDeviceId(const QByteArray& id)
{
    m_wunschOut = id;
}

void StripLiveMonitor::setMonitorChannel(int index)
{
    m_band.setChannel(index);
}

int StripLiveMonitor::monitorChannel() const
{
    return m_band.channel();
}

// Siehe m_rohVerzug im Header: der ueberbrueckte Weg muss genauso
// lange brauchen wie die Kette, sonst vergleicht das Ohr Laufzeiten
// statt Klang.
void StripLiveMonitor::setInputPick(int index) noexcept
{
    m_inPick.store(index, std::memory_order_relaxed);
}

void StripLiveMonitor::setLoudnessMatch(bool on) noexcept
{
    m_gleichLaut.setEnabled(on);
}

bool StripLiveMonitor::loudnessMatch() const noexcept
{
    return m_gleichLaut.isEnabled();
}

float StripLiveMonitor::loudnessMatchDb() const noexcept
{
    return m_gleichLaut.gainDb();
}

float StripLiveMonitor::inputPeakDbFs() const noexcept
{
    return m_letzteSekundeIn.load(std::memory_order_relaxed);
}

int StripLiveMonitor::inputChannels() const noexcept
{
    return m_inChannels;
}

int StripLiveMonitor::activeInputChannel() const noexcept
{
    return m_aktiverKanal.load(std::memory_order_relaxed);
}

int StripLiveMonitor::inputPick() const noexcept
{
    return m_inPick.load(std::memory_order_relaxed);
}

void StripLiveMonitor::verzoegereRoh(float* samples, int frames) noexcept
{
    const int soll = std::clamp(m_chain->nr().latencySamples(), 0, kMaxVerzug);
    if (soll <= 0) { return; }
    if (soll != m_rohLaenge) {
        m_rohVerzug.assign(static_cast<size_t>(kMaxVerzug), 0.0f);
        m_rohLaenge = soll;
        m_rohPos = 0;
    }
    for (int i = 0; i < frames; ++i) {
        const float alt = m_rohVerzug[static_cast<size_t>(m_rohPos)];
        m_rohVerzug[static_cast<size_t>(m_rohPos)] = samples[i];
        samples[i] = alt;
        if (++m_rohPos >= m_rohLaenge) { m_rohPos = 0; }
    }
}

void StripLiveMonitor::applyStrip(float* samples, int frames) noexcept
{
    if (samples == nullptr || frames <= 0) { return; }
    if (m_bypass.load(std::memory_order_acquire)) {
        verzoegereRoh(samples, frames);
        return;
    }
    for (int at = 0; at + kChainBlock <= frames; at += kChainBlock) {
        m_chain->processMono(samples + at, kChainBlock);
    }
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

    // ── Welcher Eingang gehoert wird ────────────────────────────
    //
    // Siehe m_inPick im Header. Bei einem oder zwei Kanälen werden
    // sie zusammengelegt; bei mehr wird EINER genommen, sonst
    // dividiert ein Mehrkanalwandler die Stimme in Grund und Boden.
    if (int(m_kanalSpitze.size()) != m_inChannels) {
        m_kanalSpitze.assign(size_t(m_inChannels), 0.0f);
    }
    // Erst binden, dann rechnen: ein Wechsel von sechzehn Eingaengen
    // auf vier liesse m_autoPick sonst auf zehn stehen -- und damit
    // zeigt die Wahl auf einen Kanal, den es nicht gibt. Das Ohr
    // bekaeme Stille (kein c trifft je auf pick), und die Zeile im
    // Protokoll läse neben dem Feld.
    if (m_autoPick >= m_inChannels || m_autoPick < 0) { m_autoPick = 0; }
    const int gewuenscht = m_inPick.load(std::memory_order_relaxed);
    const int pick = (gewuenscht >= 0 && gewuenscht < m_inChannels)
                   ? gewuenscht
                   : (m_inChannels > 2 ? m_autoPick : -1);
    m_aktiverKanal.store(pick, std::memory_order_relaxed);

    const char* raw = chunk.constData();
    for (int f = 0; f < frames; ++f) {
        double sum = 0.0;
        double gewaehlt = 0.0;
        for (int c = 0; c < m_inChannels; ++c) {
            const char* at = raw + size_t(f * m_inChannels + c) * size_t(inBps);
            double v = 0.0;
            switch (inFmt) {
            case QAudioFormat::Float: {
                float x; std::memcpy(&x, at, sizeof(x)); v = double(x); break;
            }
            case QAudioFormat::Int16: {
                qint16 x; std::memcpy(&x, at, sizeof(x)); v = x / 32768.0; break;
            }
            case QAudioFormat::Int32: {
                qint32 x; std::memcpy(&x, at, sizeof(x));
                v = x / 2147483648.0; break;
            }
            case QAudioFormat::UInt8: {
                v = (double(quint8(*at)) - 128.0) / 128.0; break;
            }
            default: break;
            }
            sum += v;
            if (c == pick) { gewaehlt = v; }
            m_kanalSpitze[size_t(c)] =
                std::max(m_kanalSpitze[size_t(c)], float(std::fabs(v)));
        }
        m_mono.push_back(pick >= 0 ? float(gewaehlt)
                                   : float(sum / m_inChannels));
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
    m_inRing->feed(m_mono.data(), int(m_mono.size()));

    // Was hereinkommt, vor der Kette. Ueber die ganze Sekunde
    // gehalten, nicht nur ueber diesen Haeppchen: das Protokoll
    // erschien bisher einmal je Sekunde, zeigte aber die Spitze des
    // Bloecks, der gerade zufaellig die Sekundengrenze ueberschritt.
    // Am 2026-10-14 standen deshalb „-50 dBFS" in der einen Zeile
    // und „Kanal 1: -1,3 dBFS" in der naechsten -- dieselbe Sekunde,
    // dasselbe Signal, zwei Zahlen, fuenfzig Dezibel auseinander.
    float blockSpitzeIn = 0.0f;
    for (float v : m_mono) {
        blockSpitzeIn = std::max(blockSpitzeIn, std::fabs(v));
    }
    m_sekSpitzeIn = std::max(m_sekSpitzeIn, blockSpitzeIn);
    const float peakIn = m_sekSpitzeIn;

    // ── Gleich laut vergleichen ─────────────────────────────────
    //
    // Siehe LoudnessMatch.h. Der Effektivwert VOR der Kette ist die
    // Bezugsgroesse; angewendet wird der Ausgleich ganz am Ende, am
    // Mithoerweg allein.
    m_gleichLaut.feedInput(m_mono.data(), int(m_mono.size()));

    applyStrip(m_mono.data(), int(m_mono.size()));

    // Was herauskommt, nach der Kette. Einmal je Sekunde ins
    // Protokoll, mit dem Vorrat am Ausgang und dem Leseverhaeltnis.
    //
    // Ohne diese Zeile ist „ich hoere nichts" von „die Kette macht
    // nichts" nicht zu unterscheiden, und man raet. Dieser Abend hat
    // genug geraten.
    m_outRing->feed(m_mono.data(), int(m_mono.size()));

    // Erst NACH dem Messabgriff: die Kurve oben zeigt, was die Kette
    // tut, nicht was der Kanal davon durchlaesst. Das Ohr bekommt den
    // Kanal, das Auge die Kette -- und das steht auch so an der
    // Auswahl.
    m_band.process(m_mono.data(), int(m_mono.size()));

    // Und erst jetzt begrenzen -- siehe die Messreihe am
    // m_kanalBegrenzer im Header. Die Decke kommt aus der Kette
    // selbst, damit nicht zwei Zahlen gepflegt werden muessen.
    //
    // NUR wenn die Kette ueberhaupt laeuft. Die Bedingung lautete
    // zuerst allein „ein Kanal ist gewaehlt", und das war ein
    // Fehler mit Ansage: wer den Kanalzug ausschaltet, um sein
    // unbearbeitetes Signal zu hoeren, bekam trotzdem einen
    // Brickwall bei -2 dBFS auf ein Signal, das schon aus einem
    // Studiovorverstaerker kam. Gemessen an echter Sprache mit
    // -1,5 dBFS Spitze: 19,7 dB Fremdanteil, also rund zehn
    // Prozent, auf einem Signal, das niemand bearbeiten wollte --
    // und nur dann, wenn ein Kanal gewaehlt war. Genau der
    // Unterschied, den der Betreiber als „sobald 4000k
    // eingeschaltet ist, wird die Qualitaet massiv schlechter"
    // gemeldet hat.
    if (m_band.channel() >= 0
        && !m_bypass.load(std::memory_order_acquire)
        && m_chain->isEnabled()) {
        m_kanalBegrenzer.setCeilingDb(m_chain->limiter().ceilingDb());
        m_kanalBegrenzer.process(m_mono.data(), int(m_mono.size()), 1);
    }

    // ── Einblenden und die Rückkopplungswache ───────────────────────
    //
    // Siehe StripLiveMonitor.h. Zuerst die Einblendung: der erste
    // Block nach dem Start kommt nicht mit vollem Pegel.
    // ── Erst angleichen, dann einblenden ────────────────────────
    //
    // Der Ausgleich steht NACH Kanal und Kanalbegrenzer: die beiden
    // bilden den Sender nach und sollen den echten Pegel sehen.
    // Angeglichen wird nur, was ins Ohr geht.
    //
    // Und nur, wenn die Kette wirklich lief -- das rohe Signal ist
    // der Massstab und wird nicht gegen sich selbst geregelt.
    //
    // Beim Umschalten auf roh wird NICHT zurueckgesetzt: der
    // erarbeitete Wert soll beim Zurueckschalten sofort wieder da
    // sein. Ein Ausgleich, der jedes Mal bei null anfaengt, laesst
    // die erste Sekunde jeder Kette voll durch.
    if (!m_bypass.load(std::memory_order_acquire) && m_chain->isEnabled()) {
        m_gleichLaut.applyOutput(m_mono.data(), int(m_mono.size()));
    }

    if (m_fade < 1.0f) {
        const float schritt = 1.0f / float(std::max(1, m_inRate / 4));
        for (float& v : m_mono) {
            m_fade = std::min(1.0f, m_fade + schritt);
            v *= m_fade;
        }
    }

    for (float v : m_mono) {
        m_sekSpitzeOut = std::max(m_sekSpitzeOut, std::fabs(v));
    }
    const float peakOut = m_sekSpitzeOut;

    // ── Die Wache WARNT nur noch ────────────────────────────────────
    //
    // Zwei Fassungen, beide falsch, beide am Betreiber ausgegangen:
    // erst stummschalten und das Häkchen ausknipsen („ton kann ich
    // nicht einschalten"), dann leise drehen („das funktioniert
    // nicht"). Der Denkfehler war derselbe: gemessen wurde der
    // AUSGANG — und der steht hinter einem Begrenzer, der ihn
    // absichtlich dauerhaft an die Decke legt. Eine Wache, die auf
    // „laut und dauerhaft" anschlägt, schlägt damit bei jeder
    // ordentlich eingestellten Kette an.
    //
    // Eine Schleife erkennt man nicht am Pegel, sondern an der
    // fehlenden PAUSE: wer spricht, hört zwischendurch auf. Also:
    // bleibt der EINGANG drei Sekunden lang ohne jede Lücke über
    // -30 dBFS, ist das keine Sprache mehr. Gemeldet wird es, mehr
    // nicht — der Regler gehört dem, der ihn hört.
    {
        const double blockMs = 1000.0 * double(m_mono.size())
                               / double(std::max(1, m_inRate));
        // ── Und zwar die Spitze DIESES Haeppchens ───────────────
        //
        // Hier stand peakIn, und das war seit 12:12 die ueber die
        // ganze Sekunde gehaltene Spitze. Damit faellt die Wache um:
        // sie sucht die PAUSE, die gehaltene Zahl kennt aber keine --
        // eine einzige laute Silbe macht die restliche Sekunde heiss,
        // und nach drei Sekunden Sprechen stuende der
        // Rueckkopplungsverdacht da, wo nur jemand geredet hat.
        if (blockSpitzeIn > 0.0316f) {        // -30 dBFS
            m_hotBlocks += int(std::lround(blockMs));
        } else {
            m_hotBlocks = 0;
        }
        const bool verdacht = m_hotBlocks >= 3000;
        if (verdacht != m_feedbackCut.load(std::memory_order_acquire)) {
            m_feedbackCut.store(verdacht, std::memory_order_release);
            if (verdacht) {
                qCWarning(lcAudio)
                    << "strip live monitor: Eingang seit 3 s ohne Pause über"
                    << "-30 dBFS — Rückkopplungsverdacht."
                    << m_inName << "->" << m_outName;
            }
        }
    }

    m_logFrames += int(m_mono.size());
    if (m_logFrames >= m_inRate) {
        m_logFrames = 0;
        const auto dB = [](float p) {
            return p > 1e-6f ? 20.0f * std::log10(p) : -120.0f;
        };
        qCInfo(lcAudio) << "strip live monitor: in" << dB(peakIn) << "dBFS"
                        << "out" << dB(peakOut) << "dBFS"
                        << (m_bypass.load(std::memory_order_acquire)
                                ? "(bypassed)" : "(through strip)")
                        << "queued" << (m_sink->bufferSize()
                                        - m_sink->bytesFree())
                        << "of" << m_sink->bufferSize()
                        << "ratio" << m_ratio
                        << "gleich-laut" << m_gleichLaut.gainDb() << "dB";
        // ── Und je Kanal, wenn das Geraet mehrere hat ────────────
        //
        // Damit „der falsche Kanal" von „der Vorverstaerker steht zu
        // leise" zu unterscheiden ist. Am 2026-10-14 war genau das
        // die offene Frage am Apollo Twin.
        if (m_inChannels > 2) {
            QStringList je;
            for (int c = 0; c < m_inChannels; ++c) {
                je << QStringLiteral("%1:%2")
                          .arg(c + 1)
                          .arg(double(dB(m_kanalSpitze[size_t(c)])),
                               0, 'f', 1);
            }
            qCInfo(lcAudio).noquote()
                << "strip live monitor: je Kanal dBFS"
                << je.join(QLatin1Char(' '));

            // ── Den stillen Platz einmal verlassen ───────────────
            //
            // Nur wenn kein Kanal vorgegeben ist. Drei Sekunden
            // Stille auf dem gehoerten Eingang, waehrend anderswo
            // etwas ist: dann liegt das Mikrofon dort.
            if (m_inPick.load(std::memory_order_relaxed) < 0) {
                int laut = m_autoPick;
                for (int c = 0; c < m_inChannels; ++c) {
                    if (m_kanalSpitze[size_t(c)]
                        > m_kanalSpitze[size_t(laut)]) { laut = c; }
                }
                const bool stillHier =
                    dB(m_kanalSpitze[size_t(m_autoPick)]) < -65.0f;
                const bool lautDort = dB(m_kanalSpitze[size_t(laut)]) > -55.0f;
                if (stillHier && lautDort && laut != m_autoPick) {
                    if (++m_stilleSek >= 3) {
                        qCInfo(lcAudio)
                            << "strip live monitor: Eingang" << (m_autoPick + 1)
                            << "ist still, wechsle auf" << (laut + 1);
                        m_autoPick = laut;
                        m_stilleSek = 0;
                    }
                } else {
                    m_stilleSek = 0;
                }
            }
        }
        std::fill(m_kanalSpitze.begin(), m_kanalSpitze.end(), 0.0f);
        m_letzteSekundeIn.store(dB(m_sekSpitzeIn),
                                std::memory_order_relaxed);
        m_sekSpitzeIn  = 0.0f;
        m_sekSpitzeOut = 0.0f;
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
    // ── Zwei Uhren, dritter und letzter Anlauf ──────────────────────
    //
    // Die beiden davor haben Loecher gestanzt: erst ganze Bloecke
    // weggeworfen (5,3 ms Loch, alle paar Sekunden), dann einzelne
    // Abtastwerte verdoppelt oder weggelassen. Das zweite klang nach
    // Kratzen, weil es bei rund zweihundert Durchgaengen in der
    // Sekunde zweihundert Werte waren; nach dem Deckeln auf zehn
    // klang es immer noch schlecht.
    //
    // Der Fehler war nicht die Haeufigkeit, sondern die Methode. Ein
    // fehlender oder doppelter Abtastwert ist ein SPRUNG in der
    // Wellenform -- ein Knacks, egal wie selten. Und selten macht ihn
    // nicht leiser, nur seltener.
    //
    // Richtig ist, was Thetis fuer dasselbe Problem tut (rmatch, siehe
    // docs/architecture/2026-09-27-rx-audio-rate-match-design.md):
    // nicht schneiden, sondern die Abtastrate ein Haar verstellen.
    // Hier als einfacher Interpolator mit gleitendem Verhaeltnis --
    // laeuft der Vorrat voll, wird minimal schneller gelesen, laeuft
    // er leer, minimal langsamer. Gedeckelt auf zwei Promille, also
    // weit jenseits jedes realen Uhrenversatzes (der liegt unter
    // hundert Millionstel) und dabei noch unhoerbar: zwei Promille
    // sind drei Tausendstel eines Halbtons.
    //
    // Es gibt keinen Sprung mehr. Nirgends.
    const int queued     = m_sink->bufferSize() - m_sink->bytesFree();
    const int blockBytes = m_blockFrames * m_outBytesPerFrame;
    const int wanted     = blockBytes * kTargetBlocks;

    if (wanted > 0) {
        const double error = double(queued - wanted) / double(wanted);
        const double target = std::clamp(1.0 + 0.002 * error,
                                         1.0 - 0.002, 1.0 + 0.002);
        // Traege nachgefuehrt. Ein Verhaeltnis, das jedem Zappeln der
        // Puffermeldung folgt, ist selbst eine Modulation.
        m_ratio += 0.02 * (target - m_ratio);
    }

    // Lineare Interpolation. Bei einem Verhaeltnis von 1,000 plus
    // minus zwei Promille liegt ihr Hoehenverlust bei 5 kHz unter
    // einem fuenftel Dezibel -- messbar, nicht hoerbar, und um
    // Groessenordnungen harmloser als ein einziger Sprung.
    {
        std::vector<float>& in = m_mono;
        m_resampled.clear();
        m_resampled.reserve(in.size() + 2);
        double at = m_readPos;
        while (true) {
            const double idx = at;
            const auto i0 = qsizetype(std::floor(idx));
            if (i0 + 1 >= qsizetype(in.size())) { break; }
            const double f = idx - double(i0);
            const float a = (i0 < 0) ? m_lastOut : in[size_t(i0)];
            const float b = in[size_t(i0 + 1)];
            m_resampled.push_back(float(a + (b - a) * f));
            at += m_ratio;
        }
        // ── Die Blockgrenze, und warum sie ein Sample gekostet hat ──
        //
        // Der naechste Block beginnt bei der alten Stelle N, nicht
        // bei N-1. Hier stand `at - (N-1)`, also eins zu viel --
        // damit fiel je Block ein Abtastwert unter den Tisch. Bei 256
        // Rahmen sind das 0,39 Prozent, und genau so stand es im
        // Protokoll: der Vorrat am Ausgang lief stetig leer (968,
        // 824, 704, 600 ... Bytes), waehrend das Leseverhaeltnis mit
        // 0,9981 am unteren Anschlag klebte und vergeblich
        // dagegenhielt.
        //
        // Das ist auch die Erklaerung fuer „wenn ich channel strip
        // aktiviere hoere ich gar nichts": der Vorrat war da schon
        // fast aufgebraucht.
        //
        // Und die Grenze darf NEGATIV werden: eine Stelle zwischen
        // -1 und 0 liegt zwischen dem letzten Wert des vorigen Blocks
        // (m_lastOut) und dem ersten des neuen. Genau dafuer wird
        // m_lastOut aufgehoben; das alte Abschneiden bei 0 hat den
        // Uebergang jedes Mal verschluckt.
        if (!in.empty()) {
            m_lastOut = in.back();
            m_readPos = std::clamp(at - double(in.size()), -1.0, 1.0);
        }
        m_mono.swap(m_resampled);
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
