// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbook_one_page.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Das Logbuch als eine Seite (Betreiber, 2026-09-22, nach dem Bild der
// Vorlage): Tabelle, Karte und Detailkarte nebeneinander, die Kennzahlen
// als Reihe darunter. Beides folgt dem Filter der Tabelle, beides laesst
// sich abschalten, und der Zustand ueberlebt den Neustart. Die Fenster
// bleiben ueber „↗" erreichbar.

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/LogbookStatsWidget.h"

#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include "FensterFrist.h"

using namespace Longpath;

namespace {
QString writeAdif(const QDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("log.adi"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:5>OE3AA <QSO_DATE:8>20260901 <TIME_ON:6>150104 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>JN88 <MY_GRIDSQUARE:6>JN67UT <EOR>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260902 <TIME_ON:6>143710 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>FN42 <MY_GRIDSQUARE:6>JN67UT <EOR>\n"
            "<CALL:5>W2XYZ <QSO_DATE:8>20260903 <TIME_ON:6>150126 <BAND:3>40m <MODE:2>CW "
            "<GRIDSQUARE:4>FN31 <MY_GRIDSQUARE:6>JN67UT <EOR>\n"
            "<CALL:5>N3DEF <QSO_DATE:8>20260904 <TIME_ON:6>150126 <BAND:3>40m <MODE:2>CW "
            "<GRIDSQUARE:4>FM19 <MY_GRIDSQUARE:6>JN67UT <EOR>\n");
    f.close();
    return path;
}
} // namespace

class TstLogbookOnePage : public QObject { Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookMode"), QStringLiteral("Log"));
    }

    // Seit Entwurf C (2026-11-12): Karte in „Log" und „Browse", Kennzahlen
    // als eigene Seite „Statistics" -- beide folgen dem Filter der Liste.
    void mapAndStatsFollowTheTable()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* map = w.mapPanelForTest();
        QVERIFY2(map, "keine eingebettete Karte");
        map->imagery()->setNetworkEnabled(false);
        QVERIFY(map->isEmbedded());
        QTRY_VERIFY(map->isVisible());
        QVERIFY(!map->isWindow());   // eine Spalte, kein Fenster

        // Alles, was die Tabelle zeigt: vier Kontakte, vier Punkte.
        QTRY_COMPARE_WITH_TIMEOUT(map->shownCountForTest(), 4, 3000);

        // Filtern: nur W2XYZ — die Karte folgt.
        QLineEdit* search = nullptr;
        for (QLineEdit* e : w.findChildren<QLineEdit*>()) {
            if (e->placeholderText().startsWith(QLatin1String("Search"))) { search = e; }
        }
        QVERIFY(search);
        search->setText(QStringLiteral("W2XYZ"));
        QTRY_COMPARE_WITH_TIMEOUT(map->shownCountForTest(), 1, 3000);

        // Die Kennzahlen auf ihrer Seite zaehlen dasselbe.
        w.setModusForTest(2);
        LogbookStatsWidget* row = w.statsRowForTest();
        QTRY_VERIFY(row && row->isVisible());
        QVERIFY(!map->isVisible());
        QTRY_COMPARE(row->totalLabel()->text(), QStringLiteral("1"));
        search->clear();
        QTRY_COMPARE(row->totalLabel()->text(), QStringLiteral("4"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("LogbookMode")).toString(),
                 QStringLiteral("Statistics"));

        w.setModusForTest(1);   // Browse: Karte klein ueber der Station
        QTRY_VERIFY(map->isVisible());
        QVERIFY(map->isKompakt());
        QTRY_COMPARE_WITH_TIMEOUT(map->shownCountForTest(), 4, 3000);

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QDir().mkpath(grabDir);
            QTest::qWait(600);
            w.grab().save(grabDir + QStringLiteral("/logbook-one-page.png"));
        }
    }

    void theSettingIsHonouredOnOpen()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookMode"), QStringLiteral("Statistics"));
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QCOMPARE(w.modusForTest(), 2);
        QTRY_VERIFY(w.statsRowForTest()->isVisible());
        QVERIFY(!w.mapPanelForTest() || !w.mapPanelForTest()->isVisible());
    }
};

QTEST_MAIN(TstLogbookOnePage)
#include "tst_logbook_one_page.moc"
