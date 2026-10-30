// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_panel_automatik.cpp  (Longpath)
// =================================================================
//
// Ein Panel, das von selbst auf- und zugeht. Betreiber am
// 2026-10-30: „schoen waere, wenn hier die fenster eigens aufgehen
// und danach wieder zu gehen."
//
// Die Zeit kommt von Hand herein; eine Nachlaufzeit von 45 Sekunden
// laeuft darum in einer Millisekunde durch.
// =================================================================

#include <QtTest/QtTest>

#include "gui/applets/PanelAutomatik.h"

using namespace Longpath;
using Tat = PanelAutomatik::Tat;

class TstPanelAutomatik : public QObject
{
    Q_OBJECT

private slots:
    // ── Der gewuenschte Ablauf, von vorn bis hinten ───────────────
    void einQsoMachtAufUndDanachGehtEsZu()
    {
        PanelAutomatik a;
        qint64 t = 1000000;

        // Nichts los, Panel zu: es bleibt zu.
        QCOMPARE(a.pruefe(false, false, t), Tat::Nichts);

        // Ein QSO faengt an.
        QCOMPARE(a.pruefe(true, false, t += 1000), Tat::Aufmachen);
        QVERIFY(a.vonUnsAufgemacht());
        // Solange es laeuft, bleibt es -- und wird nicht nochmal
        // aufgemacht.
        QCOMPARE(a.pruefe(true, true, t += 1000), Tat::Nichts);
        QCOMPARE(a.pruefe(true, true, t += 1000), Tat::Nichts);

        // Fertig. Jetzt laeuft die Nachlaufzeit.
        QCOMPARE(a.pruefe(false, true, t += 1000), Tat::Nichts);
        QCOMPARE(a.pruefe(false, true, t + 1000), Tat::Nichts);
        QCOMPARE(a.pruefe(false, true,
                          t + PanelAutomatik::kNachlaufMs - 1), Tat::Nichts);

        // Und jetzt zu.
        QCOMPARE(a.pruefe(false, true,
                          t + PanelAutomatik::kNachlaufMs), Tat::Zumachen);
        QVERIFY(!a.vonUnsAufgemacht());
    }

    // ── Die Nachlaufzeit ueberbrueckt die Luecke zwischen QSOs ────
    //
    // Bei FT8 liegt zwischen zwei QSOs eine halbe Minute, in der
    // nichts laeuft. Ohne Nachlauf ginge das Panel nach jedem RR73
    // zu und beim naechsten Anrufer wieder auf -- ein Fenster, das
    // im Takt des Bandes blinkt.
    void zwischenZweiQsosBleibtEsStehen()
    {
        PanelAutomatik a;
        qint64 t = 0;
        QCOMPARE(a.pruefe(true, false, t), Tat::Aufmachen);

        // QSO fertig, 30 s Pause -- das ist eine FT8-Luecke.
        a.pruefe(false, true, t += 1000);
        QCOMPARE(a.pruefe(false, true, t + 30000), Tat::Nichts);

        // Der naechste Anrufer kommt: nichts passiert, es steht ja
        // schon offen.
        QCOMPARE(a.pruefe(true, true, t + 31000), Tat::Nichts);
        QVERIFY2(a.vonUnsAufgemacht(),
                 "es gehoert weiterhin der Automatik");
    }

    // ── Wer selbst greift, bestimmt ───────────────────────────────
    void nachEinemGriffDesBetreibersGehoertEsIhm()
    {
        PanelAutomatik a;
        qint64 t = 0;
        QCOMPARE(a.pruefe(true, false, t), Tat::Aufmachen);

        // Er macht es zu, waehrend noch ein QSO laeuft.
        a.betreiberHatGegriffen();
        QCOMPARE(a.pruefe(true, false, t += 1000), Tat::Nichts);
        // Und es wird ihm auch nicht wieder aufgedraengt.
        QCOMPARE(a.pruefe(true, false, t += 60000), Tat::Nichts);
        // Noch wird es zugemacht, wenn er es offen haben will.
        QCOMPARE(a.pruefe(false, true, t += 60000), Tat::Nichts);
        QCOMPARE(a.pruefe(false, true, t += 60000), Tat::Nichts);
    }

    // Was wir nicht aufgemacht haben, machen wir auch nicht zu --
    // sonst schloesse die Automatik ein Panel, das der Betreiber
    // von Anfang an offen hatte.
    void einVonHandGeoeffnetesPanelWirdNichtZugemacht()
    {
        PanelAutomatik a;
        qint64 t = 0;
        // Es ist bereits offen, ohne dass wir es waren.
        QCOMPARE(a.pruefe(true, true, t), Tat::Nichts);
        QVERIFY(!a.vonUnsAufgemacht());

        a.pruefe(false, true, t += 1000);
        QCOMPARE(a.pruefe(false, true, t + PanelAutomatik::kNachlaufMs * 2),
                 Tat::Nichts);
    }

    void zuruecksetzenFaengtVonVornAn()
    {
        PanelAutomatik a;
        a.pruefe(true, false, 0);
        a.betreiberHatGegriffen();
        QVERIFY(a.gehoertDemBetreiber());
        a.zuruecksetzen();
        QVERIFY(!a.gehoertDemBetreiber());
        QCOMPARE(a.pruefe(true, false, 0), Tat::Aufmachen);
    }
};

QTEST_APPLESS_MAIN(TstPanelAutomatik)
#include "tst_panel_automatik.moc"
