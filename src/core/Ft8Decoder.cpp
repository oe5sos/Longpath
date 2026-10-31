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
#include "core/Ft8Rufzeichen.h"
#include "core/Ft8Subtract.h"

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
#include <algorithm>
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

namespace {

/// Was eine Runde zum Abziehen hinterlaesst: die Toene des Fundes und
/// wo er lag. Mehr braucht Ft8Subtract nicht.
struct Abzug {
    std::vector<uint8_t> toene;
    double f0Hz{0.0};
    double dtSekunden{0.0};
};

// ── Der Stoerabstand, und warum er hier selbst gerechnet wird ────
//
// Bis zum 2026-10-31 stand in der Spalte `kand->score * 0.5f` — wie
// im Demoprogramm von ft8_lib. Das ist der SYNC-SCORE
// (`ft8_sync_score`, decode.c:61): die gemittelte Magnitudendifferenz
// zwischen den Costas-Toenen und ihren Nachbarn, in halben dB. Ein
// Kontrastmass, kein Stoerabstand — und weil Kandidaten unter
// `kMinScore` vorher wegfallen, ist es nie negativ.
//
// Der Betreiber hat es am Geraet gesehen: „bei der spalte sehe ich
// DB. aber nur posivitv, keine negativen DB" — bei einer Zeile mit
// `19` in der Spalte und `-11` im Text derselben Zeile. Echte
// FT8-Stoerabstaende liegen etwa zwischen −24 und +15 dB und sind
// meistens negativ; mit dieser Zahl laesst sich nicht entscheiden, ob
// ein Anruf lohnt.
//
// Gerechnet wird aus demselben Wasserfall, den der Dekoder ohnehin
// aufbaut (`mag[Block][time_osr][freq_osr][Bin]`, Leistung in dB,
// `monitor.c:198`: `10*log10(mag2)`). Je Symbolblock stehen die acht
// Tonbins nebeneinander (vier bei FT4); genau EINES traegt den Ton,
// die anderen tragen Rauschen. Also:
//
//   Signal+Rauschen = das staerkste der N Bins
//   Rauschen        = Mittel der uebrigen N−1
//   Signal          = Differenz
//
// Das braucht die Tonfolge NICHT — bei einem Fund, der LDPC und CRC
// bestanden hat, ist das staerkste Bin praktisch immer der Ton. So
// gilt dieselbe Rechnung fuer FT8 (8 Toene) und FT4 (4 Toene).
//
// Der Bezug ist 2500 Hz wie bei WSJT-X. Ein Bin ist hier
// 12000/3840 = 3,125 Hz breit (`nfft = block_size * freq_osr`,
// monitor.c:62), mit Hann-Fenster also 1,5 Bins Rauschbandbreite =
// 4,6875 Hz; das macht 10*log10(4,6875/2500) = −27,3 dB.
//
// Die Zahl ist trotzdem GEMESSEN und nicht hergeleitet: die
// Herleitung gibt −27,3 dB, die Messung −28,2. Die Differenz ist
// die Luecke zwischen der Lehrbuch-Rauschbandbreite und dem, was
// Fensterung, Ueberlappung und die Spitzenbildung ueber die Bloecke
// wirklich tun. `tests/tst_ft8_stoerabstand.cpp` erzeugt ein Signal
// mit bekanntem Stoerabstand und rechnet nach; steht dort etwas
// anderes als hier, gilt die Messung.
//
// Gemessen am 2026-10-31 mit diesem Bezugswert:
//
//     Ziel   0 dB -> gemeldet  −0,3
//     Ziel  −5 dB -> gemeldet  −5,2
//     Ziel −10 dB -> gemeldet −10,1
//     Ziel −15 dB -> gemeldet −15,0
//     Ziel −18 dB -> gemeldet −17,7
//     Ziel −19 dB -> gemeldet −18,6
//
// Groesster Fehler 0,4 dB ueber 19 dB Spanne.
constexpr float kBezugDb = -28.2f;

/// Untergrenze. Ein Fund, dessen Rechnung nicht aufgeht (Rauschen
/// null, Bloecke ausserhalb des Wasserfalls), bekommt sie statt einer
/// erfundenen Zahl.
constexpr float kSnrUntergrenze = -30.0f;

float stoerabstandDb(const ftx_waterfall_t* wf,
                     const ftx_candidate_t* kand,
                     Ft8Decoder::Protocol protocol)
{
    if (!wf || !wf->mag || !kand) { return kSnrUntergrenze; }

    const bool ft4    = (protocol == Ft8Decoder::Protocol::Ft4);
    const int  toene  = ft4 ? 4 : 8;
    const int  symbole = ft4 ? FT4_NN : FT8_NN;

    const int seite = kand->time_sub * (wf->freq_osr * wf->num_bins)
                    + kand->freq_sub * wf->num_bins;

    auto leistung = [&](int block, int bin) {
        const float db =
            WF_ELEM_MAG(wf->mag[block * wf->block_stride + seite + bin]);
        return std::pow(10.0, double(db) / 10.0);
    };

    // ── Warum das Rauschen NICHT aus den Nachbartoenen kommt ─────
    //
    // Der erste Versuch nahm je Block das staerkste der N Tonbins als
    // Signal und das Mittel der uebrigen als Rauschen. Gegen bekannte
    // Signale gemessen (2026-10-31) staucht das nach oben hin:
    //
    //     Ziel −18 dB -> gemeldet −18,6   (gut)
    //     Ziel −15 dB -> gemeldet −16,3   (gut)
    //     Ziel −10 dB -> gemeldet −12,8   (schon daneben)
    //     Ziel  −5 dB -> gemeldet −10,4   (5,4 dB zu wenig)
    //
    // Der Grund ist Leckstrahlung: je staerker der Ton, desto mehr
    // davon steht in seinen Nachbarbins (Hann-Nebenzipfel, und der Ton
    // liegt nicht genau auf der Binmitte). Was als Rauschen gemessen
    // wird, ist dann zum Teil das Signal selbst — und der Quotient
    // wird zu klein.
    //
    // Darum kommt das Rauschen aus einem ABSTANDSBEREICH neben dem
    // Signal, mit zwei Bins Schutzabstand, und als MEDIAN: liegt dort
    // eine andere Station, zieht sie den Mittelwert hoch, den Median
    // nicht.
    constexpr int kSchutz  = 2;    ///< Bins Abstand zum aeussersten Ton
    constexpr int kBreite  = 12;   ///< so viele Bins je Seite
    std::vector<double> rauschproben;
    rauschproben.reserve(static_cast<size_t>(symbole) * 2 * kBreite);

    double summeSpitze = 0.0;
    int    bloecke     = 0;

    for (int k = 0; k < symbole; ++k) {
        const int block = kand->time_offset + k;
        if (block < 0 || block >= wf->num_blocks) { continue; }

        double spitze = -1.0;
        bool   gut    = true;
        for (int t = 0; t < toene; ++t) {
            const int bin = kand->freq_offset + t;
            if (bin < 0 || bin >= wf->num_bins) { gut = false; break; }
            const double p = leistung(block, bin);
            if (p > spitze) { spitze = p; }
        }
        if (!gut) { continue; }

        for (int i = 0; i < kBreite; ++i) {
            const int links  = kand->freq_offset - kSchutz - 1 - i;
            const int rechts = kand->freq_offset + toene - 1 + kSchutz + 1 + i;
            if (links >= 0) {
                rauschproben.push_back(leistung(block, links));
            }
            if (rechts < wf->num_bins) {
                rauschproben.push_back(leistung(block, rechts));
            }
        }

        summeSpitze += spitze;
        ++bloecke;
    }

    if (bloecke == 0 || rauschproben.empty()) { return kSnrUntergrenze; }

    const size_t mitte = rauschproben.size() / 2;
    std::nth_element(rauschproben.begin(), rauschproben.begin() + mitte,
                     rauschproben.end());

    const double mitSignal = summeSpitze / double(bloecke);
    const double rauschen  = rauschproben[mitte];
    const double signal    = mitSignal - rauschen;
    if (signal <= 0.0 || rauschen <= 0.0) { return kSnrUntergrenze; }

    const double jeBin = 10.0 * std::log10(signal / rauschen);
    const float  snr   = float(jeBin) + kBezugDb;
    return snr < kSnrUntergrenze ? kSnrUntergrenze : snr;
}

/// Eine Runde ueber das Fenster. Das war bis zum 2026-10-21 der ganze
/// Dekoder; jetzt ist es die innere Runde von decodeBlock().
QVector<Ft8Decode> eineRunde(const float* samples, int count,
                             Ft8Decoder::Protocol protocol,
                             std::vector<Abzug>* abzuege)
{
    QVector<Ft8Decode> out;
    if (!samples || count <= 0) { return out; }

    monitor_config_t cfg{};
    cfg.f_min       = kFMin;
    cfg.f_max       = kFMax;
    cfg.sample_rate = Ft8Decoder::kSampleRate;
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
        // Die Rufzeichen-Hashtabelle. Hier stand `nullptr`, und damit
        // kam jedes gehashte Rufzeichen als `<...>` heraus — siehe
        // Ft8Rufzeichen.h.
        if (ftx_message_decode(
                &nachricht,
                static_cast<ftx_callsign_hash_interface_t*>(
                    ft8RufzeichenSchnittstelle()),
                text, &offsets) != FTX_MESSAGE_RC_OK) {
            continue;   // entpackt nicht — dann steht auch nichts da
        }

        Ft8Decode d;
        d.score     = kand->score;
        // Nicht mehr `score * 0.5f` — das war der Sync-Score, kein
        // Stoerabstand. Siehe `stoerabstandDb` oben.
        d.snrDb     = stoerabstandDb(wf, kand, protocol);
        d.freqHz    = (mon.min_bin + kand->freq_offset
                       + static_cast<float>(kand->freq_sub) / wf->freq_osr)
                      / mon.symbol_period;
        d.dtSeconds = (kand->time_offset
                       + static_cast<float>(kand->time_sub) / wf->time_osr)
                      * mon.symbol_period;
        d.text      = QString::fromLatin1(text).trimmed();
        if (d.text.isEmpty()) { continue; }
        out.append(d);

        // Fuer den naechsten Durchgang: dieselbe Nachricht wieder in
        // Toene zuruecksetzen. Das ist die Rueckrichtung, die wir fuer
        // das Senden ohnehin schon haben -- hier dient sie dazu, das
        // gefundene Signal aus der Aufnahme herauszurechnen.
        if (abzuege != nullptr && protocol == Ft8Decoder::Protocol::Ft8) {
            Abzug ab;
            ab.toene.assign(FT8_NN, 0);
            ft8_encode(nachricht.payload, ab.toene.data());
            ab.f0Hz       = double(d.freqHz);
            ab.dtSekunden = double(d.dtSeconds);
            abzuege->push_back(std::move(ab));
        }
    }

    monitor_free(&mon);
    return out;
}

} // namespace

// ── Mehrere Durchgaenge mit Subtraktion ──────────────────────────────
//
// Warum ueberhaupt: WSJT-X und JTDX gehen MEHRFACH ueber dasselbe
// Fenster und ziehen nach jedem Durchgang die gefundenen Signale ab.
// Darunter werden schwaechere Stationen ueberhaupt erst auffindbar --
// der Unterschied zwischen zwoelf und fuenfundzwanzig Stationen je
// Durchgang. `ft8_lib` kann das von sich aus nicht; das Verfahren
// kommt aus MSHV (siehe core/Ft8Subtract.h).
//
// Belegt in tests/tst_ft8_subtraktion: zwei Stationen auf DERSELBEN
// Tonlage, die schwaechere 12 dB darunter. Vor dem Abziehen findet der
// Dekoder nur die starke, danach die schwache.
QVector<Ft8Decode> Ft8Decoder::decodeBlock(const float* samples, int count,
                                           Protocol protocol,
                                           int durchgaenge)
{
    std::vector<Abzug> abzuege;
    QVector<Ft8Decode> out = eineRunde(samples, count, protocol, &abzuege);

    // FT4 bleibt einrundig: Ft8Subtract rechnet mit 79 Symbolen zu
    // 1920 Proben, und das ist FT8. Lieber eine Runde als ein falsch
    // abgezogenes Signal -- das erzeugte eine Stoerung, wo vorher ein
    // echtes Signal stand.
    if (protocol != Protocol::Ft8 || durchgaenge <= 1 || abzuege.empty()) {
        return out;
    }
    if (count < 15 * kSampleRate) { return out; }

    // Ab hier auf einer KOPIE: der Aufrufer hat uns seine Aufnahme nur
    // geliehen, und der Abzug veraendert sie.
    std::vector<float> arbeit(samples, samples + count);

    for (int runde = 2; runde <= durchgaenge; ++runde) {
        bool etwasAbgezogen = false;
        for (const Abzug& ab : abzuege) {
            if (Ft8Subtract::zieheAb(arbeit.data(), int(arbeit.size()),
                                     ab.toene.data(), int(ab.toene.size()),
                                     ab.f0Hz, ab.dtSekunden)) {
                etwasAbgezogen = true;
            }
        }
        if (!etwasAbgezogen) { break; }

        abzuege.clear();
        const QVector<Ft8Decode> weitere =
            eineRunde(arbeit.data(), int(arbeit.size()), protocol, &abzuege);
        if (weitere.isEmpty()) { break; }

        // Nur das Neue anhaengen. Dieselbe Nachricht faellt in beiden
        // Runden an, wenn der Abzug sie nicht ganz erwischt hat.
        int neue = 0;
        for (const Ft8Decode& d : weitere) {
            bool schonDa = false;
            for (const Ft8Decode& alt : out) {
                if (alt.text == d.text) { schonDa = true; break; }
            }
            if (!schonDa) { out.append(d); ++neue; }
        }
        if (neue == 0) { break; }   // nichts Neues mehr zu holen
    }
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
