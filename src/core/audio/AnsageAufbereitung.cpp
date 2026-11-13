// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/audio/AnsageAufbereitung.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original; siehe AnsageAufbereitung.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/audio/AnsageAufbereitung.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Longpath::AnsageAufbereitung {

namespace {

constexpr float kStilleDb = -120.0f;

float zuDb(double linear)
{
    return linear > 1e-6 ? static_cast<float>(20.0 * std::log10(linear)) : kStilleDb;
}

float ausDb(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

int msZuProben(int ms, int rateHz)
{
    return static_cast<int>(static_cast<long long>(ms) * rateHz / 1000);
}

// Pegel einer sortierten Liste an der Stelle p (0..1).
float stelle(std::vector<float> werte, double p)
{
    if (werte.empty()) { return kStilleDb; }
    std::sort(werte.begin(), werte.end());
    const auto i = static_cast<size_t>(std::clamp(p, 0.0, 1.0) * double(werte.size() - 1));
    return werte[i];
}

} // namespace

Befund miss(const float* s, int n, int rateHz)
{
    Befund b;
    b.ende = n;
    if (s == nullptr || n <= 0 || rateHz <= 0) {
        return b;
    }

    const int rahmen = std::max(1, msZuProben(kRahmenMs, rateHz));
    const int anzahl = (n + rahmen - 1) / rahmen;
    std::vector<float> db(static_cast<size_t>(anzahl));
    for (int r = 0; r < anzahl; ++r) {
        const int a = r * rahmen;
        const int e = std::min(n, a + rahmen);
        double summe = 0.0;
        for (int i = a; i < e; ++i) { summe += double(s[i]) * s[i]; }
        db[static_cast<size_t>(r)] = zuDb(std::sqrt(summe / std::max(1, e - a)));
    }

    // Grundrauschen und Sprachpegel aus der Verteilung, nicht aus der
    // Spitze: ein Tastenklick ist lauter als jede Silbe, aber selten.
    const float grund = stelle(db, 0.10);
    const float laut  = stelle(db, 0.95);
    if (laut - grund < 12.0f) {
        return b;   // nur Rauschen
    }
    const float schwelle = std::max(grund + 10.0f, laut - 30.0f);

    // Sprache = mindestens kHaltMs laut am Stueck (bis auf einen Rahmen
    // Luecke). Ein Klick dauert ein, zwei Rahmen.
    const int halt = std::max(2, kHaltMs / kRahmenMs);
    const int noetig = halt - 1;
    auto istLaut = [&](int r) { return r >= 0 && r < anzahl && db[size_t(r)] >= schwelle; };
    auto zaehle = [&](int von, int bis) {
        int z = 0;
        for (int r = von; r < bis; ++r) { z += istLaut(r) ? 1 : 0; }
        return z;
    };

    int erster = -1;
    for (int r = 0; r < anzahl; ++r) {
        if (istLaut(r) && zaehle(r, r + halt) >= noetig) { erster = r; break; }
    }
    if (erster < 0) {
        return b;
    }
    int letzter = erster;
    for (int r = anzahl - 1; r >= erster; --r) {
        if (istLaut(r) && zaehle(r - halt + 1, r + 1) >= noetig) { letzter = r; break; }
    }

    b.sprache = true;
    b.anfang = erster * rahmen;
    b.ende = std::min(n, (letzter + 1) * rahmen);

    double summe = 0.0;
    int lauteRahmen = 0;
    float spitze = 0.0f;
    for (int r = erster; r <= letzter; ++r) {
        const int a = r * rahmen;
        const int e = std::min(n, a + rahmen);
        for (int i = a; i < e; ++i) {
            const float v = std::fabs(s[i]);
            spitze = std::max(spitze, v);
            if (v >= 0.99f) { ++b.amAnschlag; }
        }
        if (istLaut(r)) {
            for (int i = a; i < e; ++i) { summe += double(s[i]) * s[i]; }
            lauteRahmen += e - a;
        }
    }
    b.spitzeDb = zuDb(spitze);
    b.spracheDb = zuDb(std::sqrt(summe / std::max(1, lauteRahmen)));
    return b;
}

QVector<float> kuerze(const QVector<float>& s, int rateHz, Befund* befund)
{
    const Befund b = miss(s, rateHz);
    if (befund) { *befund = b; }
    if (!b.sprache) {
        return s;
    }
    const int n = int(s.size());
    const int a = std::max(0, b.anfang - msZuProben(kVorMs, rateHz));
    const int e = std::min(n, b.ende + msZuProben(kNachMs, rateHz));
    QVector<float> aus(s.constBegin() + a, s.constBegin() + e);

    // Weiche Kanten: ein harter Schnitt mitten im Rauschen knackt.
    const int blende = std::min(int(aus.size()) / 2, msZuProben(kBlendeMs, rateHz));
    for (int i = 0; i < blende; ++i) {
        const float g = float(i) / float(blende);
        aus[i] *= g;
        aus[aus.size() - 1 - i] *= g;
    }
    if (befund) {
        befund->anfang -= a;
        befund->ende -= a;
    }
    return aus;
}

float verstaerkung(const Befund& b)
{
    if (b.spitzeDb <= kStilleDb) {
        return 1.0f;
    }
    float db = kZielSpitzeDb - b.spitzeDb;
    if (b.sprache) {
        db = std::min(db, kZielSpracheDb - b.spracheDb);
    }
    return ausDb(std::clamp(db, -20.0f, kMaxHubDb));
}

void Hochpass::setze(int rateHz, float grenzHz)
{
    // Butterworth, bilinear transformiert (RBJ-Kochbuch, Q = 1/sqrt 2).
    const double w0 = 2.0 * std::numbers::pi * grenzHz / std::max(1, rateHz);
    const double cosw = std::cos(w0);
    constexpr double q = 1.0 / std::numbers::sqrt2;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    m_b0 = (1.0 + cosw) / 2.0 / a0;
    m_b1 = -(1.0 + cosw) / a0;
    m_b2 = m_b0;
    m_a1 = -2.0 * cosw / a0;
    m_a2 = (1.0 - alpha) / a0;
    zuruecksetzen();
}

float Hochpass::operator()(float x)
{
    const double y = m_b0 * x + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
    m_x2 = m_x1;
    m_x1 = x;
    m_y2 = m_y1;
    m_y1 = y;
    return static_cast<float>(y);
}

float verstaerkungFuerSendung(const QVector<float>& s, int rateHz)
{
    Hochpass hp;
    hp.setze(rateHz);
    QVector<float> gefiltert(s.size());
    for (int i = 0; i < s.size(); ++i) { gefiltert[i] = hp(s[i]); }
    return verstaerkung(miss(gefiltert, rateHz));
}

QVector<float> fuerSendung(const QVector<float>& s, int rateHz)
{
    const float g = verstaerkungFuerSendung(s, rateHz);
    Hochpass hp;
    hp.setze(rateHz);
    QVector<float> aus(s.size());
    for (int i = 0; i < s.size(); ++i) {
        aus[i] = std::clamp(hp(s[i]) * g, -1.0f, 1.0f);
    }
    return aus;
}

} // namespace Longpath::AnsageAufbereitung
