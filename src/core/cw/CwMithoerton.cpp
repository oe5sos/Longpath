// =================================================================
// src/core/cw/CwMithoerton.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: CwMithoerton.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/cw/CwMithoerton.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QLoggingCategory>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>
#include <cstring>

Q_LOGGING_CATEGORY(lcCwTon, "longpath.cw.ton")

namespace Longpath {

// Die Zugquelle fuer QAudioSink: das Geraet holt sich, was es braucht.
class CwMithoerton::Quelle : public QIODevice {
public:
    explicit Quelle(CwMithoerton* ton) : m_ton(ton) {}

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override
    {
        // „Immer genug": ein Tongeber laeuft nie leer.
        return 4096 + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* daten, qint64 hoechstens) override
    {
        const int rahmen = static_cast<int>(hoechstens / qint64(sizeof(float)));
        if (rahmen <= 0) { return 0; }
        m_ton->fuelle(reinterpret_cast<float*>(daten), rahmen);
        return qint64(rahmen) * qint64(sizeof(float));
    }
    qint64 writeData(const char*, qint64) override { return -1; }

private:
    CwMithoerton* m_ton;
};

CwMithoerton::CwMithoerton(QObject* eltern) : QObject(eltern) {}

CwMithoerton::~CwMithoerton()
{
    stop();
}

void CwMithoerton::setTonhoeheHz(int hz) noexcept
{
    m_hz.store(std::clamp(hz, 300, 1200));
}

void CwMithoerton::setLautstaerke(int prozent) noexcept
{
    m_prozent.store(std::clamp(prozent, 0, 100));
}

bool CwMithoerton::start()
{
    if (m_sink) { return true; }
    const QAudioDevice geraet = QMediaDevices::defaultAudioOutput();
    if (geraet.isNull()) {
        qCInfo(lcCwTon) << "Mithoerton: kein Ausgabegeraet — bleibt stumm";
        return false;
    }
    QAudioFormat format;
    format.setSampleRate(kAbtastrate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    if (!geraet.isFormatSupported(format)) {
        qCWarning(lcCwTon) << "Mithoerton: 48 kHz mono Gleitkomma wird vom "
                              "Ausgabegeraet nicht angenommen — bleibt stumm";
        return false;
    }
    m_quelle = std::make_unique<Quelle>(this);
    m_quelle->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    m_sink = std::make_unique<QAudioSink>(geraet, format);
    // Kleiner Puffer: die Flanke faellt auf die naechste Blockgrenze,
    // und die soll nah sein. 256 Proben sind 5,3 ms.
    m_sink->setBufferSize(256 * int(sizeof(float)));
    m_sink->start(m_quelle.get());
    qCInfo(lcCwTon).noquote()
        << "Mithoerton: laeuft an" << geraet.description()
        << "— Puffer" << m_sink->bufferSize() / int(sizeof(float)) << "Proben";
    return true;
}

void CwMithoerton::stop()
{
    m_taste.store(false);
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    if (m_quelle) {
        m_quelle->close();
        m_quelle.reset();
    }
    m_huelle = 0.0;
}

void CwMithoerton::fuelle(float* ziel, int rahmen) noexcept
{
    const bool unten = m_taste.load(std::memory_order_acquire);
    const double amplitude = double(m_prozent.load()) / 100.0;
    const double schritt = 2.0 * M_PI * double(m_hz.load()) / double(kAbtastrate);
    // Die Huellkurve laeuft je Probe um einen festen Schritt auf ihr Ziel
    // zu; die Flanke selbst ist der Kosinus daraus.
    const double jeProbe = 1.0 / (double(kFlankeMs) * double(kAbtastrate) / 1000.0);
    const double zielwert = unten ? 1.0 : 0.0;

    for (int i = 0; i < rahmen; ++i) {
        if (m_huelle < zielwert) { m_huelle = std::min(zielwert, m_huelle + jeProbe); }
        else if (m_huelle > zielwert) { m_huelle = std::max(zielwert, m_huelle - jeProbe); }

        if (m_huelle <= 0.0) {
            ziel[i] = 0.0f;
            // Phase bei Stille anhalten und auf null: jedes Zeichen
            // beginnt gleich, und es laeuft nichts weg.
            m_phase = 0.0;
            continue;
        }
        const double form = 0.5 - 0.5 * std::cos(M_PI * m_huelle);   // Kosinusflanke
        ziel[i] = static_cast<float>(amplitude * form * std::sin(m_phase));
        m_phase += schritt;
        if (m_phase >= 2.0 * M_PI) { m_phase -= 2.0 * M_PI; }
    }
}

} // namespace Longpath
