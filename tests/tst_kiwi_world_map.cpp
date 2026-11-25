// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Kiwi-Weltkarte (Entwurf A, 2026-11-25): stimmt die Kuestenlinie,
// trifft der Klick den Punkt, zeigt der Auswahldialog dieselben
// Empfaenger auf der Karte wie in der Tabelle -- und fehlen die, die der
// Betreiber nicht angefahren haben will, auch dort.

#include <QtTest>
#include <QPolygonF>
#include <QTableWidget>

#include "gui/KiwiPublicReceiverPicker.h"
#include "gui/widgets/KiwiWorldCoast.h"
#include "gui/widgets/KiwiWorldMap.h"

using namespace Longpath;

namespace {

bool isLand(double lat, double lon)
{
    const short* d = kiwiCoastData();
    int at = 0;
    for (int r = 0; r < kiwiCoastRingCount(); ++r) {
        const int n = d[at++];
        QPolygonF poly;
        for (int i = 0; i < n; ++i) {
            poly << QPointF(d[at + 2 * i] / 10.0, d[at + 2 * i + 1] / 10.0);
        }
        at += 2 * n;
        if (poly.containsPoint(QPointF(lon, lat), Qt::OddEvenFill)) { return true; }
    }
    return false;
}

KiwiPublicReceiver rx(const char* name, const char* host, const char* gps,
                      int users, int usersMax, int extApi, bool offline = false)
{
    KiwiPublicReceiver r;
    r.name = QString::fromLatin1(name);
    r.url = QStringLiteral("http://%1:8073").arg(QString::fromLatin1(host));
    r.gps = QString::fromLatin1(gps);
    r.users = users;
    r.usersMax = usersMax;
    r.extApi = extApi;
    r.offline = offline;
    r.bands = QStringLiteral("0-30000000");
    return r;
}

} // namespace

class TstKiwiWorldMap : public QObject
{
    Q_OBJECT
private slots:
    void kuestenlinieStimmt()
    {
        QCOMPARE(kiwiCoastRingCount(), 127);
        // Land: Linz, Sydney, Chicago; Meer: Mitte des Atlantiks, Pazifik.
        QVERIFY(isLand(48.3, 14.3));
        QVERIFY(isLand(-33.9, 151.2));
        QVERIFY(isLand(41.9, -87.6));
        QVERIFY(!isLand(30.0, -40.0));
        QVERIFY(!isLand(0.0, -150.0));
    }

    void klickTrifftDenPunkt()
    {
        KiwiWorldMap map;
        map.resize(720, 360);
        KiwiMapPoint a; a.id = QStringLiteral("a"); a.lat = 48.3; a.lon = 14.3;
        KiwiMapPoint b; b.id = QStringLiteral("b"); b.lat = -33.9; b.lon = 151.2;
        map.setPoints({a, b});
        QCOMPARE(map.idAt(map.toWidget(48.3, 14.3)), QStringLiteral("a"));
        QCOMPARE(map.idAt(map.toWidget(-33.9, 151.2) + QPointF(3, 3)), QStringLiteral("b"));
        QVERIFY(map.idAt(map.toWidget(0, 0)).isEmpty());

        QSignalSpy spy(&map, &KiwiWorldMap::pointClicked);
        map.show();
        QVERIFY(QTest::qWaitForWindowExposed(&map, 15000));
        QTest::mouseClick(&map, Qt::LeftButton, {}, map.toWidget(48.3, 14.3).toPoint());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a"));
    }

    void ausschnittFolgtDenOffenenKiwis()
    {
        KiwiWorldMap map;
        map.resize(720, 360);
        KiwiMapPoint a; a.id = QStringLiteral("a"); a.lat = 52.2; a.lon = 6.9; a.open = true;
        KiwiMapPoint far; far.id = QStringLiteral("far"); far.lat = -33.9; far.lon = 151.2;
        map.setPoints({a, far});
        map.setHome(48.0, 13.8, true, QStringLiteral("JN77"));
        double la0, la1, lo0, lo1;
        map.viewBounds(la0, la1, lo0, lo1);
        QVERIFY(lo1 - lo0 >= 359.0);          // Welt
        map.setAutoFit(true);
        map.viewBounds(la0, la1, lo0, lo1);
        QVERIFY2(lo1 - lo0 < 90.0, "Ausschnitt muss auf Europa zusammenrutschen");
        QVERIFY(lo0 < 6.9 && lo1 > 13.8);
        QVERIFY(la0 < 48.0 && la1 > 52.2);
        // Der Ferne liegt ausserhalb, wird aber nicht abgefragt.
        QVERIFY(map.toWidget(-33.9, 151.2).x() > map.width());
    }

    void dialogZeigtDieselbenWieDieTabelle()
    {
        KiwiPublicReceiverPicker dlg;
        dlg.showReceiversForTest({
            rx("Linz",     "linz.example",   "(48.30, 14.30)", 0, 4, 4),
            rx("Pisa",     "pisa.example",   "(43.70, 10.40)", 2, 4, 4),
            rx("Paris",    "paris.example",  "(48.90, 2.40)",  4, 4, 4),
            rx("Tot",      "tot.example",    "(40.00, 20.00)", 0, 4, 4, true),
            rx("NurWeb",   "web.example",    "(-10.00, -60.00)", 0, 4, 0),
            rx("OhneRegel","unk.example",    "(51.00, 11.00)", 0, 4, -1),
        });
        // Tabelle: nur ext_api > 0 und nicht offline.
        QCOMPARE(dlg.tableForTest()->rowCount(), 3);
        // Karte: dieselben drei und der Offline-Empfaenger (grau, nicht
        // anwaehlbar); der Nur-Web-Empfaenger fehlt auch dort.
        KiwiWorldMap* map = dlg.mapForTest();
        QCOMPARE(map->pointCount(), 4);
        QVERIFY(map->idAt(map->toWidget(-10.0, -60.0)).isEmpty());   // NurWeb
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg, 15000));
        // Klick auf Pisa waehlt die Zeile.
        const QPointF pisa = map->toWidget(43.7, 10.4);
        QTest::mouseClick(map, Qt::LeftButton, {}, pisa.toPoint());
        const int row = dlg.tableForTest()->currentRow();
        QVERIFY(row >= 0);
        QCOMPARE(dlg.tableForTest()->item(row, 0)->text(), QStringLiteral("Pisa"));
        QCOMPARE(map->selectedId(), QStringLiteral("pisa.example:8073"));
        // Offen -> Ring.
        dlg.setOpenEndpoints({QStringLiteral("linz.example:8073")});
        QVERIFY(dlg.mapForTest()->pointCount() == 4);
        // Bild fuer die Durchsicht, falls gewuenscht.
        const QString out = qEnvironmentVariable("LONGPATH_KIWI_MAP_PNG");
        if (!out.isEmpty()) { dlg.grab().save(out); }
    }
};

QTEST_MAIN(TstKiwiWorldMap)
#include "tst_kiwi_world_map.moc"
