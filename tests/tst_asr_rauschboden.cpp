// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-original (2026-11-11). Der Schneider der Mitschrift auf einem
// belebten Band: Schwelle am Rauschboden statt fester Schwelle.
//
// Anlass, gemessen am selben Tag an einem Pileup-Mitschnitt (NT6X,
// CQ WW SSB): der Pegel lag dort im Median bei 0,084, nie unter der
// festen Schwelle 0,010. 19 von 22 Abschnitten liefen bis zum
// 20-s-Deckel. Fuer eine Rufzeichenkontrolle heisst das: bis zu 20 s
// zu spaet, und das Rufzeichen oft mitten durchgeschnitten.
//
// Nachgebaut wird das Band hier mit gleichmaessigem Rauschen (RMS 0,08)
// und Durchgaengen darueber. Jeder Fall faehrt die alte Schwelle als
// Gegenprobe mit: sie muss am selben Signal scheitern, sonst beweist
// der Fall nichts.
//
// Modification history (Longpath):
//   2026-11-11 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude.

#define _USE_MATH_DEFINES

#include <QtTest>

#include <cmath>
#include <cstdint>
#include <vector>

#include "asr/AsrSegmenter.h"

using Longpath::AsrSegmenter;

namespace {

constexpr int kRate = 16000;

// Gleichverteiltes Rauschen aus einem festen Generator: dieselben Zahlen
// bei jedem Lauf. Gleichverteilung in [-a, a] hat RMS a/sqrt(3).
struct Rauschen {
    uint32_t zustand = 12345u;
    float naechstes(float rms)
    {
        zustand = zustand * 1664525u + 1013904223u;
        const float u = static_cast<float>(zustand >> 8) / static_cast<float>(1u << 24); // [0,1)
        return (2.0f * u - 1.0f) * rms * std::sqrt(3.0f);
    }
};

void rauschen(std::vector<float>& buf, Rauschen& r, int ms, float rms)
{
    const int n = ms * kRate / 1000;
    for (int i = 0; i < n; ++i) { buf.push_back(r.naechstes(rms)); }
}

// Ein Durchgang: Ton 0,3 (RMS ~0,21) UEBER dem Rauschen, wie eine
// Station ueber dem Bandrauschen.
void durchgang(std::vector<float>& buf, Rauschen& r, int ms, float rauschRms)
{
    const int n = ms * kRate / 1000;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kRate;
        buf.push_back(0.3f * static_cast<float>(std::sin(2.0 * M_PI * 700.0 * t))
                      + r.naechstes(rauschRms));
    }
}

void stille(std::vector<float>& buf, int ms)
{
    buf.insert(buf.end(), static_cast<size_t>(ms * kRate / 1000), 0.0f);
}

// Das Pileup: 2 s Band, dann zehn Durchgaenge zu 2 s mit je 1 s Band
// dazwischen — 32 s, also laenger als der 20-s-Deckel.
std::vector<float> pileup()
{
    Rauschen r;
    std::vector<float> a;
    rauschen(a, r, 2000, 0.08f);
    for (int i = 0; i < 10; ++i) {
        durchgang(a, r, 2000, 0.08f);
        rauschen(a, r, 1000, 0.08f);
    }
    return a;
}

std::vector<std::vector<float>> schneide(AsrSegmenter& seg, const std::vector<float>& a)
{
    auto out = seg.feed(a.data(), static_cast<int>(a.size()));
    auto rest = seg.flush();
    out.insert(out.end(), rest.begin(), rest.end());
    return out;
}

} // namespace

class TstAsrRauschboden : public QObject
{
    Q_OBJECT

private slots:
    void imPileupEinAbschnittJeDurchgang()
    {
        const std::vector<float> band = pileup();

        // Gegenprobe: die feste Schwelle. Das Band liegt durchgehend
        // darueber, also gibt es keine Pause — nur den Deckel.
        AsrSegmenter alt;
        const auto altOut = schneide(alt, band);
        QVERIFY2(altOut.size() <= 2,
                 qPrintable(QStringLiteral("Gegenprobe beweist nichts: die feste "
                                           "Schwelle schnitt %1 Abschnitte")
                                .arg(altOut.size())));
        QVERIFY2(!altOut.empty() && altOut.front().size() >= static_cast<size_t>(19 * kRate),
                 "Gegenprobe: der erste Abschnitt sollte am 20-s-Deckel enden");

        // Am Rauschboden: ein Abschnitt je Durchgang.
        AsrSegmenter neu;
        neu.setNoiseFactor(2.0f);
        const auto neuOut = schneide(neu, band);
        QCOMPARE(neuOut.size(), size_t(10));
        for (const auto& s : neuOut) {
            const double sek = static_cast<double>(s.size()) / kRate;
            // 2 s Durchgang + 0,3 s Nachlauf; ein Rahmen Spiel je Seite.
            QVERIFY2(sek > 2.0 && sek < 2.5,
                     qPrintable(QStringLiteral("Abschnitt %1 s statt ~2,3 s").arg(sek)));
        }
        QVERIFY2(neu.noiseFloor() > 0.07f && neu.noiseFloor() < 0.09f,
                 qPrintable(QStringLiteral("Rauschboden %1 statt ~0,08").arg(neu.noiseFloor())));
    }

    void dieErsteSekundeGehtNichtAlsSpracheDurch()
    {
        // Nur Band, kein Durchgang. Vor der ersten Schaetzung darf das
        // Rauschen nicht als Abschnitt an Whisper gehen.
        Rauschen r;
        std::vector<float> a;
        rauschen(a, r, 5000, 0.08f);
        AsrSegmenter seg;
        seg.setNoiseFactor(2.0f);
        QVERIFY(schneide(seg, a).empty());
    }

    void derErsteDurchgangNachDemEinschaltenBleibt()
    {
        // Eingeschaltet, waehrend jemand spricht: 0,8 s Durchgang gleich
        // am Anfang, dann Band. Der Boden ist da noch unbekannt; der
        // Anlauf hebt die Rahmen auf und spielt sie nach. Taub zu sein
        // verloere genau diesen Durchgang — nach JEDEM Einschalten und
        // jedem Sprachwechsel.
        Rauschen r;
        std::vector<float> a;
        durchgang(a, r, 800, 0.08f);
        rauschen(a, r, 2000, 0.08f);
        AsrSegmenter seg;
        seg.setNoiseFactor(2.0f);
        const auto out = seg.feed(a.data(), static_cast<int>(a.size()));
        QCOMPARE(out.size(), size_t(1));
        const double sek = static_cast<double>(out.front().size()) / kRate;
        QVERIFY2(sek > 0.8 && sek < 1.2,
                 qPrintable(QStringLiteral("Abschnitt %1 s statt ~1,1 s").arg(sek)));
    }

    void aufStillemBandWieBisher()
    {
        // Digitale Stille: der Boden ist 0, es gilt speechRms wie vorher.
        // Beide Schneider muessen dasselbe liefern.
        std::vector<float> a;
        stille(a, 2000);
        Rauschen r;
        for (int i = 0; i < 3; ++i) {
            durchgang(a, r, 500, 0.0f);
            stille(a, 400);
        }
        AsrSegmenter alt;
        AsrSegmenter neu;
        neu.setNoiseFactor(2.0f);
        const auto altOut = schneide(alt, a);
        const auto neuOut = schneide(neu, a);
        QCOMPARE(altOut.size(), size_t(3));
        QCOMPARE(neuOut.size(), altOut.size());
        QCOMPARE(neu.effectiveThreshold(), neu.speechRms());
    }

    void derBodenFolgtDemBand()
    {
        // Vom lauten aufs leise Band (Bandwechsel): nach wenigen Sekunden
        // muss die Schwelle unten sein, sonst ist die Mitschrift taub.
        Rauschen r;
        std::vector<float> laut;
        rauschen(laut, r, 30000, 0.08f);
        std::vector<float> leise;
        rauschen(leise, r, 8000, 0.004f);

        AsrSegmenter seg;
        seg.setNoiseFactor(2.0f);
        seg.feed(laut.data(), static_cast<int>(laut.size()));
        QVERIFY(seg.effectiveThreshold() > 0.14f);
        seg.feed(leise.data(), static_cast<int>(leise.size()));
        QVERIFY2(seg.effectiveThreshold() <= seg.speechRms() + 1e-6f,
                 qPrintable(QStringLiteral("nach 8 s leisem Band noch Schwelle %1")
                                .arg(seg.effectiveThreshold())));

        // reset() vergisst den Boden (neuer Lauf, neues Band).
        seg.feed(laut.data(), static_cast<int>(laut.size()));
        QVERIFY(seg.noiseFloor() > 0.0f);
        seg.reset();
        QCOMPARE(seg.noiseFloor(), 0.0f);
    }

    void ohneFaktorBleibtAllesBeimAlten()
    {
        // Vorgabe des Schneiders ist AUS (AetherSDR-Verhalten); nur
        // AsrService schaltet ein. Damit bleibt der portierte
        // asr_segmenter_test aussagekraeftig.
        AsrSegmenter seg;
        QCOMPARE(seg.noiseFactor(), 0.0f);
        const std::vector<float> band = pileup();
        seg.feed(band.data(), static_cast<int>(band.size()));
        QCOMPARE(seg.noiseFloor(), 0.0f);
        QCOMPARE(seg.effectiveThreshold(), seg.speechRms());
    }
};

QTEST_GUILESS_MAIN(TstAsrRauschboden)
#include "tst_asr_rauschboden.moc"
