#include "core/audio/RxRatenAngleich.h"

#include <algorithm>
#include <cmath>

extern "C" {
// Aus third_party/wdsp/src/varsamp.h. Nur die vier Aufrufe, die hier
// gebraucht werden -- den ganzen WDSP-Kopf einzubinden zoege `comm.h`
// und damit halb WDSP in jede Uebersetzungseinheit, die diesen Kopf
// liest.
void* create_varsamp(int run, int size, double* in, double* out,
                     int in_rate, int out_rate, double fc, double fc_low,
                     int R, double gain, double var, int varmode);
void  destroy_varsamp(void* a);
void  flush_varsamp(void* a);
int   xvarsamp(void* a, double var);
}

namespace Longpath {

namespace {

// Dieselben Werte, die rmatch.c:142-143 dem Umtaster gibt
// (create_rmatchV, rmatch.c:500-526 [v2.10.3.13]):
constexpr double kFcHigh  =  0.0;   // 0 = automatisch
constexpr double kFcLow   = -1.0;   // -1 = keine untere Grenze
constexpr double kGain    =  1.0;
constexpr int    kR       = 1024;   // Koeffizientendichte
constexpr int    kVarmode = 1;      // Verhaeltnis je Probe linear fuehren

// Wieviel mehr Rahmen hoechstens herauskommen koennen. Das Verhaeltnis ist
// auf 1,04 gedeckelt (rmatch.c:269); 1,1 ist also reichlich und faengt
// zusaetzlich den Rest auf, den der Umtaster aus seinem inneren Zustand
// nachliefert.
constexpr double kPlatzFaktor = 1.1;

}  // namespace

RxRatenAngleich::RxRatenAngleich(int rate, int kanaele)
    : m_rate(rate > 0 ? rate : 48000)
    , m_kanaele(std::clamp(kanaele, 1, 2))
{
    AudioRateMatcher::Einstellungen e;
    e.nennRateEin = m_rate;
    e.nennRateAus = m_rate;
    m_regler = AudioRateMatcher(e);
}

RxRatenAngleich::~RxRatenAngleich()
{
    if (m_varsamp) { destroy_varsamp(m_varsamp); m_varsamp = nullptr; }
}

void RxRatenAngleich::baueUmtaster(int blockRahmen)
{
    if (m_varsamp) { destroy_varsamp(m_varsamp); m_varsamp = nullptr; }
    m_blockRahmen = blockRahmen;

    // varsamp rechnet KOMPLEX: zwei doubles je Probe. Fuer Stereo faellt
    // das zusammen (links/rechts auf die beiden Teile); bei Mono wird der
    // zweite Teil mitgefuehrt und am Ende verworfen -- das kostet nichts
    // und erspart einen zweiten Rechenweg.
    m_ein.assign(static_cast<size_t>(blockRahmen) * 2, 0.0);
    const int platz = static_cast<int>(std::ceil(blockRahmen * kPlatzFaktor)) + 8;
    m_aus.assign(static_cast<size_t>(platz) * 2, 0.0);
    m_ausFloat.assign(static_cast<size_t>(platz) * 2, 0.0f);

    m_varsamp = create_varsamp(1, blockRahmen, m_ein.data(), m_aus.data(),
                               m_rate, m_rate, kFcHigh, kFcLow, kR, kGain,
                               1.0, kVarmode);
}

RxRatenAngleich::Ausgabe RxRatenAngleich::verarbeite(
    const float* ein, int rahmen, qint64 fuellungRahmen, qint64 ringRahmen)
{
    if (!ein || rahmen <= 0) { return { ein, 0 }; }

    // Unbekannter Fuellstand -> unveraendert durchreichen. Ein Regler ohne
    // Messgroesse ist keiner, und ein Umtaster, der auf Verdacht laeuft,
    // kostet Rechenzeit und Tonqualitaet fuer nichts.
    if (fuellungRahmen < 0 || ringRahmen <= 0) { return { ein, rahmen }; }

    // Dem Regler beide Richtungen melden: erzeugte Rahmen positiv, die vom
    // Geraet verbrauchten negativ. Die verbrauchten liest der Erzeuger am
    // Fuellstand ab -- so muss der Rueckruf nichts mitfuehren und keine
    // Sperre nehmen (ebendas unterscheidet diesen Weg von Thetis).
    if (m_letzteFuellung >= 0) {
        const qint64 verbraucht = m_letzteFuellung + m_letzteAusgabe - fuellungRahmen;
        if (verbraucht > 0) {
            m_regler.melde(-static_cast<int>(std::min<qint64>(verbraucht, 1 << 20)),
                           fuellungRahmen, ringRahmen);
        }
    }
    m_regler.melde(rahmen, fuellungRahmen, ringRahmen);

    if (!m_regler.regeltSchon()) {
        m_letzteFuellung = fuellungRahmen;
        m_letzteAusgabe = rahmen;
        return { ein, rahmen };
    }

    if (!m_varsamp || m_blockRahmen != rahmen) { baueUmtaster(rahmen); }

    for (int i = 0; i < rahmen; ++i) {
        m_ein[static_cast<size_t>(i) * 2 + 0] = ein[static_cast<size_t>(i) * m_kanaele];
        m_ein[static_cast<size_t>(i) * 2 + 1] =
            (m_kanaele == 2) ? ein[static_cast<size_t>(i) * 2 + 1]
                             : ein[static_cast<size_t>(i)];
    }

    const int heraus = xvarsamp(m_varsamp, m_regler.verhaeltnis());
    const int sicher = std::clamp(heraus, 0,
        static_cast<int>(m_ausFloat.size() / 2));
    for (int i = 0; i < sicher; ++i) {
        const float l = static_cast<float>(m_aus[static_cast<size_t>(i) * 2 + 0]);
        if (m_kanaele == 2) {
            m_ausFloat[static_cast<size_t>(i) * 2 + 0] = l;
            m_ausFloat[static_cast<size_t>(i) * 2 + 1] =
                static_cast<float>(m_aus[static_cast<size_t>(i) * 2 + 1]);
        } else {
            m_ausFloat[static_cast<size_t>(i)] = l;
        }
    }

    m_versatz += static_cast<qint64>(sicher) - rahmen;
    m_letzteFuellung = fuellungRahmen;
    m_letzteAusgabe = sicher;
    return { m_ausFloat.data(), sicher };
}

void RxRatenAngleich::zuruecksetzen()
{
    m_regler.zuruecksetzen();
    if (m_varsamp) { flush_varsamp(m_varsamp); }
    m_versatz = 0;
    m_letzteFuellung = -1;
    m_letzteAusgabe = 0;
}

}  // namespace Longpath
