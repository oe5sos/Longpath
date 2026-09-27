// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// DXCC-Farben der Spots (2026-09-27): eingeschaltet, abschaltbar im
// Spot-Hub, in den Farben des Hauses. Geprueft: Voreinstellung an;
// neues Land warmes Bernstein, neues Band/Betriebsart gedecktes Messing,
// schon gearbeitet ohne eigene Farbe (der Spot behaelt die seiner
// Quelle); der Schalter im Display-Reiter schreibt die Einstellung.

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/DxccColorProvider.h"
#include "gui/DxccSpotColours.h"
#include "gui/SpotHubDialog.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstDxccSpotColours : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qunsetenv("LONGPATH_CONFIG_DIR");
    }
    void init() { AppSettings::instance().remove(QString::fromLatin1(kDxccColoringKey)); }

    void onByDefaultInTheHouseColours()
    {
        DxccColorProvider dxcc;
        applyDxccSpotColours(dxcc);
        QVERIFY(dxcc.isEnabled());
        QCOMPARE(dxcc.colorNewDxcc, QColor(QString::fromLatin1(Style::kBusyAmber)));
        QCOMPARE(dxcc.colorNewBand, QColor(QString::fromLatin1(Style::kAmberWarn)));
        QCOMPARE(dxcc.colorNewMode, QColor(QString::fromLatin1(Style::kAmberWarn)));
        QVERIFY(!dxcc.colorWorked.isValid());
        // Kein Rot (Hausregel 2026-09-02).
        for (const QColor& c : {dxcc.colorNewDxcc, dxcc.colorNewBand, dxcc.colorNewMode}) {
            QVERIFY2(!(c.red() > 200 && c.green() < 90), qPrintable(c.name()));
        }
    }

    void theSwitchTurnsItOff()
    {
        AppSettings::instance().setValue(QString::fromLatin1(kDxccColoringKey), QStringLiteral("False"));
        DxccColorProvider dxcc;
        applyDxccSpotColours(dxcc);
        QVERIFY(!dxcc.isEnabled());
    }

    // Gegen das eigene Log: neues Land Bernstein, gearbeitet keine Farbe.
    void coloursAgainstTheOwnLog()
    {
        const QString dir = AppSettings::dataDir();
        QVERIFY2(!dir.startsWith(QDir::homePath() + QStringLiteral("/Library/Preferences/Longpath")),
                 qPrintable(dir));
        QDir().mkpath(dir);
        const QString log = dir + QStringLiteral("/logbook.adi");
        {
            QFile f(log);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("t\n<EOH>\n<CALL:6>DL1ABC <QSO_DATE:8>20260920 <TIME_ON:6>101500 "
                    "<BAND:3>20m <MODE:3>FT8 <FREQ:6>14.074 <EOR>\n");
        }
        {
            RadioModel radio;
            DxccColorProvider* dxcc = radio.dxccColorProvider();
            QSignalSpy done(dxcc, &DxccColorProvider::importFinished);
            QVERIFY(done.wait(5000) || done.count() > 0);
            applyDxccSpotColours(*dxcc);
            QVERIFY(!dxcc->colorForSpot(QStringLiteral("DK2XYZ"), 14.074, QStringLiteral("FT8")).isValid());
            QCOMPARE(dxcc->colorForSpot(QStringLiteral("DK2XYZ"), 7.074, QStringLiteral("FT8")),
                     QColor(QString::fromLatin1(Style::kAmberWarn)));
            QCOMPARE(dxcc->colorForSpot(QStringLiteral("JA1XYZ"), 14.074, QStringLiteral("FT8")),
                     QColor(QString::fromLatin1(Style::kBusyAmber)));
        }
        QFile::remove(log);
    }

    void theSpotHubHasTheSwitch()
    {
        RadioModel model;
        SpotHubDialog dlg(model.dxCluster(), model.rbn(), model.wsjtx(),
                          model.spotCollector(), model.pota(), model.sota(),
                          model.freeDvReporter(), model.pskReporter(),
                          model.spotModel(), model.spotTableModel(),
                          model.dxccColorProvider(), nullptr);
        auto* toggle = dlg.findChild<QPushButton*>(QStringLiteral("displayDxccToggle"));
        QVERIFY(toggle);
        QVERIFY(toggle->isChecked());
        toggle->setChecked(false);
        QCOMPARE(AppSettings::instance().value(QString::fromLatin1(kDxccColoringKey)).toString(),
                 QStringLiteral("False"));
        QVERIFY(!dxccColoringEnabled());
        toggle->setChecked(true);
        QVERIFY(dxccColoringEnabled());
    }
};

QTEST_MAIN(TstDxccSpotColours)
#include "tst_dxcc_spot_colours.moc"
