// =================================================================
// src/core/Ft8Tx.cpp  (Longpath)
// =================================================================
// Longpath-original. Siehe Ft8Tx.h.
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Tx.h"

#include "core/Resampler.h"

#include <algorithm>
#include <cstring>

namespace Longpath {

namespace {
constexpr int kBlock = 4096;
} // namespace

Ft8TxSource::Ft8TxSource(int micRateHz)
    : m_micRateHz(micRateHz)
{
}

Ft8TxSource::~Ft8TxSource() = default;

bool Ft8TxSource::vorbereiten(const QString& text,
                              float toneHz,
                              Ft8Decoder::Protocol protocol)
{
    // Erst aufhoeren, dann umbauen: sonst liest der Tonfaden mitten im
    // Austausch aus dem alten Feld.
    abbrechen();

    QVector<float> bei12k;
    if (!Ft8Decoder::encodeBlock(text, toneHz, protocol, &bei12k)) {
        return false;
    }

    // 12 kHz -> Mikrofonrate. Der Umtaster wird hier gebaut, nicht im
    // Tonfaden; `processInto` darunter rechnet ohne neue Anforderung.
    if (!m_umtaster) {
        m_umtaster = std::make_unique<Resampler>(
            double(Ft8Decoder::kSampleRate), double(m_micRateHz), kBlock);
    }

    QVector<float> aus;
    aus.reserve(bei12k.size() * (m_micRateHz / Ft8Decoder::kSampleRate + 1) + 64);
    QVector<float> puffer(kBlock * 8 + 64);
    for (int pos = 0; pos < bei12k.size(); pos += kBlock) {
        const int n = std::min<int>(kBlock, bei12k.size() - pos);
        const int geschrieben = m_umtaster->processInto(
            bei12k.constData() + pos, n, puffer.data(), puffer.size());
        for (int i = 0; i < geschrieben; ++i) { aus.append(puffer[i]); }
    }

    m_proben = std::move(aus);
    m_text = text;
    m_pos.store(0, std::memory_order_release);
    return !m_proben.isEmpty();
}

void Ft8TxSource::starten()
{
    if (m_proben.isEmpty()) { return; }
    m_pos.store(0, std::memory_order_release);
    m_laeuft.store(true, std::memory_order_release);
}

void Ft8TxSource::abbrechen()
{
    m_laeuft.store(false, std::memory_order_release);
    m_pos.store(0, std::memory_order_release);
}

int Ft8TxSource::pullSamples(float* dst, int n)
{
    if (!dst || n <= 0) { return 0; }

    if (!m_laeuft.load(std::memory_order_acquire)) {
        std::memset(dst, 0, size_t(n) * sizeof(float));
        return n;
    }

    const int gesamt = m_proben.size();
    int pos = m_pos.load(std::memory_order_acquire);
    const int da = std::max<int>(0, std::min<int>(n, gesamt - pos));

    if (da > 0) {
        std::memcpy(dst, m_proben.constData() + pos, size_t(da) * sizeof(float));
        pos += da;
        m_pos.store(pos, std::memory_order_release);
    }
    if (da < n) {
        // Fenster zu Ende: den Rest still fuellen und aufhoeren. Der
        // Sender bleibt getastet, bis ihn jemand abschaltet — das ist
        // mit Absicht nicht Sache dieser Klasse.
        std::memset(dst + da, 0, size_t(n - da) * sizeof(float));
        m_laeuft.store(false, std::memory_order_release);
    }
    return n;
}

qint64 Ft8Tx::bisZumNaechsten(qint64 utcMs,
                              Ft8Decoder::Protocol p,
                              Ft8Periode periode)
{
    const qint64 fenster = fensterMs(p);
    if (istSendefenster(utcMs, p, periode) && (utcMs % fenster) == 0) {
        return 0;
    }
    // Hoechstens zwei Fenster weiter: eines ist immer das richtige.
    const qint64 beginn = (utcMs / fenster) * fenster;
    for (int i = 1; i <= 2; ++i) {
        const qint64 kandidat = beginn + i * fenster;
        if (istSendefenster(kandidat, p, periode)) { return kandidat - utcMs; }
    }
    return 0;
}

} // namespace Longpath
