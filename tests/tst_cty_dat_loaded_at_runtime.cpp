// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// cty.dat ist im laufenden Programm geladen (2026-09-26). Der Port aus
// AetherSDR brachte DxccColorProvider mit, aber weder den Aufruf von
// loadCtyDat() noch den Eintrag in resources.qrc -- der Parser war zur
// Laufzeit leer. Die Tests bemerkten es nie: sie laden cty.dat selbst
// aus dem Quellbaum.

#include <QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/CtyDatParser.h"
#include "core/DxccColorProvider.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstCtyDatLoadedAtRuntime : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Ein Schritt schreibt und loescht dataDir()/logbook.adi -- nie
        // in einem echten Ordner.
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

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

    // Das eigene Logbuch ist "schon gearbeitet": ohne es galt mit
    // geladener cty.dat jeder Spot als neues Land.
    void theOwnLogbookFeedsWorkedStatus()
    {
        const QString dir = AppSettings::dataDir();
        QVERIFY2(!dir.startsWith(QDir::homePath() + QStringLiteral("/Library/Preferences/Longpath"))
                 && !dir.startsWith(QDir::homePath() + QStringLiteral("/.config/Longpath")),
                 qPrintable(QStringLiteral("Testpfad zeigt auf echte Daten: ") + dir));
        QDir().mkpath(dir);
        const QString log = dir + QStringLiteral("/logbook.adi");
        {
            QFile f(log);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("t\n<EOH>\n"
                    "<CALL:6>DL1ABC <QSO_DATE:8>20260920 <TIME_ON:6>101500 <BAND:3>20m <MODE:3>FT8 <FREQ:6>14.074 <EOR>\n");
        }
        {
            RadioModel radio;
            DxccColorProvider* dxcc = radio.dxccColorProvider();
            QSignalSpy done(dxcc, &DxccColorProvider::importFinished);
            QVERIFY(done.wait(5000) || done.count() > 0);
            QCOMPARE(dxcc->statusForSpot(QStringLiteral("DK2XYZ"), 14.074, QStringLiteral("FT8")),
                     DxccStatus::Worked);                 // Deutschland, 20 m, Digital
            QCOMPARE(dxcc->statusForSpot(QStringLiteral("DK2XYZ"), 7.074, QStringLiteral("FT8")),
                     DxccStatus::NewBand);
            QCOMPARE(dxcc->statusForSpot(QStringLiteral("JA1XYZ"), 14.074, QStringLiteral("FT8")),
                     DxccStatus::NewDxcc);
        }
        QFile::remove(log);
    }
};

QTEST_MAIN(TstCtyDatLoadedAtRuntime)
#include "tst_cty_dat_loaded_at_runtime.moc"
