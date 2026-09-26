// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// cty.dat ist im laufenden Programm geladen (2026-09-26). Der Port aus
// AetherSDR brachte DxccColorProvider mit, aber weder den Aufruf von
// loadCtyDat() noch den Eintrag in resources.qrc -- der Parser war zur
// Laufzeit leer. Die Tests bemerkten es nie: sie laden cty.dat selbst
// aus dem Quellbaum.

#include <QtTest>

#include "core/CtyDatParser.h"
#include "core/DxccColorProvider.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstCtyDatLoadedAtRuntime : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void theResourceIsThere()
    {
        QVERIFY2(QFile::exists(QStringLiteral(":/cty.dat")), "cty.dat fehlt in resources.qrc");
    }

    void theRadioModelsProviderResolvesCallsigns()
    {
        RadioModel radio;
        DxccColorProvider* dxcc = radio.dxccColorProvider();
        QVERIFY(dxcc);
        const CtyDatParser& cty = dxcc->ctyDat();
        QCOMPARE(cty.resolvePrimaryPrefix(QStringLiteral("OE5SOS")), QStringLiteral("OE"));
        QCOMPARE(cty.resolvePrimaryPrefix(QStringLiteral("KB2UKA")), QStringLiteral("K"));
        QCOMPARE(cty.resolvePrimaryPrefix(QStringLiteral("DL1ABC")), QStringLiteral("DL"));
        const DxccEntity* oe = cty.entityByPrefix(QStringLiteral("OE"));
        QVERIFY(oe);
        QVERIFY(oe->hasLatLon);
        // Oesterreich liegt oestlich von Greenwich und noerdlich von 45 Grad.
        QVERIFY(oe->latitude > 45.0 && oe->latitude < 50.0);
        QVERIFY(oe->longitude > 9.0 && oe->longitude < 18.0);
    }
};

QTEST_MAIN(TstCtyDatLoadedAtRuntime)
#include "tst_cty_dat_loaded_at_runtime.moc"
