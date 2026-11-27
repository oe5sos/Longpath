// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_pan_options_button.cpp  (Longpath)
// =================================================================
// Der Zahnrad-Knopf am Panadapter — sichtbar und wirksam.
//
// Der Betreiber, 2026-08-20: „weiters fehlt auch die option button um
// beim pandapter gleich etwas zu aendern wie zb bild, groesse usw."
//
// Bild, Deckkraft und Grundfarbe lagen nur unter Setup -> Display,
// drei Klicks und einen Dialog von der Flaeche entfernt, die man
// gerade ansieht.
//
// Geprueft wird der WEG, nicht der Zustand — die Lehre aus dem
// Ablöseknopf, den es gab, den aber niemand erreichen konnte.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QDir>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>

#include "gui/PanadapterApplet.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "gui/SpectrumWidget.h"
#include "gui/meters/MeterPoller.h"
#include "models/SliceModel.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestPanOptionsButton : public QObject
{
    Q_OBJECT
private slots:
    void theGearIsVisibleInTheHeader()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(800, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        QPushButton* gear = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("⚙")) { gear = b; break; }
        }
        QVERIFY2(gear, "in der Kopfleiste MUSS ein Zahnrad stehen");
        QVERIFY2(gear->isVisible(), "und es muss zu sehen sein");
        QVERIFY2(gear->isEnabled(), "und anklickbar");
        QVERIFY2(!gear->toolTip().isEmpty(),
                 "mit einem Hinweis, was es tut");
    }

    // Die Eintraege muessen wirklich etwas ausloesen.
    // 2026-11-06, Betreiber: "1. aber bitte als option zum einblenden im
    // panadapter." Ab Werk aus, keine Zeile; jeder Haken einzeln.
    void dieAnzeigeZeileIstEineOption()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(800, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        QWidget* zeile = applet.readoutRowForTest();
        QVERIFY(zeile);
        QVERIFY2(!zeile->isVisible(), "ohne Haken gibt es keine Zeile");
        QVERIFY(!applet.frequencyReadoutShown() && !applet.swrReadoutShown());

        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);
        QAction* frequenz = nullptr;
        QAction* swr = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("Frequency readout")) { frequenz = a; }
            if (a->text() == QStringLiteral("SWR readout")) { swr = a; }
        }
        QVERIFY2(frequenz && swr, "beide Haken stehen im Zahnrad");
        QVERIFY(frequenz->isCheckable() && !frequenz->isChecked());

        frequenz->trigger();
        QVERIFY(applet.frequencyReadoutShown());
        QVERIFY2(zeile->isVisible(), "ein Haken genuegt fuer die Zeile");
        swr->trigger();
        QVERIFY(applet.swrReadoutShown());
        frequenz->trigger();
        QVERIFY2(zeile->isVisible(), "SWR allein traegt die Zeile auch");
        swr->trigger();
        QVERIFY2(!zeile->isVisible(), "beide aus: Zeile weg");
        delete m;
    }

    // Die Zeile zeigt die Abstimmfrequenz der Scheibe und laeuft mit.
    void dieAnzeigeZeileLaeuftMitDerScheibe()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SliceModel slice;
        slice.setFrequency(7113065.0);
        applet.updateStatusOverlay(&slice, 0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("7.113.065"));
        slice.setFrequency(14225000.0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("14.225.000"));

        // Eine andere Scheibe loest die erste ab.
        SliceModel zweite;
        zweite.setFrequency(3650500.0);
        applet.updateStatusOverlay(&zweite, 0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("3.650.500"));
        slice.setFrequency(21000000.0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("3.650.500"));
    }

    // Ohne Leistung kein SWR: im Empfang steht ein Strich. Beim Senden die
    // Zahl aus derselben Quelle wie das Frequenz-Widget.
    void imEmpfangSteht_einStrich()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        const QString strich = QString::fromUtf8("\xe2\x80\x94");
        QCOMPARE(applet.swrReadoutTextForTest(), strich);
        applet.onReading(MeterBinding::TxSwr, 1.4);
        QCOMPARE(applet.swrReadoutTextForTest(), strich);     // noch keine Leistung
        applet.onReading(MeterBinding::TxPower, 25.0);
        QCOMPARE(applet.swrReadoutTextForTest(), QStringLiteral("1.40"));
        applet.onReading(MeterBinding::TxPower, 0.0);
        QCOMPARE(applet.swrReadoutTextForTest(), strich);     // zurueck im Empfang
    }

    // ── Das S-Meter als dritte Wahl (2026-11-17) ────────────────────
    //
    // Betreiber: „S-Meter soll auch optional im Panadapter zu sehen
    // sein." Derselbe Weg wie Frequenz und SWR: ein Haken im Zahnrad,
    // ab Werk aus, und er traegt die Zeile auch allein.
    void dasSMeterIstDieDritteWahl()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(900, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        QWidget* zeile = applet.readoutRowForTest();
        QVERIFY(zeile);
        QVERIFY2(!applet.sMeterReadoutShown(), "ab Werk aus");
        QVERIFY(!zeile->isVisible());

        QMenu* m = applet.buildDisplayMenuForTesting();
        QAction* sMeter = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("S-meter readout")) { sMeter = a; }
        }
        QVERIFY2(sMeter, "der Haken steht im Zahnrad");
        QVERIFY(sMeter->isCheckable() && !sMeter->isChecked());

        sMeter->trigger();
        QVERIFY(applet.sMeterReadoutShown());
        QVERIFY2(zeile->isVisible(), "das S-Meter traegt die Zeile auch allein");
        sMeter->trigger();
        QVERIFY2(!zeile->isVisible(), "aus: Zeile weg");
        delete m;
    }

    // Betreiber 2026-11-25: „kiwi sollte auch in den optionen im
    // panadapter verfuegbar sein". Der Haken stand nur im Rechtsklick-
    // menue; er steht jetzt auch im Zahnrad, und beide Menues schalten
    // dieselbe Quelle.
    void kiwiStehtImZahnradUndSchaltetDieQuelle()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(900, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        SpectrumWidget* sw = applet.spectrumWidget();
        QVERIFY(sw);
        QVERIFY(!sw->kiwiDisplaySource());

        QMenu* m = applet.buildDisplayMenuForTesting();
        QAction* kiwi = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("Display from the KiwiSDR")) { kiwi = a; }
        }
        QVERIFY2(kiwi, "der Kiwi-Haken steht im Zahnrad");
        QVERIFY(kiwi->isCheckable() && !kiwi->isChecked());
        kiwi->trigger();
        QVERIFY2(sw->kiwiDisplaySource(), "das Zahnrad legt die Quelle um");
        delete m;

        QMenu* r = applet.buildContextMenuForTesting();
        QAction* kiwi2 = nullptr;
        for (QAction* a : r->actions()) {
            if (a->text() == QStringLiteral("Display from the KiwiSDR")) { kiwi2 = a; }
        }
        QVERIFY2(kiwi2 && kiwi2->isChecked(),
                 "das Rechtsklickmenue zeigt denselben Zustand");
        delete r;
    }

    // 2026-11-27, Betreiber mit Flex: "am kiwi geklickt und er haengt
    // wieder". Die Flex liefert ihr Bild ueber denselben Weg wie ein
    // KiwiSDR; Pille und Menuehaken schalteten damit die einzige Quelle ab.
    void dasBildDesFunkgeraetsLaesstSichNichtAbschalten()
    {
        PanadapterApplet applet(QStringLiteral("F"));
        applet.resize(900, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        SpectrumWidget* sw = applet.spectrumWidget();
        SpectrumStatusOverlay* pillen = applet.statusOverlayForTest();
        QVERIFY(sw && pillen);

        // So macht es MainWindow_Flex beim Verbinden.
        sw->setFremdbildVomGeraet(true);
        sw->setKiwiDisplaySource(true);
        QVERIFY2(!pillen->kiwiActive(), "keine KIWI-Pille fuer das Bild der Flex");

        emit pillen->kiwiBadgeClicked();
        QVERIFY2(sw->kiwiDisplaySource(), "ein Klick schaltet das Bild nicht ab");

        QMenu* m = applet.buildDisplayMenuForTesting();
        QAction* kiwi = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("Display from the KiwiSDR")) { kiwi = a; }
        }
        QVERIFY(kiwi);
        QVERIFY2(!kiwi->isEnabled() && !kiwi->isChecked(),
                 "der Menuehaken ist an der Flex ohne Wirkung und sagt das");
        delete m;

        // Und so beim Trennen: dann geht es wieder aus.
        sw->setFremdbildVomGeraet(false);
        sw->setKiwiDisplaySource(false);
        QVERIFY(!sw->kiwiDisplaySource());
        QVERIFY(!pillen->kiwiActive());
    }

    // Vor dem ersten Wert ein Strich; dann S-Stufe und Balken aus der
    // EINEN Skala (ReadingSource) -- dieselbe wie im S-Meter-Instrument.
    void dasSMeterZeigtSStufen()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        const QString strich = QString::fromUtf8("\xe2\x80\x94");
        QCOMPARE(applet.sMeterReadoutTextForTest(), strich);
        QCOMPARE(applet.sMeterReadoutBarForTest(), 0);

        // S9 = -73 dBm, eine S-Stufe = 6 dB (Kurzwelle).
        applet.onReading(MeterBinding::SignalAvg, -73.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S9"));
        const int beiS9 = applet.sMeterReadoutBarForTest();
        QVERIFY2(beiS9 > 0 && beiS9 < 1000,
                 qPrintable(QStringLiteral("Balken bei S9: %1").arg(beiS9)));

        applet.onReading(MeterBinding::SignalAvg, -103.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S4"));
        QVERIFY2(applet.sMeterReadoutBarForTest() < beiS9, "schwaecher: kuerzerer Balken");

        applet.onReading(MeterBinding::SignalAvg, -53.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S9+20"));
        QVERIFY2(applet.sMeterReadoutBarForTest() > beiS9, "staerker: laengerer Balken");

        // Andere Messgroessen ruehren es nicht an.
        applet.onReading(MeterBinding::TxSwr, 3.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S9+20"));
    }

    // Ein Bild fuer den Betreiber (LONGPATH_BILD_DIR): alle drei Stuecke
    // der Zeile nebeneinander. Gruen heisst, dass es da ist -- nicht,
    // dass es gut aussieht.
    void dieZeileAlsBild()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(1100, 300);
        SliceModel slice;
        slice.setFrequency(14074000.0);
        applet.updateStatusOverlay(&slice, 0);
        applet.setFrequencyReadoutShown(true);
        applet.setSMeterReadoutShown(true);
        applet.setSwrReadoutShown(true);
        applet.onReading(MeterBinding::SignalAvg, -67.0);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        QWidget* zeile = applet.readoutRowForTest();
        QVERIFY(zeile && zeile->isVisible());
        const QByteArray ziel = qgetenv("LONGPATH_BILD_DIR");
        if (!ziel.isEmpty()) {
            QDir().mkpath(QString::fromLocal8Bit(ziel));
            zeile->grab().save(QString::fromLocal8Bit(ziel)
                               + QStringLiteral("/pan-anzeigezeile-s-meter.png"));
        }
    }

    void itsEntriesActuallyFire()
    {
        PanadapterApplet applet(QStringLiteral("A"));

        QSignalSpy imgSpy(&applet,
                          &PanadapterApplet::backgroundImageRequested);
        QSignalSpy opaSpy(&applet,
                          &PanadapterApplet::backgroundOpacityRequested);
        QSignalSpy colSpy(&applet,
                          &PanadapterApplet::backgroundColourRequested);
        QSignalSpy setSpy(&applet,
                          &PanadapterApplet::displaySetupRequested);

        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);

        auto fire = [m](const QString& text) -> bool {
            for (QAction* a : m->actions()) {
                if (a->text() == text) { a->trigger(); return true; }
                if (a->menu()) {
                    for (QAction* sub : a->menu()->actions()) {
                        if (sub->text() == text) { sub->trigger(); return true; }
                    }
                }
            }
            return false;
        };

        QVERIFY2(fire(PanadapterApplet::tr("Remove background image")),
                 "der Eintrag zum Entfernen MUSS im Menue stehen");
        QCOMPARE(imgSpy.count(), 1);
        QVERIFY2(imgSpy.at(0).at(0).toString().isEmpty(),
                 "Entfernen heisst: leerer Pfad");

        // „Bild sichtbar 75 %" heisst intern „Fuellfarbe sichtbar 25".
        //
        // Der Wert, den SpectrumWidget haelt, ist die Sichtbarkeit der
        // FUELLFARBE (100 = nur Fuellfarbe, 0 = nur Bild). Das Menue
        // sprach zuerst von „Deckkraft" mit demselben Zahlenwert — also
        // genau verkehrt herum, und der Betreiber sah am 2026-08-20
        // sein Foto nicht. Diese Pruefung haelt die Umrechnung fest.
        QVERIFY2(fire(QStringLiteral("75 %")),
                 "die Bildsichtbarkeit MUSS waehlbar sein");
        QCOMPARE(opaSpy.count(), 1);
        QCOMPARE(opaSpy.at(0).at(0).toInt(), 25);

        QVERIFY2(fire(QStringLiteral("Grundfarbe…")), "Grundfarbe fehlt");
        QCOMPARE(colSpy.count(), 1);

        // Dieselbe tr()-Rolle wie im Programm (2026-10-14): seit
        // Englisch die Quellsprache ist, ist ein Menuetext
        // uebersetzbar und taugt nicht als Bezeichner.
        QVERIFY2(fire(PanadapterApplet::tr("All display settings…")),
                 "der Weg in den Setup-Dialog MUSS bleiben — ein Menue, "
                 "das alles kann, waere wieder der Dialog, nur schlechter");
        QCOMPARE(setSpy.count(), 1);

        m->deleteLater();
    }

    // Tempo des Wasserfalls im Zahnrad (2026-11-22): Betreiber suchte es
    // hier, es stand nur im Rechtsklick und im Setup. Derselbe Wert.
    void dasWasserfallTempoStehtImZahnrad()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumWidget* sw = applet.spectrumWidget();
        QVERIFY(sw);
        const auto tempoMenue = [](QMenu* m) -> QMenu* {
            for (QAction* a : m->actions()) {
                if (a->menu() && a->text() == QStringLiteral("Waterfall speed")) {
                    return a->menu();
                }
            }
            return nullptr;
        };

        sw->setWfUpdatePeriodMs(30);
        QMenu* m = applet.buildDisplayMenuForTesting();
        QMenu* tempo = tempoMenue(m);
        QVERIFY2(tempo, "Untermenue „Waterfall speed“ im Zahnrad");
        QStringList namen;
        for (QAction* a : tempo->actions()) {
            if (!a->isSeparator()) { namen << a->text(); }
        }
        QCOMPARE(namen, (QStringList{QStringLiteral("Fast (30 ms)"),
                                     QStringLiteral("Normal (60 ms)"),
                                     QStringLiteral("Slow (120 ms)"),
                                     QStringLiteral("Very slow (250 ms)")}));
        QVERIFY(tempo->actions().first()->isChecked());
        for (QAction* a : tempo->actions()) {
            if (a->text() == QStringLiteral("Slow (120 ms)")) { a->trigger(); }
        }
        QCOMPARE(sw->wfUpdatePeriodMs(), 120);
        delete m;

        // Ein eigener Wert (Rechtsklick/Setup) wird gezeigt, nicht verworfen.
        sw->setWfUpdatePeriodMs(135);
        m = applet.buildDisplayMenuForTesting();
        tempo = tempoMenue(m);
        QVERIFY(tempo);
        QAction* eigen = tempo->actions().last();
        QCOMPARE(eigen->text(), QStringLiteral("Custom · 135 ms"));
        QVERIFY(eigen->isChecked() && !eigen->isEnabled());
        QCOMPARE(sw->wfUpdatePeriodMs(), 135);
        delete m;
    }

    // Kopfzeile bei schmalem Panadapter (2026-11-22). Betreiber: „das TX -
    // die pille ueber den optionen" -- bei ~1160 px lag die rote TX-Pille
    // ueber 3D und Zahnrad, vom Zahnrad blieb ein Rest. Kein Kopfknopf darf
    // vom Pillen-Streifen verdeckt sein, das Zahnrad muss klickbar sein.
    void diePillenLiegenNieUeberDenKopfknoepfen_data()
    {
        QTest::addColumn<int>("breite");
        QTest::newRow("1400") << 1400;
        QTest::newRow("1160") << 1160;
        QTest::newRow("900")  << 900;
    }
    void diePillenLiegenNieUeberDenKopfknoepfen()
    {
        QFETCH(int, breite);
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumStatusOverlay* streifen = applet.statusOverlayForTest();
        QVERIFY(streifen);
        streifen->setTxBound(true);      // CH 0 | TX | KIWI wie beim Betreiber
        streifen->setKiwiActive(true);
        applet.resize(breite, 500);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        const QRect pillen = streifen->geometry();
        for (QPushButton* knopf : {applet.optionsButtonForTest(),
                                   applet.renderModeButtonForTest()}) {
            QVERIFY(knopf && knopf->isVisible());
            const QRect r(knopf->mapTo(&applet, QPoint(0, 0)), knopf->size());
            QVERIFY2(!pillen.intersects(r),
                     qPrintable(QStringLiteral("Pillen %1..%2 ueber dem Knopf %3 bei %4..%5")
                                    .arg(pillen.left()).arg(pillen.right())
                                    .arg(knopf->text())
                                    .arg(r.left()).arg(r.right())));
            // Und klickbar: oben liegt der Knopf, nicht der Streifen.
            QCOMPARE(applet.childAt(r.center()), static_cast<QWidget*>(knopf));
        }
    }
};
QTEST_MAIN(TestPanOptionsButton)
#include "tst_pan_options_button.moc"
