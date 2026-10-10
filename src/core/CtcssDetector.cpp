// no-port-check: Longpath-original file; siehe den Kopf von
// CtcssDetector.h. Der Verweis auf `console.cs:236-241` unten ist die
// Fundstelle fuer die TIA-603-D-Normtonliste, kein portierter Code.

#include "core/CtcssDetector.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Longpath {

const QVector<double>& ctcssStandardTonesHz()
{
    // Dieselbe Liste wie Thetis `console.cs:236-241 [@852bf0e]`
    // (`CTCSS_array`), die ihrerseits die Normtoene aus TIA-603-D
    // fuehrt. Reihenfolge aufsteigend, wie dort.
    static const QVector<double> kTones = {
        67.0,  69.3,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,
        94.8,  97.4,  100.0, 103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3,
        131.8, 136.5, 141.3, 146.2, 151.4, 156.7, 159.8, 162.2, 165.5, 167.9,
        171.3, 173.8, 177.3, 179.9, 183.5, 186.2, 189.9, 192.8, 199.5, 203.5,
        206.5, 210.7, 218.1, 225.7, 229.1, 233.6, 241.8, 250.3, 254.1};
    return kTones;
}

double nearestCtcssToneHz(double hz)
{
    double best = 0.0;
    double bestDelta = 1.0;  // mehr als 1 Hz daneben gilt als "kein Normton"
    for (double tone : ctcssStandardTonesHz()) {
        const double delta = std::abs(tone - hz);
        if (delta <= bestDelta) {
            bestDelta = delta;
            best = tone;
        }
    }
    return best;
}

void CtcssDetector::designLowpass(Biquad& bq, double cutoffHz, double sampleRate, double q)
{
    // Robert Bristow-Johnson, Audio-EQ-Cookbook, Abschnitt LPF -- die
    // ueblichen Biquad-Koeffizienten, hier nur niedergeschrieben.
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

CtcssDetector::CtcssDetector(double sampleRate, double toneHz)
    : m_sampleRate(sampleRate > 0.0 ? sampleRate : 48000.0)
    , m_toneHz(toneHz)
{
    rebuild();
}

void CtcssDetector::setToneHz(double toneHz)
{
    if (qFuzzyCompare(m_toneHz + 1.0, toneHz + 1.0)) {
        return;
    }
    m_toneHz = toneHz;
    rebuild();
}

void CtcssDetector::setThresholds(double openRatio, double closeRatio)
{
    m_openRatio  = std::clamp(openRatio, 0.01, 1.0);
    // Die Schliessschwelle muss unter der Oeffnungsschwelle liegen,
    // sonst gibt es keine Hysterese, sondern Flattern.
    m_closeRatio = std::clamp(closeRatio, 0.0, m_openRatio * 0.95);
}

void CtcssDetector::rebuild()
{
    // Butterworth 4. Ordnung = zwei Biquads mit diesen Gueten.
    designLowpass(m_lp1, kLowpassHz, m_sampleRate, 0.54119610);
    designLowpass(m_lp2, kLowpassHz, m_sampleRate, 1.30656296);

    m_decimation   = std::max(1, static_cast<int>(std::lround(m_sampleRate / kWorkRateHz)));
    const double workRate = m_sampleRate / static_cast<double>(m_decimation);
    m_windowFrames = std::max(32, static_cast<int>(std::lround(kWindowSeconds * workRate)));

    // Die beiden Nachbarn des Soll-Tons AUS DER NORMLISTE. Steht der
    // Soll-Ton nicht in der Liste (freie Eingabe), nehmen wir die
    // naechstliegenden Normtoene darunter und darueber -- nur die kommen
    // auf dem Band real vor.
    m_belowHz = 0.0;
    m_aboveHz = 0.0;
    for (double t : ctcssStandardTonesHz()) {
        if (t < m_toneHz - 0.05) {
            m_belowHz = t;                       // steigt bis zum letzten darunter
        } else if (t > m_toneHz + 0.05 && m_aboveHz <= 0.0) {
            m_aboveHz = t;                       // der erste darueber
        }
    }

    tuneGoertzel(m_target, m_toneHz, workRate);
    if (m_belowHz > 0.0) { tuneGoertzel(m_below, m_belowHz, workRate); }
    if (m_aboveHz > 0.0) { tuneGoertzel(m_above, m_aboveHz, workRate); }

    reset();
}

void CtcssDetector::tuneGoertzel(Goertzel& g, double hz, double workRate) const
{
    // k auf das Fenster bezogen, nicht gerundet -- der Normton liegt
    // selten genau auf einem Rasterpunkt, und der gerundete Koeffizient
    // kostet bis zu 3 dB Empfindlichkeit.
    const double k = (static_cast<double>(m_windowFrames) * hz) / workRate;
    const double omega = (2.0 * M_PI * k) / static_cast<double>(m_windowFrames);
    g.coeff = 2.0 * std::cos(omega);
    g.clear();
}

void CtcssDetector::reset()
{
    m_lp1.clear();
    m_lp2.clear();
    m_decimSum   = 0.0;
    m_decimCount = 0;
    m_target.clear();
    m_below.clear();
    m_above.clear();
    m_bandPower  = 0.0;
    m_filled     = 0;
    m_detected   = false;
    m_lastRatio  = 0.0;
    m_lastNeighbourMargin = 0.0;
}

bool CtcssDetector::process(const float* samples, int frames)
{
    if (!samples || frames <= 0 || m_toneHz <= 0.0) {
        return false;
    }
    const bool before = m_detected;

    for (int i = 0; i < frames; ++i) {
        // 1. Tiefpass bei 300 Hz, dann dezimieren (Mittelwert ueber
        //    m_decimation Proben). Der Tiefpass muss VOR die Dezimation,
        //    sonst klappen Sprachanteile oberhalb 500 Hz ins Messband
        //    zurueck.
        const double gefiltert = m_lp2.step(m_lp1.step(static_cast<double>(samples[i])));
        m_decimSum += gefiltert;
        if (++m_decimCount < m_decimation) {
            continue;
        }
        const double x = m_decimSum / static_cast<double>(m_decimation);
        m_decimSum   = 0.0;
        m_decimCount = 0;

        // 2. Alle drei Goertzel-Zweige takten und die Bandleistung
        //    mitfuehren.
        m_target.step(x);
        if (m_belowHz > 0.0) { m_below.step(x); }
        if (m_aboveHz > 0.0) { m_above.step(x); }
        m_bandPower += x * x;

        // 3. Fenster voll: entscheiden.
        if (++m_filled >= m_windowFrames) {
            const double tonePower = m_target.power();
            // Goertzel liefert die Leistung des Tons ueber das ganze
            // Fenster; die Bandleistung ist die Summe der Quadrate.
            // Beide auf dieselbe Laenge bezogen ergibt ein Verhaeltnis
            // zwischen 0 und ungefaehr 1 -- bei reinem Ton knapp
            // darunter, bei reinem Rauschen nahe 1/Fensterlaenge.
            const double denom = m_bandPower * static_cast<double>(m_windowFrames) / 2.0;
            m_lastRatio = denom > 0.0 ? std::clamp(tonePower / denom, 0.0, 1.0) : 0.0;

            // Sitzt die Gegenstelle auf einem NACHBARTON? Dann ist dort
            // mehr Leistung als auf dem eingestellten, und das Fenster
            // trennt die beiden nicht (bei 67,0/69,3 nur 2,3 Hz).
            double neighbour = 0.0;
            if (m_belowHz > 0.0) { neighbour = std::max(neighbour, m_below.power()); }
            if (m_aboveHz > 0.0) { neighbour = std::max(neighbour, m_above.power()); }
            // Ohne Nachbarn (Rand der Liste) gilt der Vergleich als
            // bestanden; mit Nachbarn muss der Soll-Zweig sie um
            // kNeighbourMargin uebertreffen.
            m_lastNeighbourMargin = neighbour > 0.0 ? tonePower / neighbour : kNeighbourMargin;
            const bool eigenerTon = m_lastNeighbourMargin >= kNeighbourMargin;

            // Der Nachbarvergleich gilt nur beim OEFFNEN. Beim Halten
            // waere er zu streng: eine laute tiefe Stimme fuellt einen
            // Nachbarzweig zeitweise staerker als den Soll-Zweig, und
            // die Sperre fiele mitten im Durchgang zu, obwohl der Subton
            // unveraendert anliegt (am Pruefstand gesehen, bevor diese
            // Unterscheidung da war). Fuer das Halten zaehlt allein, ob
            // der Soll-Ton noch ueber der Schliess-Schwelle liegt.
            if (!m_detected && m_lastRatio >= m_openRatio && eigenerTon) {
                m_detected = true;
            } else if (m_detected && m_lastRatio < m_closeRatio) {
                m_detected = false;
            }

            m_target.clear();
            m_below.clear();
            m_above.clear();
            m_bandPower = 0.0;
            m_filled = 0;
        }
    }
    return m_detected != before;
}

} // namespace Longpath
