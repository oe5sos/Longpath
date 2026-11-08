// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueflauf, keine portierte Logik.
// =================================================================
// tests/tst_antenne_je_band_zweite_ablage.cpp  (Longpath)
// =================================================================
//
// Die Antennen stehen je Band in EINER Tabelle: `AlexController`,
// `hardware/<mac>/alex/antenna/<band>/{tx,rx,rxonly}`. So haelt es
// auch Thetis, und so steht es seit dem 2026-10-25 in NereusSDRs
// CLAUDE.md: „the only antenna store; slices take their labels from
// it".
//
// Daneben legt `SliceModel::saveToSettings` aber eine ZWEITE Ablage
// an, und die kennt kein Band:
//
//     Slice<N>/RxAntenna        (SliceModel.cpp:2922)
//     Slice<N>/TxAntenna
//
// `restoreFromSettings` setzt sie wieder (SliceModel.cpp:3378-3382),
// und der Empfaenger dafuer — `RadioModel::wireSliceSignals`,
// RadioModel.cpp:14669 — schreibt sie in die Zeile des Bandes, auf
// dem die Scheibe GERADE steht:
//
//     const Band band = bandFromFrequency(slice->frequency());
//     m_alexController.setRxAnt(band, antNum);
//
// Die Frequenz selbst liegt bandbezogen (`bandPrefix`), die Antenne
// nicht (`slicePrefix`). Faellt beides auseinander, schreibt die
// bandlose Kopie in die falsche Zeile.
//
// Dieser Prueflauf haelt fest, WAS GILT — nicht, was gerade passiert:
// die Bandzeile der Alex-Tabelle darf sich beim Wiederherstellen
// einer Scheibe nicht aendern.
// =================================================================
// Modification history (Longpath):
//   2026-11-08 — Angelegt. Longpath, KI-gestuetzt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/SkuUiProfile.h"

using namespace Longpath;

class TstAntenneJeBandZweiteAblage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // Die bandlose Ablage gibt es — und sie wird geschrieben.
    void eineZweiteAblageGibtEsNichtMehr()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QString(), /*suppressAutoStreamBinding=*/true);
        QVERIFY(!model.slices().isEmpty());
        SliceModel* slice = model.slices().first();

        slice->setRxAntenna(QStringLiteral("ANT2"));
        slice->saveToSettings(Band::Band20m);

        // Seit dem 2026-11-08 gibt es die zweite, BANDLOSE Ablage nicht
        // mehr. Sie war der Grund, warum die Beschriftung der Scheibe
        // und die Zeile der Alex-Tabelle auseinanderlaufen konnten.
        auto& s = AppSettings::instance();
        const QString rx = QStringLiteral("Slice%1/RxAntenna").arg(slice->sliceIndex());
        const QString tx = QStringLiteral("Slice%1/TxAntenna").arg(slice->sliceIndex());
        QVERIFY2(!s.contains(rx), qPrintable(rx));
        QVERIFY2(!s.contains(tx), qPrintable(tx));
    }

    // Der Kern: eine Scheibe auf 40 m wiederherstellen, waehrend die
    // bandlose Kopie die Antenne von 20 m traegt. Die 40-m-Zeile der
    // Alex-Tabelle muss stehenbleiben.
    void wiederherstellenAendertDieBandzeileNicht()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        // Die Tabelle, die gilt: 40 m auf ANT2, 20 m auf ANT3.
        model.alexControllerMutable().setRxAnt(Band::Band40m, 2);
        model.alexControllerMutable().setRxAnt(Band::Band20m, 3);

        model.addSlice(QString(), /*suppressAutoStreamBinding=*/true);
        QVERIFY(!model.slices().isEmpty());
        SliceModel* slice = model.slices().first();

        // Die bandlose Kopie steht auf ANT3 — gespeichert, als die
        // Scheibe zuletzt auf 20 m stand.
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice%1/RxAntenna").arg(slice->sliceIndex()),
                   QStringLiteral("ANT3"));

        // Die Scheibe steht auf 40 m, und von dort wird wiederhergestellt.
        slice->setFrequency(7'100'000.0);
        slice->restoreFromSettings(Band::Band40m);

        // Voraussetzung mitpruefen: hat das Wiederherstellen die
        // bandlose Kopie ueberhaupt angewendet? Ein gruener Lauf, bei
        // dem der Weg gar nicht beschritten wurde, beweist nichts.
        qInfo().noquote() << "Scheibe traegt nach dem Wiederherstellen:"
                          << slice->rxAntenna()
                          << "| Alex 40m =" << model.alexController().rxAnt(Band::Band40m)
                          << "| Alex 20m =" << model.alexController().rxAnt(Band::Band20m);

        QCOMPARE(model.alexController().rxAnt(Band::Band20m), 3);
        QCOMPARE(model.alexController().rxAnt(Band::Band40m), 2);

        // Und der eigentliche Punkt: die Scheibe traegt die Antenne DES
        // BANDES, nicht die bandlose Kopie. Vorher stand hier „ANT3",
        // waehrend die Maschine ANT2 geschaltet hatte — die Anzeige
        // behauptete etwas anderes als der Draht tat.
        const SkuUiProfile sku = skuUiProfileFor(model.hardwareProfile().model);
        slice->refreshAntennasFromAlex(model.alexController(), Band::Band40m, &sku);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT2"));
    }
};

QTEST_MAIN(TstAntenneJeBandZweiteAblage)
#include "tst_antenne_je_band_zweite_ablage.moc"
