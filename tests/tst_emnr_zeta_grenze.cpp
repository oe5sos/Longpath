// =================================================================
// tests/tst_emnr_zeta_grenze.cpp  (Longpath)
// =================================================================
//
// Die zweite Indexgrenze in `getZeta` (emnr.c) verglich die dB-Zahl
// statt des Index.  Warren Pratt hat das in WDSP 2.11 geändert
// (`i_xi >= dim_zeta` statt `xi_dB >= dim_zeta`, TAPR OpenHPSDR-wdsp
// @e69de50, `wdsp 2.11/Source/emnr.c:1086`); übernommen am 2026-10-31,
// siehe `docs/attribution/WDSP-PROVENANCE.md`.
//
// Warum das zählt: mit den ausgelieferten Tabellengrenzen
// (`zetaHat.c`: Ximin −39,5 dB, Ximax +20,5 dB, 60 × 60 Einträge) ist
// eine Zelle genau 1 dB breit, also `i_xi = floor(xi_dB + 39,5)`.  Ab
// einem a-priori-Störabstand von **+20,5 dB** liegt `i_xi` außerhalb
// der 60 Spalten — die alte Grenze griff aber erst ab **+60 dB**.
// Dazwischen las `getZeta` die falsche Zeile der Tabelle, und bei
// hohem gamma über ihr Ende hinaus.
//
// Der Prüflauf fährt die Grenze von beiden Seiten und hält sie an
// einer Marke hinter der Tabelle fest, damit ein Rückfall auf die
// alte Zeile hier auffällt und nicht erst am Gerät.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Angelegt beim Übernehmen der WDSP-2.11-Rosinen,
//                 KI-gestützt über Anthropic Claude Code, Betreiber
//                 Martin Fischer.
// =================================================================

#include <QtTest/QtTest>

#include <cmath>
#include <vector>

extern "C" {
#include "comm.h"
#include "emnr.h"
int getZeta (EMNR a, double gamma, double eps, double* zeta);
}

namespace {

// Die ausgelieferten Grenzen aus third_party/wdsp/src/zetaHat.c:27-32.
constexpr int    kDim     = 60;
constexpr double kGammaMin = -29.5;
constexpr double kGammaMax =  30.5;
constexpr double kXiMin    = -39.5;
constexpr double kXiMax     = 20.5;

// gamma/eps sind lineare Leistungsverhältnisse; getZeta rechnet sie in dB.
double fromDb (double db) { return std::pow (10.0, db / 10.0); }

} // namespace

class TstEmnrZetaGrenze : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void innerhalbDerTabelleLiefertDenWert();
    void oberhalbDerSpaltengrenzeWirdAbgewiesen_data();
    void oberhalbDerSpaltengrenzeWirdAbgewiesen();
    void unterhalbDerSpaltengrenzeWirdAbgewiesen();
    void dieMarkeHinterDerTabelleBleibtUnberuehrt();

private:
    emnr                m_a {};
    std::vector<double> m_zetaHat;
    std::vector<int>    m_zetaTrue;

    static constexpr int kGuard = 256;   // Marken hinter der Tabelle
};

void TstEmnrZetaGrenze::init()
{
    m_a = emnr {};
    m_a.g.dim_zeta    = kDim;
    m_a.g.z_gamma_min = kGammaMin;
    m_a.g.z_gamma_max = kGammaMax;
    m_a.g.z_xihat_min = kXiMin;
    m_a.g.z_xihat_max = kXiMax;
    m_a.g.zeta_thresh = 0.5;

    // Tabelle plus Marken: jeder Eintrag innerhalb trägt seinen Index als
    // Wert, jeder hinter der Tabelle eine unverwechselbare Zahl.
    m_zetaHat.assign (kDim * kDim + kGuard, -12345.0);
    m_zetaTrue.assign (kDim * kDim + kGuard, -1);
    for (int i = 0; i < kDim * kDim; ++i) {
        m_zetaHat[i]  = static_cast<double> (i);
        m_zetaTrue[i] = 1;
    }
    m_a.g.zeta_hat  = m_zetaHat.data();
    m_a.g.zeta_true = m_zetaTrue.data();
}

void TstEmnrZetaGrenze::cleanup()
{
    m_a.g.zeta_hat  = nullptr;
    m_a.g.zeta_true = nullptr;
}

// Mitte der Tabelle: der gelesene Wert muss genau der Zelle entsprechen,
// die aus gamma und eps folgt. Hält die Indexrechnung selbst fest.
void TstEmnrZetaGrenze::innerhalbDerTabelleLiefertDenWert()
{
    const double gammaDb = 0.0;    // i_gamma = floor(0 + 29.5) = 29
    const double xiDb    = 0.0;    // i_xi    = floor(0 + 39.5) = 39
    double zeta = -1.0;

    QCOMPARE (getZeta (&m_a, fromDb (gammaDb), fromDb (xiDb), &zeta), 0);
    QCOMPARE (zeta, static_cast<double> (29 * kDim + 39));
}

void TstEmnrZetaGrenze::oberhalbDerSpaltengrenzeWirdAbgewiesen_data()
{
    QTest::addColumn<double> ("xiDb");
    QTest::addColumn<double> ("gammaDb");

    // Alle vier lagen zwischen der echten Grenze (+20,5 dB) und der
    // alten, falschen (+60 dB) — also genau im Loch.
    QTest::newRow ("knapp drueber, mittleres gamma") << 20.6 <<  0.0;
    QTest::newRow ("knapp drueber, hohes gamma")     << 20.6 << 29.0;
    QTest::newRow ("mitten im Loch")                 << 40.0 <<  0.0;
    QTest::newRow ("hartes Signal, hohes gamma")     << 59.0 << 29.0;
}

void TstEmnrZetaGrenze::oberhalbDerSpaltengrenzeWirdAbgewiesen()
{
    QFETCH (double, xiDb);
    QFETCH (double, gammaDb);

    double zeta = -1.0;
    QCOMPARE (getZeta (&m_a, fromDb (gammaDb), fromDb (xiDb), &zeta), -1);
    QCOMPARE (zeta, -1.0);          // unberührt gelassen
}

void TstEmnrZetaGrenze::unterhalbDerSpaltengrenzeWirdAbgewiesen()
{
    double zeta = -1.0;
    // xi_dB = −40 → i_xi = −1
    QCOMPARE (getZeta (&m_a, fromDb (0.0), fromDb (-40.0), &zeta), -1);
    QCOMPARE (zeta, -1.0);
}

// Die eigentliche Folge der alten Grenze: ein Lesen hinter der Tabelle.
// Bei i_gamma = 59 und i_xi = 99 ergibt die Indexrechnung 3639 — 39
// Einträge hinter dem Ende. Die Marken dort dürfen nie herauskommen.
void TstEmnrZetaGrenze::dieMarkeHinterDerTabelleBleibtUnberuehrt()
{
    double zeta = -1.0;
    const int rc = getZeta (&m_a, fromDb (30.0), fromDb (20.6), &zeta);

    QCOMPARE (rc, -1);
    QVERIFY2 (zeta != -12345.0,
              "getZeta hat eine Marke hinter der Tabelle gelesen — die "
              "Indexgrenze in emnr.c vergleicht wieder die dB-Zahl "
              "statt des Index");
    QCOMPARE (zeta, -1.0);
}

QTEST_MAIN (TstEmnrZetaGrenze)
#include "tst_emnr_zeta_grenze.moc"
