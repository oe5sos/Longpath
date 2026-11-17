// no-port-check: Longpath-eigener Prüfstand; die Fälle der ersten zwei
// Gruppen sind den C-Prüfprogrammen der Zeus Station Engine nachgebaut,
// keine Thetis-Herkunft.
// =================================================================
// tests/tst_ps_gedaechtnis.cpp  (Longpath)
// =================================================================
//
// Der PureSignal-Satz aus der Zeus Station Engine v2.0.71
// (third_party/wdsp/src/calcc.c, iqc.c, ps_scheck.c, ps_memcorr.c —
// Herkunft in docs/attribution/WDSP-PROVENANCE.md). Vier Gruppen:
//
//  1. Die Fit-zu-Fit-Toleranz (ps_scheck): unter Hüllkurve 0,20
//     ausgangsbezogen, darüber und für die Phase unverändert 0,10.
//     Nachgebaut aus Zeus native/wdsp/tests/ps_scheck_tests.c @v2.0.71
//     (GPL-2.0-or-later, Douglas J. Cerrato KB2UKA / Christian Suarez
//     N9WAR and contributors).
//  2. Der Schätzer der Gedächtniskorrektur (ps_memcorr): löst Speicher,
//     Zeitversatz und Pegeltrimm gemeinsam, verwirft einen Dauerträger und
//     ein entartetes System, klemmt |beta|. Nachgebaut aus Zeus
//     native/wdsp/tests/ps_memcorr_tests.c @v2.0.71 (ebenso).
//  3. Die zwei Schalter an einem ECHTEN Sendekanal: schnelles Nachführen
//     ist nach dem Aufbau aus, überlebt ein LRESET und ist nach einem
//     neuen Kanal wieder aus; die Gedächtniskorrektur lässt sich schalten
//     und hat danach nichts gelernt.
//  4. Der Abbau mitten im Einspielen einer Korrektur. Der Worker
//     doPSCorrChange wartet in SetTXAiqcStart auf den Sendefaden, der bei
//     einem stehenden Kanal nie kommt. Die alte Reihenfolge in
//     destroy_calcc gab die eingespielten Kurven frei, BEVOR der Worker
//     fertig war -- iqc hielt sie da schon und gab sie im eigenen Abbau
//     ein zweites Mal frei. Ein Doppel-Free bricht den Lauf ab; grün heisst
//     hier: der Prozess lebt noch.
//  5. Der Weg vom Kaestchen im PureSignal-Fenster bis in die
//     Einstellungen, und zurueck in ein neues PureSignal.
// =================================================================
// Modification history (Longpath):
//   2026-11-15 — Neuer Prüfstand zum Zeus-v2.0.71-PureSignal-Satz.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "core/AppSettings.h"
#include "core/PureSignal.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "gui/PsForm.h"

extern "C" {
#include "ps_memcorr.h"
#include "ps_scheck.h"
#ifdef HAVE_WDSP
#include "comm.h"
// Oeffentliche PS-Schnittstelle aus calcc.c, die calcc.h nicht nennt.
void SetPSRunCal (int channel, int run);
void SetPSControl (int channel, int reset, int mancal, int automode, int turnon);
#endif
}

using namespace Longpath;

namespace {

constexpr int kTxChannelId = 1;

// ── Gruppe 1: Hilfen ──────────────────────────────────────────────────────

struct Loesungen {
    double mPrev[SCHECK_PTS], mNew[SCHECK_PTS];
    double cPrev[SCHECK_PTS], cNew[SCHECK_PTS];
    double sPrev[SCHECK_PTS], sNew[SCHECK_PTS];

    Loesungen() { zuruecksetzen(); }

    // Eine glaubwürdige eingeschwungene Lösung: Verstärkung fällt zur
    // Vollaussteuerung hin auf 1, kleine Phasendrehung.
    void zuruecksetzen()
    {
        for (int k = 0; k < SCHECK_PTS; ++k) {
            const double x = ps_scheck_x(k);
            mPrev[k] = mNew[k] = 1.3 - 0.3 * x;
            cPrev[k] = cNew[k] = std::cos(0.1 * x);
            sPrev[k] = sNew[k] = std::sin(0.1 * x);
        }
    }

    bool weichtAb() const
    {
        return ps_scheck_deviates(mPrev, mNew, cPrev, cNew, sPrev, sNew) != 0;
    }
};

// ── Gruppe 2: Hilfen ──────────────────────────────────────────────────────

constexpr double kRate  = 192000.0;
constexpr double kZweiPi = 6.2831853071795864;

struct K {
    double re;
    double im;
};

K mal(K a, K b) { return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re}; }
K durch(K a, K b)
{
    const double m = b.re * b.re + b.im * b.im;
    return {(a.re * b.re + a.im * b.im) / m, (a.im * b.re - a.re * b.im) / m};
}

K zweiton(long n, double amp)
{
    const double t = static_cast<double>(n) / kRate;
    return {amp * (std::cos(kZweiPi * 700.0 * t) + std::cos(kZweiPi * 1900.0 * t)),
            amp * (std::sin(kZweiPi * 700.0 * t) + std::sin(kZweiPi * 1900.0 * t))};
}

void einspeisen(ps_memcorr_acc* acc, K x, K t)
{
    double mag = 0.0, d = 0.0, dtr = 0.0, dti = 0.0;
    if (ps_memcorr_advance(acc, ps_memcorr_dscale(kRate), x.re, x.im, t.re, t.im,
                           &mag, &d, &dtr, &dti)) {
        // Einheitskurven: der Rest ist e = T - X.
        ps_memcorr_accumulate(acc, x.re, x.im, t.re, t.im, d, dtr, dti, 1.0, 1.0, 0.0);
    }
}

#ifdef HAVE_WDSP
// ── Gruppe 4: eine gültige Korrekturdatei ─────────────────────────────────
//
// Von Hand gebaute Kurve (ein Zweig, vier Punkte), geschrieben mit WDSPs
// eigenem WriteCorrectionFileV2 -- so liest ReadCorrectionFileV2 sie
// garantiert, und der Worker kommt bis zur Übergabe an iqc.
NS_Spline* flacheKurve(double y)
{
    auto* s = static_cast<NS_Spline*>(std::calloc(1, sizeof(NS_Spline)));
    s->n_branches = 1;
    s->branches = static_cast<NS_Branch*>(std::calloc(1, sizeof(NS_Branch)));
    NS_Branch& br = s->branches[0];
    br.n_pts = 4;
    br.t_mid = 0.5;
    br.xs = static_cast<double*>(std::malloc(4 * sizeof(double)));
    br.ys = static_cast<double*>(std::malloc(4 * sizeof(double)));
    br.segs = static_cast<NS_Seg*>(std::calloc(3, sizeof(NS_Seg)));
    for (int i = 0; i < 4; ++i) {
        br.xs[i] = static_cast<double>(i) / 3.0;
        br.ys[i] = y;
    }
    return s;
}

bool schreibeKorrektur(const QByteArray& pfad)
{
    NS_Spline* m = flacheKurve(1.0);
    NS_Spline* c = flacheKurve(1.0);
    NS_Spline* s = flacheKurve(0.0);
    CurveEMA mE, cE, sE;
    curve_ema_init2(&mE, 0.30, 0.30, 0.20,  0.1, 2.0);
    curve_ema_init2(&cE, 0.30, 0.30, 0.20, -1.1, 1.1);
    curve_ema_init2(&sE, 0.30, 0.30, 0.20, -1.1, 1.1);
    curve_ema_update(&mE, m);
    curve_ema_update(&cE, c);
    curve_ema_update(&sE, s);
    const int fehler = WriteCorrectionFileV2(pfad.constData(), m, &mE, c, &cE, s, &sE);
    ns_free(m);
    ns_free(c);
    ns_free(s);
    return fehler == 0;
}
#endif

} // namespace

class TestPsGedaechtnis : public QObject {
    Q_OBJECT

private slots:
    // ── 1. Fit-zu-Fit-Toleranz ────────────────────────────────────────────

    void toleranzverlauf()
    {
        // 0,50 bei k = 0 und 1 (unter dem Boden 0,04), fällt wie 1/x auf
        // 0,10 bei x >= 0,20, und von dort an genau SCHECK_TOL.
        QVERIFY(std::fabs(ps_scheck_mag_tol(ps_scheck_x(0)) - 0.50) < 1e-12);
        QVERIFY(std::fabs(ps_scheck_mag_tol(ps_scheck_x(1)) - 0.50) < 1e-12);
        QVERIFY(std::fabs(ps_scheck_mag_tol(0.10) - 0.20) < 1e-12);
        QCOMPARE(ps_scheck_mag_tol(0.20), SCHECK_TOL);
        for (int k = 0; k < SCHECK_PTS; ++k) {
            const double x = ps_scheck_x(k);
            const double tol = ps_scheck_mag_tol(x);
            QVERIFY(std::isfinite(tol));
            if (x >= SCHECK_MAG_WEIGHT_X) {
                QCOMPARE(tol, SCHECK_TOL);
            } else {
                QVERIFY(tol > SCHECK_TOL && tol <= 0.50 + 1e-12);
            }
        }
    }

    void gleiche_loesung_geht_durch()
    {
        Loesungen l;
        QVERIFY(!l.weichtAb());
    }

    void zittern_unten_am_warmen_verstaerker_geht_durch()
    {
        // Der Befund am Sender: die angepasste Verstärkung ganz unten in der
        // Hüllkurve (k = 1..3, x = 0,026..0,077) springt zwischen zwei
        // Anpassungen. 0,3 bei x <= 0,051 und 0,2 bei x = 0,077 sind am
        // Ausgang je unter 0,016 wert und dürfen die Anpassung nicht
        // verwerfen. Mit dem alten festen 0,10 wäre das ein Abweichen.
        Loesungen l;
        for (int k = 0; k <= 2; ++k) { l.mNew[k] += 0.3; }
        l.mNew[3] -= 0.2;
        QVERIFY(!l.weichtAb());
    }

    void betrag_ab_der_grenze_wie_bisher()
    {
        Loesungen l;
        for (int k = 0; k < SCHECK_PTS; ++k) {
            if (ps_scheck_x(k) < SCHECK_MAG_WEIGHT_X) { continue; }
            l.zuruecksetzen();
            l.mNew[k] += 0.3;
            QVERIFY2(l.weichtAb(), qPrintable(QStringLiteral("k=%1").arg(k)));
            l.zuruecksetzen();
            l.mNew[k] -= 0.099;
            QVERIFY2(!l.weichtAb(), qPrintable(QStringLiteral("k=%1").arg(k)));
        }
        // Der erste gewichtete Punkt unter der Grenze verwirft 0,3 noch:
        // seine Toleranz ist 0,10 * 0,20 / x, rund 0,11 bei x = 0,18.
        l.zuruecksetzen();
        l.mNew[7] += 0.3;
        QVERIFY(ps_scheck_x(7) < SCHECK_MAG_WEIGHT_X);
        QVERIFY(l.weichtAb());
    }

    void phase_unveraendert()
    {
        Loesungen l;
        for (int k = 0; k < SCHECK_PTS; ++k) {
            l.zuruecksetzen();
            l.cNew[k] += 0.15;
            QVERIFY(l.weichtAb());
            l.zuruecksetzen();
            l.sNew[k] -= 0.15;
            QVERIFY(l.weichtAb());
            l.zuruecksetzen();
            l.cNew[k] += 0.099;
            l.sNew[k] -= 0.099;
            QVERIFY(!l.weichtAb());
        }
    }

    // ── 2. Schätzer der Gedächtniskorrektur ───────────────────────────────

    void massstaebe()
    {
        QCOMPARE(ps_memcorr_window(kRate), 96000L);
        QCOMPARE(ps_memcorr_window(48000.0), 24000L);
        QVERIFY(std::fabs(ps_memcorr_dscale(kRate) - kRate / (kZweiPi * 1000.0)) < 1e-12);
    }

    void gemeinsame_loesung_findet_speicher_versatz_und_trimm()
    {
        // T so gebaut, dass genau T - X = c1*X*D(X) + c2*dT/dt + c3*T gilt.
        // Die Zweiton-Hüllkurve geht durch null: Proben unter X_MIN fallen
        // heraus, und die Stetigkeit muss über sie hinweg halten.
        const K c1{-0.008, 0.003};    // Speicher
        const K c2{-0.0063, 0.0011};  // ~1 us Versatz der Ströme
        const K c3{0.02, -0.01};      // Pegel-/Phasentrimm
        const double k = ps_memcorr_dscale(kRate);
        const K nenner{1.0 - c3.re - c2.re * k, -c3.im - c2.im * k};
        ps_memcorr_acc acc;
        K tVor{0.0, 0.0};
        double x2Vor = 0.0;
        double cRe[PS_MEMCORR_NREG], cIm[PS_MEMCORR_NREG];
        long untenAusgelassen = 0;
        ps_memcorr_reset(&acc);
        for (long n = 0; n < 96000; ++n) {
            const K x = zweiton(n, 0.4);
            const double x2 = x.re * x.re + x.im * x.im;
            const double d = (x2 - x2Vor) * k;
            K rechts = mal(x, K{1.0 + c1.re * d, c1.im * d});
            const K uebertrag = mal(c2, tVor);
            rechts.re -= uebertrag.re * k;
            rechts.im -= uebertrag.im * k;
            const K t = durch(rechts, nenner);
            if (std::sqrt(x2) < PS_MEMCORR_X_MIN) { ++untenAusgelassen; }
            einspeisen(&acc, x, t);
            tVor = t;
            x2Vor = x2;
        }
        QVERIFY(untenAusgelassen > 0);
        QCOMPARE(acc.n, 96000L - 1 - untenAusgelassen);
        QVERIFY(ps_memcorr_solve(&acc, cRe, cIm));
        QVERIFY(std::fabs(cRe[0] - c1.re) < 1e-9 && std::fabs(cIm[0] - c1.im) < 1e-9);
        QVERIFY(std::fabs(cRe[1] - c2.re) < 1e-9 && std::fabs(cIm[1] - c2.im) < 1e-9);
        QVERIFY(std::fabs(cRe[2] - c3.re) < 1e-9 && std::fabs(cIm[2] - c3.im) < 1e-9);
    }

    void ausgelassene_proben_halten_die_stetigkeit()
    {
        ps_memcorr_acc acc;
        double x = 0.0, d = 0.0, dtr = 0.0, dti = 0.0;
        const double k = ps_memcorr_dscale(kRate);
        ps_memcorr_reset(&acc);
        // Die erste Probe nach dem Rücksetzen hat keinen Vorgänger.
        QVERIFY(!ps_memcorr_advance(&acc, k, 0.5, 0.0, 0.5, 0.0, &x, &d, &dtr, &dti));
        // Unter dem Boden und über Vollaussteuerung: ausgelassen, aber
        // jede wird Vorgänger der nächsten.
        QVERIFY(!ps_memcorr_advance(&acc, k, 0.01, 0.0, 0.2, 0.0, &x, &d, &dtr, &dti));
        QVERIFY(!ps_memcorr_advance(&acc, k, 1.5, 0.0, 0.3, 0.0, &x, &d, &dtr, &dti));
        QVERIFY(ps_memcorr_advance(&acc, k, 0.0, 0.6, 0.1, 0.4, &x, &d, &dtr, &dti));
        QVERIFY(std::fabs(x - 0.6) < 1e-15);
        QVERIFY(std::fabs(d - (0.36 - 2.25) * k) < 1e-9);
        QVERIFY(std::fabs(dtr - (0.1 - 0.3) * k) < 1e-12);
        QVERIFY(std::fabs(dti - 0.4 * k) < 1e-12);
        // Fenstersummen leeren hält die Stetigkeit, Rücksetzen nicht.
        ps_memcorr_accumulate(&acc, 0.0, 0.6, 0.1, 0.4, d, dtr, dti, 1.0, 1.0, 0.0);
        ps_memcorr_clear_sums(&acc);
        QVERIFY(acc.n == 0 && acc.g_re[0][0] == 0.0 && acc.r_re[0] == 0.0 && acc.sum_x2 == 0.0);
        QVERIFY(ps_memcorr_advance(&acc, k, 0.0, 0.5, 0.1, 0.4, &x, &d, &dtr, &dti));
        QVERIFY(std::fabs(d - (0.25 - 0.36) * k) < 1e-9);
        ps_memcorr_reset(&acc);
        QVERIFY(!ps_memcorr_advance(&acc, k, 0.0, 0.5, 0.1, 0.4, &x, &d, &dtr, &dti));
    }

    void dauertraeger_wird_verworfen()
    {
        // TUN: feste Hüllkurve plus Rauschen im Rückweg. Der Speicherterm
        // sieht nur Rauschen; das Fenster darf keine Schätzung liefern.
        ps_memcorr_acc acc;
        double cRe[PS_MEMCORR_NREG] = {7.0, 7.0, 7.0};
        double cIm[PS_MEMCORR_NREG] = {7.0, 7.0, 7.0};
        unsigned int saat = 12345u;
        ps_memcorr_reset(&acc);
        for (long n = 0; n < 96000; ++n) {
            const double ph = kZweiPi * 1000.0 * static_cast<double>(n) / kRate;
            saat = saat * 1103515245u + 12345u;
            const double rauschen =
                3e-4 * (static_cast<double>((saat >> 8) & 0xffff) / 32768.0 - 1.0);
            const K x{(0.9 + rauschen) * std::cos(ph), (0.9 + rauschen) * std::sin(ph)};
            const K t{0.95 * std::cos(ph), 0.95 * std::sin(ph)};
            einspeisen(&acc, x, t);
        }
        QVERIFY(acc.n > 0);
        QVERIFY(!ps_memcorr_solve(&acc, cRe, cIm));
        QCOMPARE(cRe[0], 7.0);   // bei Misserfolg unberührt
        QCOMPARE(cIm[0], 7.0);
    }

    void entartetes_system_wird_verworfen()
    {
        ps_memcorr_acc acc;
        double cRe[PS_MEMCORR_NREG], cIm[PS_MEMCORR_NREG];
        ps_memcorr_reset(&acc);
        QVERIFY(!ps_memcorr_solve(&acc, cRe, cIm));   // leeres Fenster
        // Genug Hüllkurvenhub, aber keine Sendereferenz: phi2 = phi3 = 0.
        for (long n = 0; n < 9600; ++n) {
            einspeisen(&acc, zweiton(n, 0.4), K{0.0, 0.0});
        }
        QVERIFY(acc.n > 0);
        QVERIFY(!ps_memcorr_solve(&acc, cRe, cIm));
    }

    void lernrate_und_klemme()
    {
        double br = 0.0, bi = 0.0;
        ps_memcorr_learn(&br, &bi, 0.01, -0.005);
        QVERIFY(std::fabs(br - 0.08 * 0.01) < 1e-15 && std::fabs(bi + 0.08 * 0.005) < 1e-15);
        ps_memcorr_learn(&br, &bi, 0.01, -0.005);
        QVERIFY(std::fabs(br - (0.0008 + 0.08 * (0.01 - 0.0008))) < 1e-15);
        // Eine davonlaufende Schätzung wird auf |beta| = BETA_MAX gehalten,
        // die Richtung bleibt.
        for (int i = 0; i < 50; ++i) {
            ps_memcorr_learn(&br, &bi, 1.0, 1.0);
            QVERIFY(std::sqrt(br * br + bi * bi) <= PS_MEMCORR_BETA_MAX * (1.0 + 1e-12));
        }
        QVERIFY(std::fabs(std::sqrt(br * br + bi * bi) - PS_MEMCORR_BETA_MAX) < 1e-12);
        QVERIFY(std::fabs(br - bi) < 1e-12);
    }

    // ── 3. Die Schalter am echten Sendekanal ──────────────────────────────

    void schnelles_nachfuehren_am_echten_kanal()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        for (int runde = 0; runde < 2; ++runde) {
            WdspEngine engine;
            engine.m_initialized = true;   // friend access (LONGPATH_BUILD_TESTS)
            engine.openPsFeedbackChannelForTesting();
            TxChannel* tx = engine.createTxChannel(kTxChannelId);
            QVERIFY(tx != nullptr);

            // Ein neuer Kanal faehrt mit der gewohnten Glaettung -- auch in
            // der zweiten Runde, obwohl die erste es eingeschaltet liess.
            QCOMPARE(GetPSFastTracking(kTxChannelId), 0);
            tx->setPSFastTracking(true);
            QCOMPARE(GetPSFastTracking(kTxChannelId), 1);

            // LRESET ueber den echten Rueckweg: die Wahl bleibt.
            double nullen[2 * 64] = {};
            SetPSRunCal(kTxChannelId, 1);
            SetPSControl(kTxChannelId, 1, 0, 0, 0);
            pscc(kTxChannelId, 64, nullen, nullen);
            QCOMPARE(GetPSFastTracking(kTxChannelId), 1);

            tx->setPSFastTracking(false);
            QCOMPARE(GetPSFastTracking(kTxChannelId), 0);
            tx->setPSFastTracking(true);   // so stehen lassen
            engine.destroyTxChannel(kTxChannelId);
        }
#endif
    }

    void tun_nimmt_das_schnelle_nachfuehren_zurueck()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        WdspEngine engine;
        engine.m_initialized = true;
        engine.openPsFeedbackChannelForTesting();
        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);
        tx->setPSFastTracking(true);
        tx->setTuneTone(true, 600.0, 0.5);
        QCOMPARE(GetPSFastTracking(kTxChannelId), 0);
        tx->setPSFastTracking(true);
        tx->setTuneTone(false, 600.0, 0.5);
        QCOMPARE(GetPSFastTracking(kTxChannelId), 0);
        engine.destroyTxChannel(kTxChannelId);
#endif
    }

    void gedaechtniskorrektur_schalten()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        WdspEngine engine;
        engine.m_initialized = true;
        engine.openPsFeedbackChannelForTesting();
        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);

        double re = 1.0, im = 1.0;
        tx->getPSMemoryCorrection(re, im);
        QCOMPARE(re, 0.0);
        QCOMPARE(im, 0.0);

        tx->setPSMemoryCorrection(true);
        // Ohne Rueckweg und ohne Korrektur lernt sie nichts.
        double nullen[2 * 64] = {};
        pscc(kTxChannelId, 64, nullen, nullen);
        tx->getPSMemoryCorrection(re, im);
        QCOMPARE(re, 0.0);
        QCOMPARE(im, 0.0);
        // iqc bekommt einen Koeffizienten erst, wenn einer gelernt ist.
        QCOMPARE(txa[kTxChannelId].iqc.p->mem_beta_re, 0.0);
        QCOMPARE(txa[kTxChannelId].iqc.p->mem_beta_im, 0.0);

        tx->setPSMemoryCorrection(false);
        engine.destroyTxChannel(kTxChannelId);
#endif
    }

    // ── 4. Abbau mitten im Einspielen ─────────────────────────────────────

    void abbau_waehrend_der_worker_auf_iqc_wartet()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        const QByteArray pfad = ordner.filePath(QStringLiteral("korrektur.txt")).toLocal8Bit();
        QVERIFY(schreibeKorrektur(pfad));

        WdspEngine engine;
        engine.m_initialized = true;
        engine.openPsFeedbackChannelForTesting();
        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);

        // Der Worker liest die Datei und uebergibt die Kurven an iqc.
        // SetTXAiqcStart setzt iqc.busy und wartet, bis der Sendefaden es
        // loescht -- der laeuft hier nie, also bleibt der Worker stehen.
        tx->psRestoreCorr(QString::fromLocal8Bit(pfad));
        IQC iq = txa[kTxChannelId].iqc.p;
        QElapsedTimer uhr;
        uhr.start();
        while (!(iq->busy & 1) && uhr.elapsed() < 3000) {
            QThread::msleep(5);
        }
        QVERIFY2(iq->busy & 1,
                 "Der Worker hat die Kurven nie an iqc uebergeben -- die "
                 "Gegenprobe prueft dann nichts.");
        QVERIFY(iq->m_spline[0] != nullptr);

        // Jetzt abbauen. Mit der alten Reihenfolge waere das ein Doppel-Free.
        uhr.restart();
        engine.destroyTxChannel(kTxChannelId);
        QVERIFY2(uhr.elapsed() < 5000,
                 qPrintable(QStringLiteral("Abbau brauchte %1 ms").arg(uhr.elapsed())));
#endif
    }

    // ── 5. Kaestchen -> PureSignal -> Einstellungen ───────────────────────

    void kaestchen_schaltet_und_wird_gemerkt()
    {
        AppSettings::instance().setValue(QStringLiteral("PsMemoryCorrection"),
                                         QStringLiteral("False"));
        TxChannel tx(kTxChannelId);
        {
            PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
            QCOMPARE(ps.memoryCorrection(), false);   // Vorgabe: aus
            PsForm form(nullptr, &ps);
            auto* kaestchen =
                form.findChild<QCheckBox*>(QStringLiteral("chkPSMemoryCorrection"));
            QVERIFY(kaestchen);
            QCOMPARE(kaestchen->isChecked(), false);

            QSignalSpy spion(&ps, &PureSignal::memoryCorrectionChanged);
            kaestchen->click();
            QCOMPARE(spion.count(), 1);
            QCOMPARE(ps.memoryCorrection(), true);
            QCOMPARE(AppSettings::instance()
                         .value(QStringLiteral("PsMemoryCorrection"))
                         .toString(),
                     QStringLiteral("True"));
        }
        // Ein neues PureSignal (naechster Start) liest es zurueck, und das
        // Fenster zeigt es.
        PureSignal ps2(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps2.memoryCorrection(), true);
        PsForm form2(nullptr, &ps2);
        auto* kaestchen2 =
            form2.findChild<QCheckBox*>(QStringLiteral("chkPSMemoryCorrection"));
        QVERIFY(kaestchen2);
        QCOMPARE(kaestchen2->isChecked(), true);

        ps2.setMemoryCorrection(false);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("PsMemoryCorrection"))
                     .toString(),
                 QStringLiteral("False"));
    }
};

QTEST_MAIN(TestPsGedaechtnis)
#include "tst_ps_gedaechtnis.moc"
