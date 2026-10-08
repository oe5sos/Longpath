// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/tst_receiver_manager_log_je_kanal.cpp  (Longpath)
//
// ReceiverManager protokollierte das erste Durchreichen und den ersten
// Wegfall je EINMAL -- global, mit zwei bool.
//
// Kanal 0 kommt immer zuerst. Der Merker war also gesetzt, bevor hw1
// ueberhaupt auftauchte, und fuer den zweiten Empfaenger erschien nie
// eine Zeile. Beim naechsten Zweiempfaenger-Lauf am Geraet haette damit
// nichts belegt, dass Kanal 1 ankommt -- und dieses Projekt hat eine
// eigene Regel dafuer, dass Abwesenheit kein Befund ist.
//
// Seit dem 2026-10-08 je Hardware-Index.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-08 — Original fuer Longpath, KI-gestuetzt (Anthropic
//                 Claude), Betreiber Martin Fischer.
// =================================================================

#include <QtTest>

#include "core/ReceiverManager.h"

using namespace Longpath;

class TestReceiverManagerLogJeKanal : public QObject
{
    Q_OBJECT

private slots:
    // Zwei Hardware-Indizes, fuer die es oben keinen Empfaenger gibt.
    // Beide muessen eine Zeile erzeugen, nicht nur der erste.
    void jederHardwareIndexBekommtSeineEigeneZeile()
    {
        ReceiverManager rm;
        QCOMPARE(rm.geloggteWegfaelleForTest(), 0);

        const QVector<float> proben(8, 0.0f);
        rm.feedIqData(0, proben);
        QCOMPARE(rm.geloggteWegfaelleForTest(), 1);

        // Derselbe Kanal nochmal: keine zweite Zeile, sonst flutet es
        // das Protokoll mit Hunderten je Sekunde.
        rm.feedIqData(0, proben);
        QCOMPARE(rm.geloggteWegfaelleForTest(), 1);

        // Und jetzt der zweite -- DAS ist der Fall, der vorher still war.
        rm.feedIqData(1, proben);
        QVERIFY2(rm.geloggteWegfaelleForTest() == 2,
                 "der zweite Hardware-Index muss eine eigene Zeile "
                 "erzeugen -- sonst belegt beim Zweiempfaenger-Lauf "
                 "nichts, was mit Kanal 1 geschieht");
    }
};

QTEST_MAIN(TestReceiverManagerLogJeKanal)
#include "tst_receiver_manager_log_je_kanal.moc"
