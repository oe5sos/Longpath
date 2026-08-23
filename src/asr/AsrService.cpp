// =================================================================
// src/asr/AsrService.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung steht in der Kopfdatei.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "asr/AsrService.h"

#include "core/LogCategories.h"
#include "core/Resampler.h"
#include "core/audio/AudioTapRing.h"

#include <QThread>
#include <QTimer>

namespace Longpath {

namespace {
// Wie oft der Ring geleert wird. 50 ms ist reichlich: der Zerleger
// arbeitet in 10-ms-Rahmen, und der Ring fasst weit mehr.
constexpr int kPumpIntervalMs = 50;
constexpr int kAsrRateHz = 16000;   // was jeder Whisper-Dienst erwartet
// Hoechstzahl Rahmen je Wandleraufruf. Muss zur Groesse passen,
// mit der der Resampler gebaut wird — sonst rechnet er auf einem
// Puffer, der ihm zu klein ist.
constexpr int kResampleBlockFrames = 4096;
} // namespace

// ── Der Arbeiter auf dem eigenen Faden ──────────────────────────────
//
// Er tut nur eines: einen Abschnitt entgegennehmen, den Erkenner rufen
// und das Ergebnis zurueckmelden. Der Aufruf blockiert — deshalb sitzt
// er hier und nicht im Hauptfaden.
class AsrWorker : public QObject {
    Q_OBJECT
public:
    explicit AsrWorker(IAsrBackend* backend) : m_backend(backend) {}

public slots:
    void transcribe(const std::vector<float>& pcm16k)
    {
        if (!m_backend) { return; }
        QString error;
        const AsrTranscript t = m_backend->transcribe(pcm16k, &error);
        if (!error.isEmpty()) {
            emit failed(error);
            return;
        }
        if (!t.text.trimmed().isEmpty()) {
            emit done(t.text.trimmed(), t.confidence);
        }
    }

signals:
    void done(const QString& text, float confidence);
    void failed(const QString& reason);

private:
    IAsrBackend* m_backend{nullptr};
};

AsrService::AsrService(QObject* parent)
    : QObject(parent)
{
    // ── Ohne diese Zeile kommt NICHTS an ────────────────────────────
    //
    // dispatch() reicht den Abschnitt ueber die Warteschlange an den
    // Arbeiterfaden. Qt kann einen Typ nur ueber eine Warteschlange
    // schicken, wenn er angemeldet ist — sonst schlaegt der Aufruf
    // fehl und gibt lediglich false zurueck. Kein Uebersetzungsfehler,
    // keine Meldung: es passiert einfach nichts.
    //
    // Genau die Sorte Fehler, die man erst bemerkt, wenn der Betreiber
    // sagt "es kommt kein Text". Der Prueflauf unten faengt sie.
    qRegisterMetaType<std::vector<float>>("std::vector<float>");

    m_pump = new QTimer(this);
    m_pump->setInterval(kPumpIntervalMs);
    connect(m_pump, &QTimer::timeout, this, &AsrService::drainRing);
}

AsrService::~AsrService()
{
    stop();
}

void AsrService::setBackend(std::unique_ptr<IAsrBackend> backend)
{
    // Nicht im Lauf tauschen: der Arbeiter haelt einen nackten Zeiger
    // darauf, und ihn unter dessen Fuessen wegzuziehen waere genau die
    // Art Fehler, die nur unter Last auftritt.
    if (m_running) { stop(); }
    m_backend = std::move(backend);
}

void AsrService::setSource(AudioTapRing* ring, int sourceRateHz)
{
    m_ring = ring;
    m_sourceRate = sourceRateHz > 0 ? sourceRateHz : 48000;
    m_resampler.reset();
}

void AsrService::start()
{
    if (m_running || !m_backend) { return; }

    QString error;
    if (!m_backend->load(QString(), &error)) {
        emit failed(error.isEmpty()
                        ? tr("Der Erkennungsdienst liess sich nicht öffnen.")
                        : error);
        return;
    }

    m_worker = new QThread;
    m_workerObj = new AsrWorker(m_backend.get());
    m_workerObj->moveToThread(m_worker);
    connect(m_worker, &QThread::finished, m_workerObj, &QObject::deleteLater);
    connect(m_workerObj, &AsrWorker::done, this, &AsrService::transcript);
    connect(m_workerObj, &AsrWorker::failed, this, [this](const QString& r) {
        // Einmal je Ursache. Ein abgeschalteter Dienst wuerde sonst bei
        // jedem Sprechabschnitt dieselbe Meldung schreiben.
        if (r == m_lastFailure) { return; }
        m_lastFailure = r;
        qCWarning(lcDsp) << "Spracherkennung:" << r;
        emit failed(r);
    });
    m_worker->start();

    m_segmenter.reset();
    m_running = true;
    m_pump->start();
    qCInfo(lcDsp) << "Spracherkennung gestartet";
}

void AsrService::stop()
{
    if (!m_running) { return; }
    m_running = false;
    m_pump->stop();

    if (m_worker) {
        m_worker->quit();
        // Warten, nicht nur bitten: der Arbeiter haelt einen Zeiger auf
        // den Erkenner, und der wird gleich freigegeben. Ohne dieses
        // Warten liefe er weiter auf einem toten Objekt.
        m_worker->wait(5000);
        delete m_worker;
        m_worker = nullptr;
        m_workerObj = nullptr;
    }
    if (m_backend) { m_backend->unload(); }
    m_segmenter.reset();
    if (m_listening) { m_listening = false; emit listeningChanged(false); }
    qCInfo(lcDsp) << "Spracherkennung angehalten";
}

void AsrService::drainRing()
{
    if (!m_ring || !m_running) { return; }
    // Fester Zwischenspeicher: der Zeitgeber laeuft zwanzigmal in der
    // Sekunde, und eine Speicheranforderung je Runde waere Abfall ohne
    // Zweck.
    static thread_local std::vector<float> scratch;
    const int want = m_sourceRate / 2 * 2;   // eine halbe Sekunde, Stereo
    if (static_cast<int>(scratch.size()) < want) { scratch.resize(want); }

    const int got = m_ring->read(scratch.data(), want);
    if (got < 2) { return; }
    feedSamples(scratch.data(), got / 2, m_sourceRate);
}

void AsrService::feedForTest(const float* stereo, int frames, int rateHz)
{
    feedSamples(stereo, frames, rateHz);
}

void AsrService::feedSamples(const float* stereo, int frames, int rateHz)
{
    if (frames <= 0 || !stereo) { return; }

    // ── Stereo 48 kHz -> Mono 16 kHz ────────────────────────────────
    //
    // Whisper und alles, was seinem Format folgt, will 16 kHz Mono.
    // processStereoToMono macht beides in einem Zug — erst mischen,
    // dann umtasten, mit dem Filter, der ohnehin schon im Baum steht.
    if (!m_resampler || int(m_resampler->srcRate()) != rateHz) {
        m_resampler = std::make_unique<Resampler>(double(rateHz),
                                                  double(kAsrRateHz),
                                                  kResampleBlockFrames);
    }

    // ── In Bloecken, nicht am Stueck ────────────────────────────────
    //
    // Der Wandler wird fuer eine HOECHSTZAHL an Rahmen gebaut und darf
    // nicht mehr auf einmal bekommen. Hier stand ein einziger Aufruf
    // mit allem, was ankam — bei einer Sekunde Ton sind das 48 000
    // Rahmen gegen 16 384, fuer die er gebaut war.
    //
    // Der Fehler war nicht sichtbar: es kam Ton heraus, nur die
    // falsche Menge. Die Pruefung tst_asr_end_to_end hat ihn gefunden,
    // weil sie ZAEHLT, was beim Erkenner ankommt — 17 600 Werte, wo
    // 12 800 hingehoert haetten. Whisper haette daraus etwas gemacht,
    // das plausibel klingt und nicht stimmt.
    std::vector<float> mono16k;
    for (int off = 0; off < frames; off += kResampleBlockFrames) {
        const int chunk = qMin(kResampleBlockFrames, frames - off);
        const QByteArray part =
            m_resampler->processStereoToMono(stereo + size_t(off) * 2, chunk);
        if (part.isEmpty()) { continue; }
        const auto* pf = reinterpret_cast<const float*>(part.constData());
        mono16k.insert(mono16k.end(), pf,
                       pf + part.size() / int(sizeof(float)));
    }
    if (mono16k.empty()) { return; }

    const float* f = mono16k.data();
    const int n = int(mono16k.size());

    auto closed = m_segmenter.feed(f, n);
    const bool nowListening = m_segmenter.inSpeech();
    if (nowListening != m_listening) {
        m_listening = nowListening;
        emit listeningChanged(m_listening);
    }
    for (auto& utt : closed) { dispatch(std::move(utt)); }
}

void AsrService::dispatch(std::vector<float>&& utterance)
{
    if (!m_workerObj || utterance.empty()) { return; }
    // Der Rueckgabewert wird GEPRUEFT. invokeMethod meldet einen nicht
    // angemeldeten Typ nur so — und still zu scheitern ist bei einer
    // Erkennung, auf deren Text jemand wartet, das Schlimmste.
    // Ueber die Warteschlange, damit der Aufruf auf dem Arbeiterfaden
    // landet. Die Kopie ist gewollt: der Arbeiter lebt laenger als der
    // Aufruf hier.
    const bool queued = QMetaObject::invokeMethod(
        m_workerObj, "transcribe", Qt::QueuedConnection,
        Q_ARG(std::vector<float>, utterance));
    if (!queued) {
        qCWarning(lcDsp) << "Spracherkennung: Abschnitt konnte nicht an den "
                            "Arbeiterfaden uebergeben werden";
    }
}

} // namespace Longpath

#include "AsrService.moc"
