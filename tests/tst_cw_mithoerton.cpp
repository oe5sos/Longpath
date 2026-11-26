// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_cw_mithoerton.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Der lokale Mithoerton (src/core/cw/CwMithoerton):
// `fuelle()` ist die ganze Tonerzeugung als reine Funktion -- Stille bei
// Taste oben, weiche Flanken, der eingestellte Pegel bei Taste unten.
// Eine Tonausgabe wird hier nie geoeffnet.
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test.

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/cw/CwMithoerton.h"

using namespace Longpath;

namespace {

constexpr int kFlanke = CwMithoerton::kFlankeMs * CwMithoerton::kAbtastrate / 1000;   // 240

float spitze(const std::vector<float>& x, int von, int bis)
{
    float m = 0.0f;
    for (int i = von; i < bis; ++i) { m = std::max(m, std::abs(x[size_t(i)])); }
    return m;
}

float groessterSprung(const std::vector<float>& x, float davor)
{
    float m = 0.0f;
    float alt = davor;
    for (const float v : x) {
        m = std::max(m, std::abs(v - alt));
        alt = v;
    }
    return m;
}

} // namespace

class TstCwMithoerton : public QObject {
    Q_OBJECT

private slots:
    void stilleBeiTasteOben()
    {
        CwMithoerton ton;
        std::vector<float> x(4800, 1.0f);
        ton.fuelle(x.data(), int(x.size()));
        QCOMPARE(spitze(x, 0, int(x.size())), 0.0f);
    }

    void pegelBeiTasteUnten()
    {
        CwMithoerton ton;
        ton.setLautstaerke(40);
        ton.setTonhoeheHz(600);
        ton.setTaste(true);
        std::vector<float> x(4800);
        ton.fuelle(x.data(), int(x.size()));
        // Nach der Flanke steht der Sinus auf dem eingestellten Pegel.
        const float oben = spitze(x, kFlanke, int(x.size()));
        QVERIFY2(std::abs(oben - 0.40f) < 0.005f, qPrintable(QString::number(oben)));
    }

    // Hart getastet knackt ein Sinus. Die Flanke braucht ihre 5 ms: nach
    // einer Millisekunde ist der Ton noch leise, und kein Probenschritt
    // ist groesser als der des stehenden Sinus.
    void flankeOhneSprung()
    {
        CwMithoerton ton;
        ton.setLautstaerke(100);
        ton.setTonhoeheHz(600);
        const float schrittSinus = float(2.0 * M_PI * 600.0 / CwMithoerton::kAbtastrate);

        ton.setTaste(true);
        std::vector<float> auf(1201);   // ungerade: das Ende faellt mitten in die Welle
        ton.fuelle(auf.data(), int(auf.size()));
        QVERIFY2(spitze(auf, 0, 48) < 0.15f, qPrintable(QString::number(spitze(auf, 0, 48))));
        QVERIFY(spitze(auf, kFlanke, int(auf.size())) > 0.99f);
        QVERIFY(groessterSprung(auf, 0.0f) < schrittSinus * 1.2f);

        ton.setTaste(false);
        std::vector<float> ab(1200);
        ton.fuelle(ab.data(), int(ab.size()));
        const float sprung = groessterSprung(ab, auf.back());
        QVERIFY2(sprung < schrittSinus * 1.2f, qPrintable(QString::number(sprung)));
        // In der ersten Millisekunde ist er noch fast voll da ...
        QVERIFY(spitze(ab, 0, 48) > 0.5f);
        // ... und nach der Flanke ist Ruhe.
        QCOMPARE(spitze(ab, kFlanke, int(ab.size())), 0.0f);
    }

    void grenzenDerEinstellwerte()
    {
        CwMithoerton ton;
        ton.setTonhoeheHz(50);
        QCOMPARE(ton.tonhoeheHz(), 300);
        ton.setTonhoeheHz(5000);
        QCOMPARE(ton.tonhoeheHz(), 1200);
        ton.setLautstaerke(250);
        QCOMPARE(ton.lautstaerke(), 100);
        ton.setLautstaerke(0);
        ton.setTaste(true);
        std::vector<float> x(2400, 1.0f);
        ton.fuelle(x.data(), int(x.size()));
        QCOMPARE(spitze(x, 0, int(x.size())), 0.0f);
    }
};

QTEST_GUILESS_MAIN(TstCwMithoerton)
#include "tst_cw_mithoerton.moc"
