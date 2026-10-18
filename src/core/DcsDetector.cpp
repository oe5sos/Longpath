// no-port-check: Longpath-original file; siehe den Kopf von DcsDetector.h.

#include "core/DcsDetector.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
/// Grenzfrequenz des Tiefpasses vor der Bitentscheidung.
constexpr double kLowpassHz = 300.0;
/// Zeitkonstante der Gleichanteil-Nachfuehrung, in Wortlaengen.
constexpr double kDcWords = 4.0;
} // namespace

void DcsDetector::designLowpass(Biquad& bq, double cutoffHz, double sampleRate, double q)
{
    const double w0    = 2.0 * M_PI * cutoffHz / sampleRate;
    const double cosw  = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0    = 1.0 + alpha;
    bq.b0 = ((1.0 - cosw) / 2.0) / a0;
    bq.b1 = (1.0 - cosw) / a0;
    bq.b2 = bq.b0;
    bq.a1 = (-2.0 * cosw) / a0;
    bq.a2 = (1.0 - alpha) / a0;
    bq.clear();
}

DcsDetector::DcsDetector(double sampleRate, int oktal, DcsPolarity polarity)
    : m_sampleRate(sampleRate > 0.0 ? sampleRate : 48000.0)
    , m_oktal(oktal)
    , m_polarity(polarity)
{
    rebuild();
}

void DcsDetector::setCode(int oktal, DcsPolarity polarity)
{
    if (m_oktal == oktal && m_polarity == polarity) {
        return;
    }
    m_oktal    = oktal;
    m_polarity = polarity;
    rebuild();
}

void DcsDetector::setThresholdBits(int bits)
{
    // Unter 17 kaeme ein fremder Code durch (gemessen: hoechstens 16 von
    // 23 stimmen ueberein), mehr als 23 gibt es nicht.
    m_schwelle = std::clamp(bits, 17, kDcsWordBits);
}

void DcsDetector::rebuild()
{
    m_erwartet       = dcsExpectedWord(m_oktal, DcsPolarity::Normal);
    m_erwartetInvers = (~m_erwartet) & 0x7FFFFF;

    designLowpass(m_lp1, kLowpassHz, m_sampleRate, 0.54119610);
    designLowpass(m_lp2, kLowpassHz, m_sampleRate, 1.30656296);

    m_phaseStep = kDcsBitRateHz / m_sampleRate;

    // Gleichanteil ueber kDcWords Wortlaengen nachfuehren.
    const double wortProben = m_sampleRate * kDcsWordBits / kDcsBitRateHz;
    m_dcAlpha = std::exp(-1.0 / (kDcWords * wortProben));

    reset();
}

void DcsDetector::reset()
{
    m_lp1.clear();
    m_lp2.clear();
    m_dc = 0.0;
    m_phase = 0.0;
    m_bitSumme = 0.0;
    m_bitProben = 0;
    m_vorigeProbe = 0.0;
    m_schieberegister = 0;
    m_bitsGesehen = 0;
    m_bitZaehler = 0;
    m_letzterTreffer = -1;
    m_folgeTreffer = 0;
    m_detected = false;
    m_lastAgreement = 0;
}

bool DcsDetector::process(const float* samples, int frames)
{
    if (!samples || frames <= 0 || m_oktal <= 0) {
        return false;
    }
    const bool vorher = m_detected;

    for (int i = 0; i < frames; ++i) {
        // 1. Tiefpass: der Datenstrom liegt unter 134 Hz.
        const double x = m_lp2.step(m_lp1.step(static_cast<double>(samples[i])));

        // 2. Gleichanteil abziehen. Ein NRZ-Strom ist ueber 23 Bit nicht
        //    ausgeglichen; ohne das wandert die Entscheidungsschwelle
        //    mit dem Pegel und die Bits kippen reihenweise.
        m_dc = m_dcAlpha * m_dc + (1.0 - m_dcAlpha) * x;
        const double y = x - m_dc;

        // 3. Bittakt nachziehen. Ein Nulldurchgang sollte auf einer
        //    Bitgrenze liegen -- liegt er daneben, die Phase sanft
        //    dorthin schieben. Sanft, weil ein harter Sprung bei
        //    Rauschen den Takt zerreisst.
        const bool nulldurchgang = (y >= 0.0) != (m_vorigeProbe >= 0.0);
        m_vorigeProbe = y;
        if (nulldurchgang) {
            const double abweichung = (m_phase < 0.5) ? -m_phase : (1.0 - m_phase);
            m_phase += 0.10 * abweichung;
            if (m_phase < 0.0) { m_phase += 1.0; }
        }

        // 4. Ueber das Bit mitteln, am Bitende entscheiden.
        m_bitSumme += y;
        ++m_bitProben;
        m_phase += m_phaseStep;
        if (m_phase < 1.0) {
            continue;
        }
        m_phase -= 1.0;

        const double mittel = m_bitProben > 0 ? m_bitSumme / m_bitProben : 0.0;
        m_bitSumme  = 0.0;
        m_bitProben = 0;

        // Das zuerst gesendete Bit ist das niederwertigste, also von
        // oben hereinschieben.
        m_schieberegister = (m_schieberegister >> 1) & 0x3FFFFF;
        if (mittel > 0.0) {
            m_schieberegister |= 1u << (kDcsWordBits - 1);
        }
        if (m_bitsGesehen < kDcsWordBits) {
            ++m_bitsGesehen;
            continue;                      // noch kein volles Wort gesehen
        }

        // 5. Gegen das erwartete Wort korrelieren. KEINE Schleife ueber
        //    die Lagen: dieser Vergleich laeuft bei jedem Bit, also
        //    kommen alle 23 Phasen ohnehin nacheinander vorbei. Eine
        //    Lagenschleife wuerde nur die Gelegenheiten zum Fehltreffer
        //    verdreiundzwanzigfachen (siehe den Kopf).
        ++m_bitZaehler;
        int beste = 0;
        if (m_polarity != DcsPolarity::Inverted) {
            beste = std::max(beste, dcsAgreement(m_schieberegister, m_erwartet));
        }
        if (m_polarity != DcsPolarity::Normal) {
            beste = std::max(beste, dcsAgreement(m_schieberegister, m_erwartetInvers));
        }
        m_lastAgreement = beste;

        if (beste >= m_schwelle) {
            // Ein echter Sender wiederholt dasselbe Wort endlos -- der
            // naechste Treffer muss also genau eine Wortlaenge spaeter
            // kommen. Alles andere ist Zufall und zaehlt von vorne.
            if (m_letzterTreffer >= 0
                && (m_bitZaehler - m_letzterTreffer) == kDcsWordBits) {
                ++m_folgeTreffer;
            } else {
                m_folgeTreffer = 1;
            }
            m_letzterTreffer = m_bitZaehler;
            if (m_folgeTreffer >= kRequiredWords) {
                m_detected = true;
            }
        } else if (m_letzterTreffer >= 0
                   && (m_bitZaehler - m_letzterTreffer) > kHoldWords * kDcsWordBits) {
            // So lange kein Treffer mehr -- der Sender ist weg.
            m_detected = false;
            m_folgeTreffer = 0;
            m_letzterTreffer = -1;
        }
    }
    return m_detected != vorher;
}

} // namespace Longpath
