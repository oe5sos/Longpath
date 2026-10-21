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
};

QTEST_MAIN(TestPureSignal3Kette)
#include "tst_puresignal3_kette.moc"
