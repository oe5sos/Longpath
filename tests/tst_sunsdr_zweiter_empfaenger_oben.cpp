// Kommt der zweite Strom der QRP oben als zweiter Empfaenger an?
//
// Am 2026-10-04 wurde am Geraet belegt, dass der Treiber Kanal 1 nicht
// mehr verwirft (b33072d9: beide Kanaele, 0 verworfen). Das ist aber nur
// die halbe Strecke -- gemessen wurde die TREIBERSEITE. Ob der Kanal
// oben auch bei einem zweiten Empfaenger landet, haengt an
// ReceiverManager::rebuildHardwareMapping, und das war nicht geprueft.
//
// Diese Pruefung schliesst die Luecke ohne Funkgeraet: zwei Empfaenger
// anlegen, Kanal 1 einspeisen, und nachsehen, bei wem er herauskommt.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/ReceiverManager.h"

using Longpath::ReceiverManager;

class TstSunSdrZweiterEmpfaengerOben : public QObject
{
    Q_OBJECT

private slots:
    // Mit nur einem Empfaenger gibt es fuer Kanal 1 niemanden -- das Paket
    // muss fallen, nicht beim ersten landen. Waechter: ein zweiter Strom,
    // den niemand hoert, darf nicht in den ersten Empfaenger laufen.
    void kanalEinsOhneZweitenEmpfaengerLandetNirgends()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);
        QCOMPARE(rm.createReceiver(), 0);
        rm.activateReceiver(0);

        QSignalSpy spy(&rm, &ReceiverManager::iqDataForReceiver);
        rm.feedIqData(1, QVector<float>{0.1f, 0.2f});
        QCOMPARE(spy.count(), 0);

        rm.feedIqData(0, QVector<float>{0.3f, 0.4f});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);
    }

    // Mit zwei Empfaengern muss Kanal 1 beim ZWEITEN ankommen.
    void kanalEinsLandetBeimZweitenEmpfaenger()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);
        QCOMPARE(rm.createReceiver(), 0);
        QCOMPARE(rm.createReceiver(), 1);
        rm.activateReceiver(0);
        rm.activateReceiver(1);

        QSignalSpy spy(&rm, &ReceiverManager::iqDataForReceiver);

        rm.feedIqData(0, QVector<float>{0.1f, 0.2f});
        rm.feedIqData(1, QVector<float>{0.3f, 0.4f});

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(1).at(0).toInt(), 1);

        // Und wirklich die Daten des jeweiligen Kanals, nicht zweimal
        // dieselben -- sonst waere die Abbildung zwar da, aber falsch.
        const auto zweite = spy.at(1).at(1).value<QVector<float>>();
        QVERIFY(zweite.size() >= 2);
        QVERIFY(qFuzzyCompare(zweite[0], 0.3f));
    }
};

QTEST_MAIN(TstSunSdrZweiterEmpfaengerOben)
#include "tst_sunsdr_zweiter_empfaenger_oben.moc"
