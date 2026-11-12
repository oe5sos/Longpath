// =================================================================
// tests/tst_bandstack_controller.cpp  (Longpath)
// =================================================================
//
// Die Steuerung des Bandstapels am echten RadioModel: Bandtaste auf ein
// anderes Band, nochmal auf dasselbe (weiter, mit Umlauf), Umschalt
// zurueck, Speichern beim Verlassen und die Faelle, in denen nichts
// gespeichert wird (danebenstehend, ausserhalb des Bandes der Region),
// das Seitenband nach dem Band, Sperre, Ablage je Geraet, Regionswechsel
// und das Bandgedaechtnis, das ein Abruf weiter mitbringt.
//
// Startbelegung Region 1 (Thetis clsBandStackManager.cs
// AddRegion1BandStack): 40 m 7.005 CWL, 7.010 CWL, 7.045 DIGU,
// 7.090 LSB, 7.100 LSB; 20 m 14.005 CWU, 14.010 CWU, 14.085 DIGU,
// 14.155 USB, 14.225 USB.
//
// no-port-check: Pruefstand; die Regeln stehen in BandStackStore.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 - Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackController.h"

using namespace Longpath;

namespace {

const QString kMacEins = QStringLiteral("02:00:00:00:BA:01");
const QString kMacZwei = QStringLiteral("02:00:00:00:BA:02");

struct Stand {
    std::unique_ptr<RadioModel> radio;
    SliceModel* scheibe{nullptr};
    BandStackController* stapel{nullptr};
    int id{-1};
};

Stand aufbauen(const QString& mac = kMacEins)
{
    Stand s;
    s.radio = std::make_unique<RadioModel>();
    RadioInfo info;
    info.macAddress = mac;
    s.radio->setLastRadioInfoForTest(info);
    s.radio->addSlice();
    s.scheibe = s.radio->activeSlice();
    s.scheibe->setFrequency(14225000.0);
    s.scheibe->setDspMode(DSPMode::USB);
    s.radio->setConnectionStateForTest(ConnectionState::Connected);
    s.stapel = s.radio->bandStack();
    s.id = s.scheibe->sliceIndex();
    return s;
}

bool druck(Stand& s, Band band, BandStackDirection d = BandStackDirection::Next,
           QString* grund = nullptr)
{
    return s.stapel->press(s.id, band, d, {}, grund);
}

qint64 hz(const Stand& s)
{
    return qRound64(s.scheibe->frequency());
}

QList<qint64> frequenzen(const Stand& s, Band band)
{
    QList<qint64> out;
    for (const BandStackEntry& e : s.stapel->store().entries(band)) {
        out.append(e.frequencyHz);
    }
    return out;
}

} // namespace

class TstBandStackController : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("Region"), QStringLiteral("Europe"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    void ohneGeraetKeinStapel()
    {
        RadioModel radio;
        radio.addSlice();
        BandStackController* stapel = radio.bandStack();
        QVERIFY(stapel != nullptr);
        QVERIFY(!stapel->isReady());
        QVERIFY(!stapel->unavailableReason().isEmpty());
        QVERIFY(stapel->positionText(radio.activeSlice()->sliceIndex()).isEmpty());
        QString grund;
        QVERIFY(!stapel->press(radio.activeSlice()->sliceIndex(), Band::Band40m,
                               BandStackDirection::Next, {}, &grund));
        QCOMPARE(grund, stapel->unavailableReason());
    }

    void startbelegungDerRegion()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->isReady());
        QCOMPARE(frequenzen(s, Band::Band40m),
                 QList<qint64>({7005000, 7010000, 7045000, 7090000, 7100000}));
        QCOMPARE(s.stapel->store().region(), QStringLiteral("Europe"));
    }

    // Ein anderes Band ohne Stelle: der erste Sprecheintrag (Upstream,
    // wie Issue 118), nicht der erste Eintrag, der meist CW ist.
    void anderesBandLandetAufErstemSprecheintrag()
    {
        Stand s = aufbauen();
        QSignalSpy bericht(s.stapel, &BandStackController::recalled);
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(hz(s), qint64(7090000));
        QCOMPARE(s.scheibe->dspMode(), DSPMode::LSB);
        QCOMPARE(bericht.size(), 1);
        QCOMPARE(bericht.first().at(1).toString(), QStringLiteral("40m: 7.090 LSB, 4 of 5"));
        QCOMPARE(s.stapel->positionText(s.id), QStringLiteral("40m 4/5"));
    }

    void nochmalSchaltetWeiterMitUmlauf()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(hz(s), qint64(7100000));
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(hz(s), qint64(7005000));
        QCOMPARE(s.scheibe->dspMode(), DSPMode::CWL);
        QVERIFY(druck(s, Band::Band40m, BandStackDirection::Previous));
        QCOMPARE(hz(s), qint64(7100000));
    }

    // Wer auf seiner Stelle weiterstimmt und das Band verlaesst, nimmt die
    // neue Frequenz in den Eintrag mit (Thetis UpdateCurrentWithLastVisitedData).
    void verlassenSpeichertDieStelle()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        s.scheibe->setFrequency(7093000.0);
        QVERIFY(druck(s, Band::Band20m));
        QVERIFY(frequenzen(s, Band::Band40m).contains(7093000));
        QVERIFY(!frequenzen(s, Band::Band40m).contains(7090000));
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(hz(s), qint64(7093000));
    }

    // Am Geraet am 2026-11-11: auf der Stelle 14.010 CWU weit
    // weitergestimmt, dann verlassen -- der Eintrag stand danach als
    // 14.254 USB an zweiter Stelle. Wie Thetis wird nach dem Speichern
    // nach Frequenz geordnet.
    void verlassenOrdnetNachFrequenz()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));                 // 7.090
        QVERIFY(druck(s, Band::Band40m, BandStackDirection::Previous)); // 7.045
        QVERIFY(druck(s, Band::Band40m, BandStackDirection::Previous)); // 7.010
        QCOMPARE(hz(s), qint64(7010000));
        s.scheibe->setFrequency(7095000.0);
        QVERIFY(druck(s, Band::Band20m));
        QCOMPARE(frequenzen(s, Band::Band40m),
                 QList<qint64>({7005000, 7045000, 7090000, 7095000, 7100000}));
    }

    // Auf ein Band gestimmt statt abgerufen: die Scheibe steht neben ihrer
    // Stelle, der Stand zeigt „-", und Verlassen schreibt nichts hinein.
    void hineingestimmtSpeichertNichts()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        QVERIFY(druck(s, Band::Band20m)); // Stelle auf 20 m: 14.155
        QCOMPARE(hz(s), qint64(14155000));
        QVERIFY(druck(s, Band::Band40m));
        const QList<qint64> vorher = frequenzen(s, Band::Band20m);
        s.scheibe->setFrequency(14200000.0); // von Hand nach 20 m
        QCOMPARE(s.stapel->positionText(s.id), QStringLiteral("20m -/5"));
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(frequenzen(s, Band::Band20m), vorher);
        // Ein Druck auf das Band, auf dem sie danebensteht, fuehrt zur
        // Stelle, nicht einen Schritt weiter.
        s.scheibe->setFrequency(14200000.0);
        QVERIFY(druck(s, Band::Band20m));
        QCOMPARE(hz(s), qint64(14155000));
    }

    // Betreiber am 2026-10-29: eine Stelle ausserhalb des Bandes der
    // eingestellten Region kommt nicht in den Stapel. 7.250 ist nach der
    // Region-2-Tabelle 40 m, in Region 1 nicht.
    void ausserhalbDesBandesWirdNichtGespeichert()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        s.scheibe->setFrequency(7250000.0);
        QVERIFY(druck(s, Band::Band20m));
        QVERIFY(!frequenzen(s, Band::Band40m).contains(7250000));
        QVERIFY(frequenzen(s, Band::Band40m).contains(7090000));
    }

    // Betreiber am 2026-10-23: 160, 80 und 40 m bei der Bandauswahl
    // immer LSB -- auch aus einem Eintrag, der USB traegt.
    void seitenbandRichtetSichNachDemBand()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        s.scheibe->setFrequency(7150000.0);
        s.scheibe->setDspMode(DSPMode::USB);
        QString grund;
        QVERIFY2(s.stapel->add(s.id, {}, &grund), qPrintable(grund));
        QVERIFY(druck(s, Band::Band20m));
        // Der neue Eintrag ist die Stelle auf 40 m.
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(hz(s), qint64(7150000));
        QCOMPARE(s.scheibe->dspMode(), DSPMode::LSB);
        QVERIFY(s.scheibe->filterHigh() <= 0);
    }

    void hinzufuegenOrdnetNachFrequenz()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        s.scheibe->setFrequency(7060000.0);
        QString grund;
        QVERIFY2(s.stapel->add(s.id, {}, &grund), qPrintable(grund));
        QCOMPARE(frequenzen(s, Band::Band40m),
                 QList<qint64>({7005000, 7010000, 7045000, 7060000, 7090000, 7100000}));
        QCOMPARE(s.stapel->positionText(s.id), QStringLiteral("40m 4/6"));
        // Innerhalb 100 Hz ist dieselbe Stelle.
        s.scheibe->setFrequency(7060050.0);
        QVERIFY(!s.stapel->add(s.id, {}, &grund));
        QVERIFY(!grund.isEmpty());
    }

    void gesperrteScheibeBleibt()
    {
        Stand s = aufbauen();
        s.scheibe->setLocked(true);
        QString grund;
        QVERIFY(!druck(s, Band::Band40m, BandStackDirection::Next, &grund));
        QVERIFY(grund.contains(QStringLiteral("locked")));
        QCOMPARE(hz(s), qint64(14225000));
    }

    // Das Bandgedaechtnis bringt beim Abruf weiter, was es haelt (hier der
    // AGC-Wert) -- nur Frequenz, Betriebsart und Filter kommen aus dem
    // Eintrag.
    void bandgedaechtnisKommtMit()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        s.scheibe->setAgcThreshold(-70);
        QVERIFY(druck(s, Band::Band20m));
        s.scheibe->setAgcThreshold(-110);
        QVERIFY(druck(s, Band::Band40m));
        QCOMPARE(s.scheibe->agcThreshold(), -70);
        QCOMPARE(hz(s), qint64(7090000));
        QVERIFY(druck(s, Band::Band20m));
        QCOMPARE(s.scheibe->agcThreshold(), -110);
    }

    // Je Geraet ein eigenes Dokument, und ein neuer Start liest es zurueck.
    void ablageJeGeraet()
    {
        {
            Stand s = aufbauen(kMacEins);
            QVERIFY(druck(s, Band::Band40m));
            s.scheibe->setFrequency(7061000.0);
            QString grund;
            QVERIFY2(s.stapel->add(s.id, {}, &grund), qPrintable(grund));
            s.stapel->flush();
        }
        {
            Stand s = aufbauen(kMacZwei);
            QVERIFY(!frequenzen(s, Band::Band40m).contains(7061000));
        }
        {
            Stand s = aufbauen(kMacEins);
            QVERIFY(frequenzen(s, Band::Band40m).contains(7061000));
        }
    }

    // Ein Regionswechsel laedt die Startbelegung der neuen Region; ein von
    // Hand gesperrter Eintrag bleibt.
    void regionswechselLaedtNeueStartbelegung()
    {
        Stand s = aufbauen();
        QVERIFY(druck(s, Band::Band40m));
        s.scheibe->setFrequency(7061000.0);
        QString grund;
        QVERIFY2(s.stapel->add(s.id, {}, &grund), qPrintable(grund));
        QString eigener;
        for (const BandStackEntry& e : s.stapel->store().entries(Band::Band40m)) {
            if (e.frequencyHz == 7061000) { eigener = e.id; }
        }
        QVERIFY(s.stapel->edit(eigener, std::nullopt, true, &grund));

        AppSettings::instance().setValue(QStringLiteral("Region"),
                                         QStringLiteral("United States"));
        QVERIFY(druck(s, Band::Band20m));
        QCOMPARE(s.stapel->store().region(), QStringLiteral("United States"));
        const QList<qint64> vierzig = frequenzen(s, Band::Band40m);
        QVERIFY(vierzig.contains(7061000));
        QVERIFY(!vierzig.contains(7090000)); // Region 1, nicht Region 2
    }
};

QTEST_MAIN(TstBandStackController)
#include "tst_bandstack_controller.moc"
