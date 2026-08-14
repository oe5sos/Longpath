// tests/tst_band_tables_agree.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port — this is about a disagreement
// between a ported table and one of ours.
//
// ── Two band tables, and only one of them gates the transmitter ──────
//
// This program carries the amateur band edges twice:
//
//   core/antenna/AmateurBands.cpp   — draws the chart, shades the bands,
//                                     names them, decides "usable span"
//   core/safety/BandPlanGuard.cpp   — decides whether MOX may key, and
//                                     where a sweep is allowed to
//                                     transmit
//
// On 2026-08-14 they were found to disagree on 60 m in Region 1 by a
// factor of twenty-seven:
//
//   AmateurBands  5.3515 – 5.3665 MHz   (15 kHz — the real allocation)
//   BandPlanGuard 5.100  – 5.500  MHz   (400 kHz — ported from Thetis)
//
// The permissive one is the one with authority, so a range sweep across
// HF keyed 385 kHz outside the allocation, and MOX would have permitted
// it by hand too. Every other band in Region 1 matched exactly, which
// is what makes this worth pinning: the tables are nearly consistent,
// so the one place they are not is easy to miss and stays missed.
//
// This test is the guard rail. It does not decide what the 60 m edges
// should be — that is the licence holder's call and is recorded as a
// task. It fails if any OTHER band ever drifts apart, and it fails if
// 60 m is changed on one side without the other.

#include <QtTest>

#include <algorithm>

#include "core/antenna/AmateurBands.h"
#include "core/safety/BandPlanGuard.h"
#include "models/Band.h"

using namespace NereusSDR;

namespace {

struct Pairing {
    const char* name;       // as AmateurBands names it
    Band        band;       // as BandPlanGuard knows it
};

// Everything both tables carry for HF plus 6 m. 2 m and 70 cm are in
// AmateurBands only.
const Pairing kPairs[] = {
    { "160 m", Band::Band160m },
    { "80 m",  Band::Band80m  },
    { "60 m",  Band::Band60m  },
    { "40 m",  Band::Band40m  },
    { "30 m",  Band::Band30m  },
    { "20 m",  Band::Band20m  },
    { "17 m",  Band::Band17m  },
    { "15 m",  Band::Band15m  },
    { "12 m",  Band::Band12m  },
    { "10 m",  Band::Band10m  },
    { "6 m",   Band::Band6m   },
};

AmateurBands::Band drawn(const char* name, AmateurBands::Region r)
{
    for (const auto& b : AmateurBands::forRegion(r)) {
        if (b.name == QString::fromLatin1(name)) { return b; }
    }
    return {};
}

/// Walk the guard on a 500 Hz grid and report the widest contiguous run
/// it permits around the band. Reading the private tables is not
/// possible from here, and probing is the honest way anyway: it tests
/// what the guard actually DOES, not what its arrays say.
QPair<double, double> permitted(const safety::BandPlanGuard& g,
                                safety::Region region,
                                double nearHz, DSPMode mode)
{
    constexpr double kStep = 500.0;
    auto ok = [&](double f) {
        return g.isValidTxFreq(region, static_cast<std::int64_t>(f), mode,
                               /*extended=*/false);
    };
    if (!ok(nearHz)) { return {0.0, 0.0}; }
    double lo = nearHz;
    while (lo - kStep > 0.0 && ok(lo - kStep)) { lo -= kStep; }
    double hi = nearHz;
    while (ok(hi + kStep)) { hi += kStep; }
    return {lo, hi};
}

} // namespace

class TestBandTablesAgree : public QObject
{
    Q_OBJECT

private slots:
    // ── The one that would have caught the 60 m fault ────────────────
    void region1_drawnEdgesMatchWhatMayBeTransmitted()
    {
        safety::BandPlanGuard g;
        QStringList drift;

        for (const Pairing& p : kPairs) {
            const AmateurBands::Band d =
                drawn(p.name, AmateurBands::Region::One);
            if (!d.isValid()) { continue; }

            const auto allowed = permitted(g, safety::Region::Europe,
                                           d.centreHz(), DSPMode::LSB);
            if (allowed.second <= allowed.first) {
                drift << QStringLiteral("%1: the guard permits nothing at "
                                        "the band centre").arg(p.name);
                continue;
            }
            // 1 kHz of slack: the probe grid is 500 Hz and the tables
            // are written to the kilohertz.
            const double lowSlop  = d.lowHz  - allowed.first;
            const double highSlop = allowed.second - d.highHz;
            if (lowSlop > 1000.0 || highSlop > 1000.0) {
                drift << QStringLiteral(
                    "%1: drawn %2–%3, but the guard permits %4–%5 — "
                    "%6 kHz of transmitting outside the band as drawn")
                    .arg(p.name)
                    .arg(d.lowHz / 1e6, 0, 'f', 4)
                    .arg(d.highHz / 1e6, 0, 'f', 4)
                    .arg(allowed.first / 1e6, 0, 'f', 4)
                    .arg(allowed.second / 1e6, 0, 'f', 4)
                    .arg((std::max(0.0, lowSlop) + std::max(0.0, highSlop))
                             / 1e3, 0, 'f', 1);
            }
        }

        // No exceptions. There was one — 60 m — and the assertion that
        // guarded it did its job on the first run after the table was
        // corrected: it failed, saying "remove it rather than leaving a
        // standing exception nobody rechecks". So it is removed.
        QVERIFY2(drift.isEmpty(),
                 qPrintable(QStringLiteral(
                     "the chart and the transmit guard have drifted "
                     "apart:\n  %1").arg(drift.join("\n  "))));
    }

    // The guard must never permit LESS than the chart shows either:
    // that direction costs the operator band he is entitled to, and it
    // is just as much a disagreement.
    void region1_theGuardDoesNotWithholdBandTheChartOffers()
    {
        safety::BandPlanGuard g;
        for (const Pairing& p : kPairs) {
            const AmateurBands::Band d =
                drawn(p.name, AmateurBands::Region::One);
            if (!d.isValid()) { continue; }

            for (double f : { d.lowHz + 1000.0, d.centreHz(),
                              d.highHz - 1000.0 }) {
                QVERIFY2(g.isValidTxFreq(safety::Region::Europe,
                                         static_cast<std::int64_t>(f),
                                         DSPMode::LSB, false),
                         qPrintable(QStringLiteral(
                             "%1: the chart shades %2 MHz but the guard "
                             "refuses to transmit there")
                                 .arg(p.name).arg(f / 1e6, 0, 'f', 4)));
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestBandTablesAgree)
#include "tst_band_tables_agree.moc"
