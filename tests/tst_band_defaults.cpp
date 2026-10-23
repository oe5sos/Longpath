// tst_band_defaults.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here. Thetis cites are in BandDefaults.cpp.
//
// Seed-table integrity tests for BandDefaults (#118).
// Frequencies sourced verbatim from Thetis Region 2 seeds:
//   clsBandStackManager.cs:2099-2176 [v2.10.3.13 @ 501e3f51]

#include <QtTest/QtTest>

#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/BandDefaults.h"

using namespace Longpath;

class TestBandDefaults : public QObject {
    Q_OBJECT

private slots:
    void every_ham_band_has_valid_seed() {
        for (Band b : { Band::Band160m, Band::Band80m, Band::Band60m,
                        Band::Band40m,  Band::Band30m, Band::Band20m,
                        Band::Band17m,  Band::Band15m, Band::Band12m,
                        Band::Band10m,  Band::Band6m,  Band::WWV,
                        Band::GEN }) {
            BandSeed s = BandDefaults::seedFor(b);
            QVERIFY2(s.valid, qPrintable(QStringLiteral("Band %1 must have a valid seed")
                                          .arg(bandLabel(b))));
            QCOMPARE(s.band, b);
            QVERIFY(s.frequencyHz > 0.0);
        }
    }

    void xvtr_seed_is_invalid() {
        BandSeed s = BandDefaults::seedFor(Band::XVTR);
        QVERIFY(!s.valid);
    }

    void seed_freq_lands_inside_its_own_band() {
        // Skips GEN (fallback band — any-freq maps to it) and WWV
        // (detected at discrete centers ± window, so the 10 MHz seed
        // lands in WWV, not a ham band).
        for (Band b : { Band::Band160m, Band::Band80m, Band::Band60m,
                        Band::Band40m,  Band::Band30m, Band::Band20m,
                        Band::Band17m,  Band::Band15m, Band::Band12m,
                        Band::Band10m,  Band::Band6m }) {
            BandSeed s = BandDefaults::seedFor(b);
            Band derived = bandFromFrequency(s.frequencyHz);
            QVERIFY2(derived == b,
                     qPrintable(QStringLiteral("Seed for %1 (%2 Hz) derives to %3")
                                  .arg(bandLabel(b))
                                  .arg(s.frequencyHz)
                                  .arg(bandLabel(derived))));
        }
    }

    void wwv_seed_maps_to_wwv() {
        BandSeed s = BandDefaults::seedFor(Band::WWV);
        QCOMPARE(bandFromFrequency(s.frequencyHz), Band::WWV);
    }

    void seeds_match_thetis_region2_values() {
        // Exact values from clsBandStackManager.cs:2104-2168 (first voice
        // entry per band, Thetis v2.10.3.13).
        struct Expected { Band band; double hz; DSPMode mode; };
        const Expected table[] = {
            { Band::Band160m, 1840000.0,  DSPMode::LSB },   // :2104
            { Band::Band80m,  3650000.0,  DSPMode::LSB },   // :2109
            { Band::Band60m,  5354000.0,  DSPMode::USB },   // :2114
            { Band::Band40m,  7152000.0,  DSPMode::LSB },   // :2120
            { Band::Band30m,  10107000.0, DSPMode::CWU },   // :2123 (CW only)
            { Band::Band20m,  14155000.0, DSPMode::USB },   // :2130
            { Band::Band17m,  18125000.0, DSPMode::USB },   // :2135
            { Band::Band15m,  21205000.0, DSPMode::USB },   // :2140
            { Band::Band12m,  24931000.0, DSPMode::USB },   // :2145
            { Band::Band10m,  28305000.0, DSPMode::USB },   // :2150
            { Band::Band6m,   50125000.0, DSPMode::USB },   // :2155
            { Band::WWV,      10000000.0, DSPMode::SAM },   // :2165
            { Band::GEN,      13845000.0, DSPMode::SAM },   // :2168
        };
        for (const Expected& e : table) {
            BandSeed s = BandDefaults::seedFor(e.band);
            QCOMPARE(s.frequencyHz, e.hz);
            QCOMPARE(s.mode, e.mode);
        }
    }

    // ── Das Seitenband je Band (2026-10-23) ─────────────────────────
    //
    // Betreiber: „80 und 160 sowie 40 muessen immer automatisch bei
    // bandauswahl LSB sein". Die Startwerte oben tragen das schon,
    // greifen aber nur beim ERSTEN Besuch; danach stellt das
    // Gedaechtnis je Band wieder her, was zuletzt dort stand.
    void dieDreiUnterenBaenderSindLsb() {
        QCOMPARE(BandDefaults::seitenbandFuer(Band::Band160m), DSPMode::LSB);
        QCOMPARE(BandDefaults::seitenbandFuer(Band::Band80m),  DSPMode::LSB);
        QCOMPARE(BandDefaults::seitenbandFuer(Band::Band40m),  DSPMode::LSB);
        // 60 m liegt auch unter 10 MHz, ist aber kanalisiert USB.
        QCOMPARE(BandDefaults::seitenbandFuer(Band::Band60m),  DSPMode::USB);
        QCOMPARE(BandDefaults::seitenbandFuer(Band::Band20m),  DSPMode::USB);
        QCOMPARE(BandDefaults::seitenbandFuer(Band::Band10m),  DSPMode::USB);
    }

    void eineVerdrehteSeite_wirdGerichtet() {
        // Genau der Fall des Betreibers: USB lag auf 80 m.
        QCOMPARE(BandDefaults::korrigiereSeitenband(Band::Band80m,
                                                    DSPMode::USB),
                 DSPMode::LSB);
        QCOMPARE(BandDefaults::korrigiereSeitenband(Band::Band20m,
                                                    DSPMode::LSB),
                 DSPMode::USB);
        // Richtig bleibt richtig.
        QCOMPARE(BandDefaults::korrigiereSeitenband(Band::Band40m,
                                                    DSPMode::LSB),
                 DSPMode::LSB);
    }

    void cwUndDigitalBleibenStehen() {
        // Ein auf 40 m abgelegtes DIGU gehoert zu FT8 — kein
        // verdrehtes Seitenband. Dasselbe fuer CW, AM, FM.
        const DSPMode fremd[] = { DSPMode::CWL, DSPMode::CWU,
                                  DSPMode::DIGU, DSPMode::DIGL,
                                  DSPMode::AM,  DSPMode::SAM,
                                  DSPMode::FM };
        for (DSPMode m : fremd) {
            QCOMPARE(BandDefaults::korrigiereSeitenband(Band::Band40m, m), m);
            QCOMPARE(BandDefaults::korrigiereSeitenband(Band::Band20m, m), m);
        }
    }

    // Die Startwerte und die Regel muessen dasselbe sagen, sonst
    // haengt das Seitenband davon ab, ob man zum ersten oder zum
    // zweiten Mal auf das Band drueckt.
    void startwerteUndRegelStimmenUeberein() {
        for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
            const Band b = static_cast<Band>(i);
            const BandSeed s = BandDefaults::seedFor(b);
            if (!s.valid) { continue; }
            if (s.mode != DSPMode::USB && s.mode != DSPMode::LSB) { continue; }
            QVERIFY2(s.mode == BandDefaults::seitenbandFuer(b),
                     qPrintable(QStringLiteral(
                         "%1: Startwert und Regel widersprechen sich")
                         .arg(bandLabel(b))));
        }
    }
};

QTEST_MAIN(TestBandDefaults)
#include "tst_band_defaults.moc"
