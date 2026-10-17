// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_tx_bandbreiten_klickbar.cpp  (Longpath)
// =================================================================
// Die Sendebandbreite laesst sich durchklicken.
//
// Betreiber, 2026-10-16: „nur das ich anklicken kann die
// unterschiedlichen bandbreiten durchklicken kann" — und „5k kannst du
// auch bauen".
//
// Bis dahin stand die Bandbreite in der Fussleiste des TX-Fensters nur
// als Zustand; der Kommentar im Aufbau sagte es ausdruecklich
// („Mikrofonquelle und Bandbreite sind ZUSTAND, kein Bedienelement").
//
// Geprueft wird die REGEL (stufeFuer, naechsteStufe) ohne Fenster,
// dazu ein Klick am echten Knopf fuer die Verdrahtung.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QPushButton>

#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TstTxBandbreitenKlickbar : public QObject
{
    Q_OBJECT

private slots:
    // Jede Stufe wird erkannt, wenn sie genau eingestellt ist.
    void jedeStufeWirdErkannt()
    {
        QCOMPARE(TxApplet::stufeFuer(100, 2900), 0);
        QCOMPARE(TxApplet::stufeFuer(100, 3100), 1);
        QCOMPARE(TxApplet::stufeFuer(100, 3400), 2);
        QCOMPARE(TxApplet::stufeFuer( 50, 3650), 3);   // Martins ESSB
        QCOMPARE(TxApplet::stufeFuer( 40, 5000), 4);   // wie Zeus
        QCOMPARE(TxApplet::stufeFuer( 40, 8000), 5);
    }

    // Durchklicken erreicht alle sechs und laeuft dann um — sonst
    // bliebe eine Stufe unerreichbar.
    void durchklickenErreichtAlleUndLaeuftUm()
    {
        int low = 100, high = 2900;      // Stufe 0
        QVector<int> gesehen;
        for (int i = 0; i < 6; ++i) {
            const int n = TxApplet::naechsteStufe(low, high);
            gesehen.append(n);
            // Die naechste Stufe als neuen Stand annehmen.
            switch (n) {
                case 0: low = 100; high = 2900; break;
                case 1: low = 100; high = 3100; break;
                case 2: low = 100; high = 3400; break;
                case 3: low =  50; high = 3650; break;
                case 4: low =  40; high = 5000; break;
                case 5: low =  40; high = 8000; break;
                default: QFAIL("Stufe ausserhalb");
            }
        }
        QCOMPARE(gesehen, QVector<int>({1, 2, 3, 4, 5, 0}));
    }

    // Eine Breite, die auf keiner Stufe liegt (im Feinblatt von Hand
    // gesetzt oder aus einem fremden Profil), geht auf die naechst
    // GROESSERE — nicht zurueck auf die schmalste, was mitten im
    // Betrieb die Bandbreite zusammenziehen wuerde.
    void eineFremdeBreiteGehtAufDieNaechstgroessere()
    {
        // 2950 Hz breit: zwischen 2.8k (2800) und 3.0k (3000).
        QCOMPARE(TxApplet::stufeFuer(100, 3050), -1);
        QCOMPARE(TxApplet::naechsteStufe(100, 3050), 1);   // 3.0k

        // 4000 Hz breit: zwischen 3.6k (3600) und 5.0k (4960).
        QCOMPARE(TxApplet::naechsteStufe(100, 4100), 4);   // 5.0k

        // Gleich BREIT, aber andere Grenzen (150-3150 ist wie 3.0k
        // genau 3000 Hz): es geht auf die naechste ECHT groessere.
        // Sonst aendert ein Klick die Breite nicht, und es sieht aus,
        // als haette er nicht gewirkt.
        QCOMPARE(TxApplet::naechsteStufe(150, 3150), 2);   // 3.3k
    }

    // Breiter als alle Stufen: wieder von vorn, statt steckenzubleiben.
    void breiterAlsAlleFaengtVonVornAn()
    {
        QCOMPARE(TxApplet::naechsteStufe(20, 9500), 0);
    }

    // Die Reihe ist aufsteigend — darauf beruht „die naechstgroessere".
    void dieStufenSindAufsteigend()
    {
        int vorher = 0;
        for (int i = 0; i < 6; ++i) {
            const int breite = TxApplet::kTxBandbreiten[i].highHz
                               - TxApplet::kTxBandbreiten[i].lowHz;
            QVERIFY2(breite > vorher,
                     qPrintable(QStringLiteral("Stufe %1 (%2) ist nicht "
                                               "breiter als die davor")
                                    .arg(i)
                                    .arg(QLatin1String(
                                        TxApplet::kTxBandbreiten[i].name))));
            vorher = breite;
        }
    }

    // Und am echten Knopf: ein Klick setzt BEIDE Grenzen. Nur die
    // obere zu aendern liesse den Hochpass der vorigen Stufe stehen.
    void einKlickSetztBeideGrenzen()
    {
        RadioModel radio;
        TxApplet applet(&radio);

        TransmitModel& tx = radio.transmitModel();
        tx.setFilterLow(100);
        tx.setFilterHigh(2900);

        QPushButton* knopf = applet.txFilterButtonForTest();
        QVERIFY2(knopf, "die Bandbreite muss ein Knopf sein, kein Label");
        QVERIFY2(!knopf->text().isEmpty(), "der Knopf muss die Breite zeigen");

        knopf->click();
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 3100);

        // Weiter bis zu der Stufe, an der sich AUCH die untere Grenze
        // aendert — genau dort faellt auf, wenn nur eine gesetzt wird.
        knopf->click();                       // 3.3k
        knopf->click();                       // 3.6k: low 50
        QCOMPARE(tx.filterLow(), 50);
        QCOMPARE(tx.filterHigh(), 3650);

        knopf->click();                       // 5.0k: low 40
        QCOMPARE(tx.filterLow(), 40);
        QCOMPARE(tx.filterHigh(), 5000);

        knopf->click();                       // 8.0k
        QCOMPARE(tx.filterHigh(), 8000);

        knopf->click();                       // wieder 2.8k
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 2900);
    }
};

QTEST_MAIN(TstTxBandbreitenKlickbar)
#include "tst_tx_bandbreiten_klickbar.moc"
