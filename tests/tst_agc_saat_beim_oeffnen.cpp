// no-port-check: Longpath-eigener Prüfstand. Die Zitate nennen die
// Quellen der Zahlen; es ist keine Thetis-Logik portiert.
// =================================================================
// tests/tst_agc_saat_beim_oeffnen.cpp  (Longpath)
// =================================================================
//
// Welche AGC-Verstärkung ein frisch geöffneter WDSP-Empfänger WIRKLICH
// fährt — nicht welche Longpath anzeigt.
//
// WDSP öffnet jeden Empfänger mit create_wcpagc's fixed_gain = 1000.0
// (linear, 60 dB) und max_gain = 10000.0 (80 dB),
// third_party/wdsp/src/rxa.c:380/382. RxChannel cached 20 bzw. 90 dB
// (Thetis setup.designer.cs:39320 / :39245 [v2.10.3.13]) und seine Setter
// kehren bei einem Wert gleich dem gecachten früh zurück. Ohne Saat beim
// Öffnen erreichte der Stoß der Scheibe WDSP nie: AGC Off lief 60 statt
// 20 dB (40 dB zu heiss) und Max Gain 80 statt 90 dB — nach jedem
// Verbinden und jedem Neubau, und ohne eine einzige Zeile im Protokoll.
//
// Gefunden im Upstream NereusSDR (26395cc24 / af7083e45, Fassung
// 2026.10.1) und hier nachgemessen.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Neuer Prüfstand: AGC-Saat beim Öffnen des Kanals.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"

#include <cmath>

#ifdef HAVE_WDSP
// Den angewandten Wert vom WDSP-Empfänger selbst lesen, nicht aus
// Longpaths Cache — sonst prüft der Lauf nur sich selbst.
extern "C" {
#include "../third_party/wdsp/src/comm.h"
}
#undef min
#undef max
#endif

using namespace Longpath;

namespace {

constexpr int kChannelId = 0;
constexpr int kRateHz    = 192000;

#ifdef HAVE_WDSP
// SetRXAAGCFixed legt pow(10, dB/20) ab (third_party/wdsp/src/wcpAGC.c:549).
double appliedFixedGainDb(int channelId)
{
    return 20.0 * std::log10(rxa[channelId].agc.p->fixed_gain);
}

// SetRXAAGCTop legt max_gain ebenso logarithmisch ab (wcpAGC.c).
double appliedMaxGainDb(int channelId)
{
    return 20.0 * std::log10(rxa[channelId].agc.p->max_gain);
}
#endif

} // namespace

class TestAgcSaatBeimOeffnen : public QObject {
    Q_OBJECT

private slots:
    // Der geöffnete Kanal fährt die Werte, die RxChannel meldet.
    void geoeffneter_kanal_faehrt_die_gecachten_werte()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (LONGPATH_BUILD_TESTS)
        RxChannel* rx = engine.createRxChannel(kChannelId, 1024, 4096,
                                               kRateHz, 48000, 48000);
        QVERIFY(rx != nullptr);

        QCOMPARE(rx->agcFixedGain(), RxChannel::kDefaultAgcFixedGainDb);
        QCOMPARE(rx->agcMaxGain(),   RxChannel::kDefaultAgcMaxGainDb);

        // Das ist der Befund: ohne Saat stehen hier 60 und 80.
        QCOMPARE(qRound(appliedFixedGainDb(kChannelId)),
                 RxChannel::kDefaultAgcFixedGainDb);
        QCOMPARE(qRound(appliedMaxGainDb(kChannelId)),
                 RxChannel::kDefaultAgcMaxGainDb);

        engine.destroyRxChannel(kChannelId);
#endif
    }

    // Der Stoß der Scheibe mit genau dem Vorgabewert darf nichts
    // verschieben — die Gleichheitswache bleibt, sie ist jetzt nur nicht
    // mehr der einzige Weg zu WDSP.
    void vorgabestoss_laesst_den_wert_wo_er_ist()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* rx = engine.createRxChannel(kChannelId, 1024, 4096,
                                               kRateHz, 48000, 48000);
        QVERIFY(rx != nullptr);

        rx->setAgcFixedGain(RxChannel::kDefaultAgcFixedGainDb);
        rx->setAgcMaxGain(RxChannel::kDefaultAgcMaxGainDb);
        QCOMPARE(qRound(appliedFixedGainDb(kChannelId)),
                 RxChannel::kDefaultAgcFixedGainDb);
        QCOMPARE(qRound(appliedMaxGainDb(kChannelId)),
                 RxChannel::kDefaultAgcMaxGainDb);

        engine.destroyRxChannel(kChannelId);
#endif
    }

    // Und ein echter Zugriff kommt weiterhin durch.
    void geaenderter_wert_kommt_in_wdsp_an()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* rx = engine.createRxChannel(kChannelId, 1024, 4096,
                                               kRateHz, 48000, 48000);
        QVERIFY(rx != nullptr);

        rx->setAgcFixedGain(35);
        QCOMPARE(qRound(appliedFixedGainDb(kChannelId)), 35);
        rx->setAgcMaxGain(100);
        QCOMPARE(qRound(appliedMaxGainDb(kChannelId)), 100);

        engine.destroyRxChannel(kChannelId);
#endif
    }
};

QTEST_MAIN(TestAgcSaatBeimOeffnen)
#include "tst_agc_saat_beim_oeffnen.moc"
