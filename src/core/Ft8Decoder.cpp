// =================================================================
// src/core/Ft8Decoder.cpp  (Longpath)
// =================================================================
// Longpath-original. Siehe Ft8Decoder.h fuer die Herkunft und
// third_party/ft8_lib/FT8LIB-PROVENANCE.md fuer die Bibliothek.
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Decoder.h"

#include <QByteArray>

extern "C" {
#include "common/gfsk.h"
#include "common/monitor.h"
#include "ft8/constants.h"
#include "ft8/encode.h"
#include "ft8/message.h"
}

#include <cmath>
#include <cstring>
#include <vector>

namespace Longpath {
namespace {

// ── Die Zahlen stammen aus dem Vorbild, nicht aus dem Kopf ──────────
//
// ft8_lib demo/decode_ft8.c:21-28 [@9fec6ca]. Sie hier noch einmal zu
// waehlen hiesse, eine andere Empfindlichkeit zu bauen als die, gegen
// die alle Welt misst.
constexpr int kMinScore        = 10;
constexpr int kMaxCandidates   = 140;
constexpr int kLdpcIterations  = 25;
constexpr int kMaxDecoded      = 50;
constexpr int kFreqOsr         = 2;
constexpr int kTimeOsr         = 2;

// Der Durchlassbereich, in dem gesucht wird. Dieselben Grenzen wie im
// Vorbild: unterhalb 200 Hz sitzt nichts Brauchbares, oberhalb 3000 Hz
// schneidet jeder SSB-Filter ab.
constexpr float kFMin = 200.0f;
constexpr float kFMax = 3000.0f;

ftx_protocol_t toFtx(Ft8Decoder::Protocol p)
{
    return p == Ft8Decoder::Protocol::Ft4 ? FTX_PROTOCOL_FT4
                                          : FTX_PROTOCOL_FT8;
}

} // namespace

QVector<Ft8Decode> Ft8Decoder::decodeBlock(const float* samples, int count,
                                           Protocol protocol)
{
    QVector<Ft8Decode> out;
    if (!samples || count <= 0) { return out; }

    monitor_config_t cfg{};
    cfg.f_min       = kFMin;
    cfg.f_max       = kFMax;
    cfg.sample_rate = kSampleRate;
    cfg.time_osr    = kTimeOsr;
    cfg.freq_osr    = kFreqOsr;
    cfg.protocol    = toFtx(protocol);

    monitor_t mon{};
    monitor_init(&mon, &cfg);

    // Blockweise fuettern, genau wie im Vorbild: monitor_process
    // erwartet immer mon.block_size Proben.
    for (int pos = 0; pos + mon.block_size <= count; pos += mon.block_size) {
        monitor_process(&mon, samples + pos);
    }

    const ftx_waterfall_t* wf = &mon.wf;
    std::vector<ftx_candidate_t> kandidaten(kMaxCandidates);
    const int gefunden = ftx_find_candidates(wf, kMaxCandidates,
                                             kandidaten.data(), kMinScore);

    // Doppelte erkennen — dieselbe Nachricht wird oft mehrfach
    // gefunden, einmal je Kandidat. Ohne das stuende sie zehnmal in
    // der Liste.
    std::vector<ftx_message_t> gesehen;
    gesehen.reserve(kMaxDecoded);

    for (int i = 0; i < gefunden && out.size() < kMaxDecoded; ++i) {
        const ftx_candidate_t* kand = &kandidaten[static_cast<size_t>(i)];

        ftx_message_t nachricht{};
        ftx_decode_status_t status{};
        if (!ftx_decode_candidate(wf, kand, kLdpcIterations, &nachricht,
                                  &status)) {
            continue;   // LDPC oder CRC — kein Fund, kein Fehler
        }

        bool schonDa = false;
        for (const ftx_message_t& alt : gesehen) {
            if (alt.hash == nachricht.hash
                && std::memcmp(alt.payload, nachricht.payload,
                               sizeof(nachricht.payload)) == 0) {
                schonDa = true;
                break;
            }
        }
        if (schonDa) { continue; }
        gesehen.push_back(nachricht);

        char text[FTX_MAX_MESSAGE_LENGTH] = {0};
        ftx_message_offsets_t offsets{};
        if (ftx_message_decode(&nachricht, nullptr, text, &offsets)
                != FTX_MESSAGE_RC_OK) {
            continue;   // entpackt nicht — dann steht auch nichts da
        }

        Ft8Decode d;
        d.score     = kand->score;
        // Dieselbe grobe Schaetzung wie im Vorbild. Eine bessere waere
        // eine eigene Arbeit; eine ERFUNDENE waere schlimmer als eine
        // grobe.
        d.snrDb     = static_cast<float>(kand->score) * 0.5f;
        d.freqHz    = (mon.min_bin + kand->freq_offset
                       + static_cast<float>(kand->freq_sub) / wf->freq_osr)
                      / mon.symbol_period;
        d.dtSeconds = (kand->time_offset
                       + static_cast<float>(kand->time_sub) / wf->time_osr)
                      * mon.symbol_period;
        d.text      = QString::fromLatin1(text).trimmed();
        if (!d.text.isEmpty()) { out.append(d); }
    }

    monitor_free(&mon);
    return out;
}

bool Ft8Decoder::encodeBlock(const QString& text, float toneHz,
                             Protocol protocol, QVector<float>* samples)
{
    if (!samples) { return false; }

    const QByteArray roh = text.toLatin1();
    ftx_message_t nachricht{};
    ftx_message_init(&nachricht);
    if (ftx_message_encode(&nachricht, nullptr, roh.constData())
            != FTX_MESSAGE_RC_OK) {
        return false;
    }

    const bool ft4 = (protocol == Protocol::Ft4);
    const int toene = ft4 ? FT4_NN : FT8_NN;
    std::vector<uint8_t> symbole(static_cast<size_t>(toene), 0);
    if (ft4) { ft4_encode(nachricht.payload, symbole.data()); }
    else     { ft8_encode(nachricht.payload, symbole.data()); }

    // Masse aus ft8_lib demo/gen_ft8.c:160-186 [@9fec6ca].
    const float symbolPeriod = ft4 ? FT4_SYMBOL_PERIOD : FT8_SYMBOL_PERIOD;
    const float symbolBt     = ft4 ? 1.0f : 2.0f;
    const float slotZeit     = ft4 ? 7.5f : 15.0f;

    const int nutz  = static_cast<int>(0.5f + toene * symbolPeriod * kSampleRate);
    const int ganz  = static_cast<int>(slotZeit * kSampleRate);
    const int stille = (ganz - nutz) / 2;

    samples->assign(ganz, 0.0f);
    synth_gfsk(symbole.data(), toene, toneHz, symbolBt, symbolPeriod,
               kSampleRate, samples->data() + stille);
    return true;
}

} // namespace Longpath
