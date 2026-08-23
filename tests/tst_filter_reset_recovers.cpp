// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Rueckstellknopf holt einen aus einem KAPUTTEN Durchlass heraus,
// nicht nur aus einer verschobenen Mitte.
//
// Anlass, 2026-08-23. Der Betreiber hoerte auf 40 m nichts
// Verstaendliches, suchte eine Weile mit mir zusammen nach der
// Ursache (ich war bei Paketverlust und bei der Entschachtelung der
// Empfaenger) und fand sie dann selbst:
//
//   "es funktioniert, die bandweite war komplett falsch, start bei
//    2000."
//
// Bei 2000 Hz Untergrenze ist von Sprache praktisch nichts mehr uebrig
// — die traegt zwischen 300 und 3000 Hz, der Verstaendlichkeitsanteil
// liegt unter 2000. Man hoert etwas, aber man versteht es nicht. Genau
// so hat er es beschrieben.
//
// Der Weg dorthin ist lautlos: die Filterkanten im Panadapter sind
// seit dieser Woche ziehbar, und ein Durchlass von 2000 bis 2800 sieht
// auf dem Bild nicht falsch aus.
//
// Der Knopf, der danach aussieht, als hole er einen da heraus, tat es
// nicht: er rief resetFilterCenter(), und das ZENTRIERT bei
// gleichbleibender Breite. Aus einer kaputten Breite kommt man damit
// nicht zurueck.
//
// Diese Pruefung haelt beides fest — dass der Knopf die Breite
// wiederherstellt UND dass er die Lage stimmen laesst.

#include <QtTest>

#include "models/SliceModel.h"

using namespace Longpath;

class TstFilterResetRecovers : public QObject
{
    Q_OBJECT

private slots:
    void ausDemSchlitzBeiZweitausend()
    {
        SliceModel slice(0);
        slice.setDspMode(DSPMode::LSB);

        // Genau der Zustand vom 2026-08-23: ein schmaler Schlitz weit
        // oben. Bei LSB gespiegelt, also negativ.
        slice.setFilter(-2800, -2000);
        QCOMPARE(slice.filterWidth(), 800);
        qInfo() << "vorher:" << slice.filterLow() << ".." << slice.filterHigh();

        slice.resetFilter();
        qInfo() << "nachher:" << slice.filterLow() << ".." << slice.filterHigh();

        // Die Breite muss wieder sprechtauglich sein.
        QVERIFY2(slice.filterWidth() >= 2400,
                 qPrintable(QStringLiteral("Breite nur %1 Hz")
                                .arg(slice.filterWidth())));

        // Und die Lage muss die Sprache auch WIRKLICH enthalten: bei
        // LSB heisst das, die obere Kante liegt dicht an null. Eine
        // breite Flaeche, die trotzdem erst bei 2000 anfaengt, waere
        // derselbe Fehler in gross.
        QVERIFY2(qAbs(slice.filterHigh()) <= 400,
                 qPrintable(QStringLiteral("obere Kante bei %1 Hz")
                                .arg(slice.filterHigh())));
        QVERIFY2(slice.filterLow() < -2400,
                 qPrintable(QStringLiteral("untere Kante bei %1 Hz")
                                .arg(slice.filterLow())));
    }

    void auchBeiUsb()
    {
        SliceModel slice(0);
        slice.setDspMode(DSPMode::USB);
        slice.setFilter(2000, 2800);

        slice.resetFilter();
        qInfo() << "USB nachher:" << slice.filterLow() << ".." << slice.filterHigh();
        QVERIFY(slice.filterWidth() >= 2400);
        QVERIFY2(slice.filterLow() <= 400,
                 qPrintable(QStringLiteral("untere Kante bei %1 Hz")
                                .arg(slice.filterLow())));
    }

    void cwBleibtSchmal()
    {
        // Ein Rueckstellen darf CW nicht auf Sprechbreite aufreissen —
        // das waere derselbe Schaden mit umgekehrtem Vorzeichen.
        SliceModel slice(0);
        slice.setDspMode(DSPMode::CWU);
        slice.setFilter(-100, 4000);

        slice.resetFilter();
        qInfo() << "CWU nachher:" << slice.filterLow() << ".." << slice.filterHigh()
                << "Breite" << slice.filterWidth();
        QVERIFY2(slice.filterWidth() <= 1000,
                 qPrintable(QStringLiteral("CW-Breite %1 Hz")
                                .arg(slice.filterWidth())));
    }

    void zentrierenAlleinHaetteNichtGereicht()
    {
        // Die GEGENPROBE zum eigentlichen Befund: der alte Knopf haette
        // den Schlitz nur verschoben. Ohne sie waere nicht belegt, dass
        // die Aenderung ueberhaupt noetig war.
        SliceModel slice(0);
        slice.setDspMode(DSPMode::LSB);
        slice.setFilter(-2800, -2000);

        slice.resetFilterCenter();
        qInfo() << "nur zentriert:" << slice.filterLow() << ".."
                << slice.filterHigh() << "Breite" << slice.filterWidth();
        QVERIFY2(slice.filterWidth() < 2400,
                 "resetFilterCenter stellt die Breite wieder her — dann "
                 "waere resetFilter ueberfluessig und dieser Befund falsch");
    }
};

QTEST_APPLESS_MAIN(TstFilterResetRecovers)
#include "tst_filter_reset_recovers.moc"
