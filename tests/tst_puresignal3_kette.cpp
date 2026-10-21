// no-port-check: Longpath-eigener Prüfstand; die Zitate nennen nur die
// Quellen der Schnittstelle, es ist keine fremde Logik portiert.
// =================================================================
// tests/tst_puresignal3_kette.cpp  (Longpath)
// =================================================================
//
// Trägt PureSignal 3 (WDSP 2.11)? Der Lauf öffnet einen ECHTEN
// WDSP-Sendekanal, fährt die öffentliche PS-Schnittstelle und baut ihn
// wieder ab. Das prüft drei Dinge, die beim Hub von 1.29 auf 2.11
// brechen konnten und die kein Übersetzungslauf sieht:
//
//  1. `create_calcc` legt die NURB-Anpassung an, ohne zu stürzen
//     (TXA.c ruft es mit acht statt siebzehn Werten).
//  2. Der Steuerfaden `doPSCorrChange` läuft an. Er wartet unter Windows
//     mit WaitForMultipleObjects auf fünf Semaphore; auf macOS/Linux
//     läuft er über unseren POSIX-Ersatz in linux_port.c. Wäre der
//     falsch, bliebe `destroy_calcc` hängen — der Lauf liefe in die
//     Zeitüberschreitung statt grün zu enden.
//  3. `GetPSInfo` liefert die sechzehn Felder, aus denen die Anzeige
//     ihren Zustand liest.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Neuer Prüfstand zum PureSignal-3-Hub.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/PureSignal.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"

using namespace Longpath;

namespace {
constexpr int kTxChannelId = 1;
}

class TestPureSignal3Kette : public QObject {
    Q_OBJECT

private slots:
    void sendekanal_mit_ps3_oeffnet_und_schliesst()
    {
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (LONGPATH_BUILD_TESTS)
        engine.openPsFeedbackChannelForTesting();

        // Erste Runde ohne Uhr: der erste Sendekanal eines Prozesses zahlt
        // die einmaligen Kosten mit (Filterstoesse, FFT-Plaene). Gemessen
        // am 2026-10-21: ~12 s beim ersten, ~1,8 s bei jedem weiteren —
        // das ist kein PureSignal-3-Aufwand, und eine Frist darum herum
        // waere eine Frist auf die Aufwaermzeit.
        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);

        // Die Anzeige liest ihren Zustand hier heraus.
        int info[16] = {};
        tx->getPSInfo(info);

        engine.destroyTxChannel(kTxChannelId);

        // Zweite Runde MIT Uhr: haengt der Steuerfaden doPSCorrChange im
        // Abbau (falscher POSIX-Ersatz fuer WaitForMultipleObjects), kommt
        // der Lauf hier nicht mehr heraus.
        QElapsedTimer uhr;
        uhr.start();
        tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);
        tx->getPSInfo(info);
        engine.destroyTxChannel(kTxChannelId);
        QVERIFY2(uhr.elapsed() < 5000,
                 qPrintable(QStringLiteral("Zweiter Auf- und Abbau brauchte %1 ms")
                                .arg(uhr.elapsed())));
#endif
    }

    void zwei_runden_hintereinander()
    {
        // Der Steuerfaden wird je Kanal neu gestartet. Zwei Runden zeigen,
        // dass der alte wirklich endet und kein Semaphor liegen bleibt.
#ifndef HAVE_WDSP
        QSKIP("braucht WDSP");
#else
        for (int runde = 0; runde < 2; ++runde) {
            WdspEngine engine;
            engine.m_initialized = true;
            engine.openPsFeedbackChannelForTesting();
            TxChannel* tx = engine.createTxChannel(kTxChannelId);
            QVERIFY(tx != nullptr);
            engine.destroyTxChannel(kTxChannelId);
        }
#endif
    }

    // ── Die Bitfelder von PureSignal 3, in Worten ─────────────────────────
    //
    // Braucht kein WDSP: describeSolutionBits() liest nur das Feld. Darum
    // laeuft dieser Teil auch dort, wo HAVE_WDSP fehlt -- und genau hier
    // sitzt das Wissen, das am Geraet den Unterschied macht.
    void ohne_befund_wenn_alles_null()
    {
        int info[16] = {};
        QCOMPARE(PureSignal::describeSolutionBits(info),
                 QStringLiteral("no findings"));
    }

    void uebersteuerung_wird_benannt()
    {
        // Das ist Martins Fall vom 2026-10-21: sln.chk = 2 am echten
        // Sender, waehrend cor.cnt weiterstieg.
        int info[16] = {};
        info[6] = 0b0010;
        const QString s = PureSignal::describeSolutionBits(info);
        QVERIFY2(s.contains(QStringLiteral("over-drive")), qPrintable(s));
        QVERIFY2(s.contains(QStringLiteral("fill buckets")), qPrintable(s));
    }

    void ein_gestiegener_zaehler_verdeckt_den_grund_nicht()
    {
        // Gegenprobe zur Falle, die diesen Umbau ausgeloest hat: unter
        // PureSignal 3 KANN info[5] steigen und die Loesung trotzdem
        // verworfen werden (calcc.c:2049 setzt info[6] |= 2 NACH dem
        // memcpy). Ein hoher Zaehler darf den Befund also nicht
        // uebertoenen.
        int info[16] = {};
        info[5] = 99;
        info[7] = 99;
        info[6] = 0b0010;
        QVERIFY(PureSignal::describeSolutionBits(info)
                    .contains(QStringLiteral("over-drive")));
    }

    void die_oberen_bits_gehoeren_je_bauer_woanders()
    {
        // Die unteren vier Bits sind in info[1..3] gleich belegt, die
        // oberen zwei NICHT. Wer sie gemeinsam behandelt, meldet dem
        // Betreiber die falsche Ursache -- deshalb jede einzeln.
        int mag[16] = {};
        mag[1] = 0b00010000;   // count_extrema() Failed
        const QString m = PureSignal::describeSolutionBits(mag);
        QVERIFY2(m.contains(QStringLiteral("count_extrema")), qPrintable(m));
        QVERIFY2(!m.contains(QStringLiteral("sin^2")), qPrintable(m));

        int cos[16] = {};
        cos[2] = 0b00010000;   // sin^2 + cos^2 Error
        const QString c = PureSignal::describeSolutionBits(cos);
        QVERIFY2(c.contains(QStringLiteral("sin^2")), qPrintable(c));
        QVERIFY2(!c.contains(QStringLiteral("count_extrema")), qPrintable(c));

        int kalt[16] = {};
        kalt[3] = 0b00100000;  // SIN Cold Start Error
        const QString k = PureSignal::describeSolutionBits(kalt);
        QVERIFY2(k.contains(QStringLiteral("cs (sine): cold start error")), qPrintable(k));
        QVERIFY2(!k.contains(QStringLiteral("cosine")), qPrintable(k));
        QVERIFY2(!k.contains(QStringLiteral("magnitude")), qPrintable(k));
    }

    void jedes_der_vier_unteren_bits_wird_gemeldet()
    {
        int info[16] = {};
        info[2] = 0b1111;
        const QString s = PureSignal::describeSolutionBits(info);
        QVERIFY2(s.contains(QStringLiteral("cc (cosine): nf_fit() failed")),
                 qPrintable(s));
        QVERIFY2(s.contains(QStringLiteral("cc (cosine): nf_fit() quality bad")),
                 qPrintable(s));
        QVERIFY2(s.contains(QStringLiteral("cc (cosine): ns_build() failed")),
                 qPrintable(s));
        QVERIFY2(s.contains(QStringLiteral("cc (cosine): ns_build() quality bad")),
                 qPrintable(s));
    }

    void leeres_feld_stuerzt_nicht()
    {
        QVERIFY(!PureSignal::describeSolutionBits(nullptr).isEmpty());
    }

    // ── Der Takt der Kalibriertabelle ─────────────────────────────────────
    //
    // Gegenprobe gegen den Fehler, den der Hub auf 2.11 eingebaut hat:
    // PsForm hing die ganze Tabelle an calibrationCountChanged (info[5]).
    // Unter PureSignal 3 steigt info[5] nur bei GELUNGENEN Loesungen --
    // die Tabelle stand also genau dann still, wenn man sie braucht.
    // Dieser Lauf faehrt den Fall: nur info[7] (Versuche) bewegt sich.
    void nur_ein_versuch_mehr_weckt_die_tabelle()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

        QSignalSpy diag(&ps, &PureSignal::psDiagnosticsChanged);
        QSignalSpy zaehler(&ps, &PureSignal::calibrationCountChanged);

        int erste[16] = {};
        erste[5] = 4;
        erste[7] = 4;
        ps.processNewInfo(erste);
        diag.clear();
        zaehler.clear();

        // Ein Versuch mehr, keine Loesung mehr -- und ein Grund dazu.
        int zweite[16] = {};
        zweite[5] = 4;
        zweite[7] = 5;
        zweite[6] = 0b0010;
        ps.processNewInfo(zweite);

        QCOMPARE(diag.count(), 1);
        // Der alte Taktgeber haette hier NICHT gefeuert -- das ist der
        // Beleg, dass das neue Signal gebraucht wird und nicht nur eine
        // zweite Leitung zum selben Ereignis ist.
        QCOMPARE(zaehler.count(), 0);

        // Und die Tabelle liest die NEUEN Werte, nicht die der vorigen
        // Runde: das Signal kommt erst nach dem memcpy auf m_info.
        QCOMPARE(ps.infoAt(7), 5);
        QCOMPARE(ps.infoAt(6), 0b0010);
    }

    void ohne_aenderung_bleibt_es_still()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        int info[16] = {};
        info[5] = 1;
        ps.processNewInfo(info);

        QSignalSpy diag(&ps, &PureSignal::psDiagnosticsChanged);
        ps.processNewInfo(info);
        QCOMPARE(diag.count(), 0);
    }
};

QTEST_MAIN(TestPureSignal3Kette)
#include "tst_puresignal3_kette.moc"
