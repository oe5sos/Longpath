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

#include <QStringList>

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

QVector<float> Ft8TxSource::tonleiter(float grundTonHz, int anzahl)
{
    // MSHV gen_ft8.cpp:262-264 [GPLv3, LZ2HV]:
    //   double f0 = f_tx + (60.0*(double)islot);
    //   if (f0<100.0)  f0=100.0;
    //   if (f0>5000.0) f0=5000.0;
    QVector<float> aus;
    if (anzahl <= 0) { return aus; }
    aus.reserve(anzahl);
    for (int i = 0; i < anzahl; ++i) {
        float f0 = grundTonHz + kPlatzAbstandHz * static_cast<float>(i);
        if (f0 < kMinTonHz) { f0 = kMinTonHz; }
        if (f0 > kMaxTonHz) { f0 = kMaxTonHz; }
        aus.append(f0);
    }
    return aus;
}

bool Ft8TxSource::vorbereitenMehrere(const QVector<Platz>& plaetze,
                                     Ft8Decoder::Protocol protocol)
{
    if (plaetze.isEmpty()) { return false; }
    if (plaetze.size() == 1) {
        return vorbereiten(plaetze.first().text, plaetze.first().toneHz, protocol);
    }

    abbrechen();

    // Jede Nachricht fuer sich erzeugen, dann summieren. Erst ALLE
    // rechnen und dann erst den Zustand anfassen: schlaegt die dritte
    // fehl, soll nicht ein halbes Fenster stehenbleiben.
    QVector<QVector<float>> einzeln;
    einzeln.reserve(plaetze.size());
    int laenge = 0;
    QStringList texte;
    for (const Platz& p : plaetze) {
        QVector<float> bei12k;
        if (!Ft8Decoder::encodeBlock(p.text, p.toneHz, protocol, &bei12k)) {
            return false;
        }
        laenge = std::max(laenge, static_cast<int>(bei12k.size()));
        einzeln.append(std::move(bei12k));
        texte << p.text;
    }

    // Summieren und durch die Zahl der Plaetze teilen -- MSHV
    // gen_ft8.cpp:290 (`d_iwave[k1] += sin(phi)`) und :303
    // (`d_iwave[z]/(double)nslots`). Die Teilung ist kein Geschmack:
    // der Sender hat eine Hoechstamplitude, und ohne sie uebersteuert
    // jedes Fenster mit mehr als einer Station.
    QVector<float> summe(laenge, 0.0f);
    for (const QVector<float>& eine : einzeln) {
        for (int i = 0; i < eine.size(); ++i) { summe[i] += eine[i]; }
    }
    const float teiler = static_cast<float>(einzeln.size());
    for (int i = 0; i < laenge; ++i) { summe[i] /= teiler; }

    if (!m_umtaster) {
        m_umtaster = std::make_unique<Resampler>(
            double(Ft8Decoder::kSampleRate), double(m_micRateHz), kBlock);
    }

    QVector<float> aus;
    aus.reserve(summe.size() * (m_micRateHz / Ft8Decoder::kSampleRate + 1) + 64);
    QVector<float> puffer(kBlock * 8 + 64);
    for (int pos = 0; pos < summe.size(); pos += kBlock) {
        const int n = std::min<int>(kBlock, summe.size() - pos);
        const int geschrieben = m_umtaster->processInto(
            summe.constData() + pos, n, puffer.data(), puffer.size());
        for (int i = 0; i < geschrieben; ++i) { aus.append(puffer[i]); }
    }

    m_proben = std::move(aus);
    // Die Anzeige soll sehen, dass mehrere drin sind -- MSHV trennt sie
    // im eigenen Quelltext ebenfalls mit „#" (gen_ft8.cpp:209).
    m_text = texte.join(QLatin1Char('#'));
    m_pos.store(0, std::memory_order_release);
    return !m_proben.isEmpty();
}

void Ft8TxSource::starten(qint64 abMs)
{
    if (m_proben.isEmpty()) { return; }
    // Probenposition, nicht Rahmenposition: die Quelle ist mono.
    const qint64 ab = (abMs * m_micRateHz) / 1000;
    const int start = static_cast<int>(
        qBound<qint64>(0, ab, qint64(m_proben.size())));
    m_pos.store(start, std::memory_order_release);
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
