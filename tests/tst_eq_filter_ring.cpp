// =================================================================
// tests/tst_eq_filter_ring.cpp  (NereusSDR)
// =================================================================
//
// Three gestures change a band's filter type — clicking its icon,
// double-clicking its handle on the curve, and the right-click menu —
// and for a while they were three separate implementations. The one
// added last walked the ring with the two slopes the other way round,
// which nobody would ever report as a bug: the operator just decides
// the equaliser is unpredictable.
//
// So the ring and the write are one function now, and these tests hold
// it to the two things the gestures promise. The Q clamp is the half
// worth reading: it is the only place where changing a type changes a
// number the operator did not touch, and the reason it does is that Q 8
// means "narrow notch" on a peak and "resonant hump on the corner" on a
// high-pass.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/eq/EqFilterRing.h"
#include "core/strip/StripChain.h"

#include <QtTest/QtTest>

#include <memory>

using namespace NereusSDR;
using FT = ClientEq::FilterType;
namespace Ring = NereusSDR::EqFilterRing;

namespace {

std::unique_ptr<StripChain> makeChain()
{
    auto c = std::make_unique<StripChain>();
    c->prepare(48000.0);
    return c;
}

} // namespace

class TstEqFilterRing : public QObject {
    Q_OBJECT
private slots:

    // ── The ring ─────────────────────────────────────────────────────

    void stepping_forward_visits_every_type_and_comes_home()
    {
        // A ring that misses a type makes it unreachable by the gesture
        // people use most; one that comes home early makes two of them
        // unreachable and does it silently.
        QSet<int> seen;
        FT t = FT::Peak;
        for (int i = 0; i < Ring::kTypeCount; ++i) {
            seen.insert(static_cast<int>(t));
            t = Ring::next(t);
        }
        QCOMPARE(seen.size(), Ring::kTypeCount);
        QVERIFY2(t == FT::Peak, "a full lap did not return to the start");
    }

    void stepping_back_undoes_stepping_forward()
    {
        for (int i = 0; i < Ring::kTypeCount; ++i) {
            const auto t = static_cast<FT>(i);
            QVERIFY(Ring::prev(Ring::next(t)) == t);
            QVERIFY(Ring::next(Ring::prev(t)) == t);
        }
    }

    // Shift-click on the first entry is the case that would index -1 in
    // a naive implementation.
    void stepping_back_from_the_first_type_wraps_rather_than_underflows()
    {
        const FT t = Ring::prev(FT::Peak);
        QVERIFY(static_cast<int>(t) >= 0);
        QVERIFY(static_cast<int>(t) < Ring::kTypeCount);
        QVERIFY(t == FT::HighPass);
    }

    void a_direction_larger_than_the_ring_still_lands_on_the_ring()
    {
        // So a wheel event can be handed straight in.
        for (int d : {-13, -5, 0, 7, 26}) {
            const FT t = Ring::step(FT::HighShelf, d);
            QVERIFY(static_cast<int>(t) >= 0);
            QVERIFY(static_cast<int>(t) < Ring::kTypeCount);
        }
        QVERIFY(Ring::step(FT::Peak, Ring::kTypeCount) == FT::Peak);
        QVERIFY(Ring::step(FT::Peak, 0) == FT::Peak);
    }

    void every_type_has_a_name()
    {
        QSet<QString> names;
        for (int i = 0; i < Ring::kTypeCount; ++i) {
            const QString n =
                QString::fromLatin1(Ring::name(static_cast<FT>(i)));
            QVERIFY(!n.isEmpty());
            names.insert(n);
        }
        // Two types sharing a label is a menu that lies.
        QCOMPARE(names.size(), Ring::kTypeCount);
    }

    // ── The write ────────────────────────────────────────────────────

    void changing_the_type_switches_the_band_on()
    {
        auto c = makeChain();
        ClientEq::BandParams p = c->eq().band(2);
        p.enabled = false;
        c->eq().setBand(2, p);

        Ring::applyTo(c->eq(), 2, FT::HighShelf);
        QVERIFY2(c->eq().band(2).enabled,
                 "reshaping a band left it bypassed");
    }

    // The one place a type change touches a number the operator did not.
    void a_sharp_peak_does_not_become_a_resonant_slope()
    {
        auto c = makeChain();
        ClientEq::BandParams p = c->eq().band(3);
        p.type = FT::Peak;
        p.q    = 8.0f;
        c->eq().setBand(3, p);

        for (FT t : {FT::LowShelf, FT::HighShelf, FT::LowPass, FT::HighPass}) {
            ClientEq::BandParams sharp = c->eq().band(3);
            sharp.type = FT::Peak;
            sharp.q    = 8.0f;
            c->eq().setBand(3, sharp);

            Ring::applyTo(c->eq(), 3, t);
            QVERIFY2(c->eq().band(3).q <= 1.0f,
                     qPrintable(QStringLiteral("%1 kept Q at %2")
                                    .arg(QLatin1String(Ring::name(t)))
                                    .arg(double(c->eq().band(3).q))));
        }
    }

    void a_peak_keeps_whatever_q_it_had()
    {
        // The clamp is about shelves and slopes. A narrow peak is a
        // notch and entirely legitimate.
        auto c = makeChain();
        ClientEq::BandParams p = c->eq().band(4);
        p.type = FT::LowShelf;
        p.q    = 0.9f;
        c->eq().setBand(4, p);

        Ring::applyTo(c->eq(), 4, FT::Peak);
        // Compare as FLOAT: q is stored as float, and 0.9 has no exact
        // float representation — promoting to double manufactures a
        // difference (0.8999999761…) that QCOMPARE's double-epsilon
        // rejects. Bitten on the first full-suite run, 2026-08-11.
        QCOMPARE(c->eq().band(4).q, 0.9f);
    }

    void the_clamp_only_ever_lowers_q()
    {
        // A wide shelf must not be sharpened on the way through.
        auto c = makeChain();
        ClientEq::BandParams p = c->eq().band(5);
        p.type = FT::Peak;
        p.q    = 0.3f;
        c->eq().setBand(5, p);

        Ring::applyTo(c->eq(), 5, FT::HighPass);
        QCOMPARE(c->eq().band(5).q, 0.3f);  // float compare — see above
    }

    // Slopes ignore gain. If the write cleared it, a stray double-click
    // would silently throw away a shaped band, and the operator would
    // find out one lap later when it came back flat.
    void a_full_lap_gives_back_the_gain_it_started_with()
    {
        auto c = makeChain();
        ClientEq::BandParams p = c->eq().band(6);
        p.type   = FT::Peak;
        p.gainDb = -4.5f;
        p.freqHz = 1750.0f;
        p.q      = 0.8f;
        c->eq().setBand(6, p);

        FT t = FT::Peak;
        for (int i = 0; i < Ring::kTypeCount; ++i) {
            t = Ring::next(t);
            Ring::applyTo(c->eq(), 6, t);
        }
        const auto after = c->eq().band(6);
        QVERIFY2(after.type == FT::Peak, "a full lap changed the type");
        QCOMPARE(double(after.gainDb), -4.5);
        QCOMPARE(double(after.freqHz), 1750.0);
        // Q is the one thing a lap is allowed to change, because the lap
        // passes through four types that cannot hold a sharp one.
        QCOMPARE(after.q, 0.8f);  // float compare — see above
    }
};

QTEST_APPLESS_MAIN(TstEqFilterRing)
#include "tst_eq_filter_ring.moc"
