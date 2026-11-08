// no-port-check: Longpath-original, keine Thetis-Logik.
//
// src/gui/spectrum/Durchlass.cpp  (Longpath) -- siehe Durchlass.h.
//
// Modification history (Longpath):
//   2026-11-08 — Neu, Martin Fischer (OE5SOS), AI-assisted via
//                Anthropic Claude Code.

#include "Durchlass.h"

#include <algorithm>
#include <cmath>

namespace Longpath::Durchlass {

QString bandbreiteText(int loHz, int hiHz)
{
    const int b = std::abs(hiHz - loHz);
    if (b < 1000) {
        return QStringLiteral("%1 Hz").arg(b);
    }
    return QStringLiteral("%1 kHz").arg(b / 1000.0, 0, 'f', 2);
}

QString seitenband(int loHz, int hiHz)
{
    if (loHz >= 0 && hiHz > 0) { return QStringLiteral("USB"); }
    if (hiHz <= 0 && loHz < 0) { return QStringLiteral("LSB"); }
    return QString();
}

QString bandbreiteKurz(int loHz, int hiHz)
{
    const int b = std::abs(hiHz - loHz);
    const QString zahl = (b < 1000)
        ? QString::number(b)
        : QStringLiteral("%1k").arg(b / 1000.0, 0, 'f', 1);
    const QString sb = seitenband(loHz, hiHz);
    return sb.isEmpty() ? zahl : sb + QLatin1Char(' ') + zahl;
}

int raste(int hz, int rastHz)
{
    if (rastHz <= 1) { return hz; }
    return static_cast<int>(std::lround(static_cast<double>(hz) / rastHz)) * rastHz;
}

float rauschboden(const QVector<float>& px)
{
    if (px.isEmpty()) { return 0.0f; }
    QVector<float> s = px;
    const auto mitte = s.begin() + s.size() / 2;
    std::nth_element(s.begin(), mitte, s.end());
    return *mitte;
}

QVector<Signal> erkenneSignale(const QVector<float>& px, double hzJePx,
                               float bodenDbm, float minSnrDb, int brueckePx)
{
    QVector<Signal> raus;
    const int n = px.size();
    const float schwelle = bodenDbm + minSnrDb;
    int i = 0;
    while (i < n) {
        if (px[i] <= schwelle) { ++i; continue; }
        const int a = i;
        int letzterDrueber = i;
        // Weiter, solange ein Punkt ueber der Schwelle in Reichweite der
        // Bruecke liegt.
        while (i < n) {
            if (px[i] > schwelle) {
                letzterDrueber = i;
            } else if (i - letzterDrueber > brueckePx) {
                break;
            }
            ++i;
        }
        const int e = letzterDrueber + 1;
        Signal s;
        s.a = a;
        s.e = e;
        double summe = 0.0, moment = 0.0;
        float spitze = px[a];
        for (int k = a; k < e; ++k) {
            const double lin = std::pow(10.0, px[k] / 10.0);
            summe  += lin;
            moment += lin * k;
            spitze = std::max(spitze, px[k]);
        }
        s.mittePx  = (summe > 0.0) ? moment / summe : 0.5 * (a + e - 1);
        s.spitzeDbm = spitze;
        s.snrDb    = spitze - bodenDbm;
        s.breiteHz = (e - a) * hzJePx;
        raus.push_back(s);
        i = e;
    }
    return raus;
}

QVector<Luecke> findeLuecken(const QVector<float>& px, double hzJePx,
                             int vonPx, int bisPx, float bodenDbm,
                             double mindestHz, double schutzHz, float freiDb)
{
    QVector<Luecke> raus;
    const int n = px.size();
    vonPx = std::clamp(vonPx, 0, n);
    bisPx = std::clamp(bisPx, 0, n);
    if (bisPx - vonPx < 2 || hzJePx <= 0.0) { return raus; }

    const float schwelle = bodenDbm + freiDb;
    auto belegt = [&](int k) {
        if (px[k] <= schwelle) { return false; }
        // Ein einzelner Punkt ueber der Schwelle, beide Nachbarn frei:
        // Rauschen, keine Aussendung.
        const bool links  = (k > vonPx)     && px[k - 1] > schwelle;
        const bool rechts = (k + 1 < bisPx) && px[k + 1] > schwelle;
        return links || rechts;
    };
    const int schutz = static_cast<int>(std::lround(schutzHz / hzJePx));
    int k = vonPx;
    while (k < bisPx) {
        if (belegt(k)) { ++k; continue; }
        const int s = k;
        while (k < bisPx && !belegt(k)) { ++k; }
        // Am Rand des Suchbereichs liegt kein Signal, also dort kein
        // Schutzabstand -- nur zwischen zwei Signalen.
        const int a = (s == vonPx) ? s : s + schutz;
        const int e = (k == bisPx) ? k : k - schutz;
        if (e > a && (e - a) * hzJePx >= mindestHz) {
            raus.push_back({a, e});
        }
    }
    return raus;
}

const QVector<Signal>& Haltung::einspielen(const QVector<Signal>& neu)
{
    QVector<bool> zugeordnet(neu.size(), false);
    for (Eintrag& e : m_eintraege) {
        int treffer = -1;
        double bester = 1.0e9;
        for (int i = 0; i < neu.size(); ++i) {
            if (zugeordnet[i]) { continue; }
            // Dasselbe Signal, wenn die Mitte innerhalb der halben Breite
            // (mindestens zwei Punkte) liegt.
            const double tol = std::max(2.0, 0.5 * (e.s.e - e.s.a));
            const double d = std::abs(neu[i].mittePx - e.s.mittePx);
            if (d <= tol && d < bester) { bester = d; treffer = i; }
        }
        if (treffer >= 0) {
            zugeordnet[treffer] = true;
            e.s = neu[treffer];
            ++e.gesehen;
            e.verpasst = 0;
        } else {
            ++e.verpasst;
        }
    }
    m_eintraege.erase(std::remove_if(m_eintraege.begin(), m_eintraege.end(),
                                     [](const Eintrag& e) { return e.verpasst > 2; }),
                      m_eintraege.end());
    for (int i = 0; i < neu.size(); ++i) {
        if (!zugeordnet[i]) { m_eintraege.push_back({neu[i], 1, 0}); }
    }
    m_gezeigt.clear();
    for (const Eintrag& e : m_eintraege) {
        if (e.gesehen >= 2) { m_gezeigt.push_back(e.s); }
    }
    std::sort(m_gezeigt.begin(), m_gezeigt.end(),
              [](const Signal& x, const Signal& y) { return x.mittePx < y.mittePx; });
    return m_gezeigt;
}

void Haltung::leeren()
{
    m_eintraege.clear();
    m_gezeigt.clear();
}

BinBereich lupenBins(int binZahl, double binHz, double fftUntenHz,
                     double vonHz, double bisHz)
{
    BinBereich b;
    if (binZahl <= 0 || binHz <= 0.0 || bisHz <= vonHz) { return b; }
    const int erster = static_cast<int>(std::floor((vonHz - fftUntenHz) / binHz));
    const int letzter = static_cast<int>(std::ceil((bisHz - fftUntenHz) / binHz));
    b.erster  = std::max(0, erster);
    b.letzter = std::min(binZahl - 1, letzter);
    return b;
}

int bandplanTextX(int x1, int x2, int textW, const QVector<int>& punkte, int frei)
{
    const int kandidaten[3] = {(x1 + x2 - textW) / 2, x1 + 4, x2 - 4 - textW};
    for (int k : kandidaten) {
        if (k < x1 + 2 || k + textW > x2 - 2) { continue; }
        bool trifft = false;
        for (int px : punkte) {
            if (px + frei > k && px - frei < k + textW) { trifft = true; break; }
        }
        if (!trifft) { return k; }
    }
    return -1;
}

} // namespace Longpath::Durchlass
