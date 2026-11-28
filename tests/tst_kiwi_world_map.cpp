// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Kiwi-Weltkarte (Entwurf A, 2026-11-25): stimmt die Kuestenlinie,
// trifft der Klick den Punkt, zeigt der Auswahldialog dieselben
// Empfaenger auf der Karte wie in der Tabelle -- und fehlen die, die der
// Betreiber nicht angefahren haben will, auch dort.

#include <QtTest>
#include <QPolygonF>
#include <QPushButton>
#include <QTableWidget>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QToolButton>

#include "gui/KiwiPublicReceiverPicker.h"
#include "gui/widgets/KiwiWorldBorders.h"
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

    // 2026-11-28, Betreiber: "mit plus und minus vergroessern koennen, so
    // dass man die Kiwis genau erkennen kann".
    void plusUndMinusVergroessern()
    {
        KiwiWorldMap map;
        map.resize(900, 420);
        KiwiMapPoint a;
        a.id = QStringLiteral("a");
        a.name = QStringLiteral("Wels eins");
        a.lat = 48.16;
        a.lon = 14.02;
        KiwiMapPoint b = a;
        b.id = QStringLiteral("b");
        b.name = QStringLiteral("Wels zwei");
        b.lat = 48.24;
        b.lon = 14.22;
        map.setPoints({a, b});
        map.setHome(47.9, 13.8, true, QStringLiteral("JN67VV"));
        map.show();
        QVERIFY(QTest::qWaitForWindowExposed(&map, 15000));
        auto* plus = map.findChild<QToolButton*>(QStringLiteral("kiwiMapZoomIn"));
        auto* minus = map.findChild<QToolButton*>(QStringLiteral("kiwiMapZoomOut"));
        QVERIFY(plus && minus);
        QVERIFY(plus->isVisible() && minus->isVisible());
        QVERIFY(!minus->isEnabled());            // die ganze Welt: kleiner geht nicht

        auto gap = [&map, &a, &b]() {
            return QLineF(map.toWidget(a.lat, a.lon), map.toWidget(b.lat, b.lon)).length();
        };
        const double world = gap();
        QVERIFY(world < 9.0);                    // ein Fleck
        for (int i = 0; i < 8; ++i) { QTest::mouseClick(plus, Qt::LeftButton); }
        QVERIFY2(gap() > 18.0, qPrintable(QString::number(gap())));
        QCOMPARE(map.idAt(map.toWidget(a.lat, a.lon)), QStringLiteral("a"));
        QCOMPARE(map.idAt(map.toWidget(b.lat, b.lon)), QStringLiteral("b"));
        // Vergroessert wird um den eigenen Standort.
        const QPointF home = map.toWidget(47.9, 13.8);
        QVERIFY(QLineF(home, QPointF(450, 210)).length() < 2.0);

        // Ziehen verschiebt und waehlt nichts aus.
        QSignalSpy clicked(&map, &KiwiWorldMap::pointClicked);
        const QPointF from = map.toWidget(a.lat, a.lon);
        const QPointF to = from + QPointF(60, 40);
        QTest::mousePress(&map, Qt::LeftButton, {}, from.toPoint());
        QMouseEvent move(QEvent::MouseMove, to, map.mapToGlobal(to), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&map, &move);
        QTest::mouseRelease(&map, Qt::LeftButton, {}, to.toPoint());
        QVERIFY2(QLineF(map.toWidget(a.lat, a.lon), to).length() < 2.0,
                 "die Karte ging nicht mit");
        QCOMPARE(clicked.count(), 0);

        // Minus fuehrt zurueck zur ganzen Welt.
        for (int i = 0; i < 20 && minus->isEnabled(); ++i) {
            QTest::mouseClick(minus, Qt::LeftButton);
        }
        QVERIFY(!minus->isEnabled());
        QCOMPARE(gap(), world);
    }

    // 2026-11-28, Betreiber: "die Umrisse der Laender sollte man schon sehen".
    void laenderumrisseSindZuSehen()
    {
        QVERIFY(kiwiBorderRingCount() > 200);
        KiwiWorldMap map;
        map.resize(900, 420);
        map.setHome(47.9, 13.8, true, QString());
        map.show();
        QVERIFY(QTest::qWaitForWindowExposed(&map, 15000));
        for (int i = 0; i < 6; ++i) { map.zoomStep(+1); }
        const QImage img = map.grab().toImage();
        const double dpr = img.devicePixelRatio();
        // Jeder Grenzpunkt im Bild (ausser unter Knoepfen und Legende) muss
        // eine helle Linie unter sich haben.
        const QRectF inner(20, 50, 860, 330);
        const short* d = kiwiBorderData();
        int at = 0, seen = 0, lit = 0;
        for (int r = 0; r < kiwiBorderRingCount(); ++r) {
            const int n = d[at++];
            for (int i = 0; i < n; ++i) {
                const QPointF q = map.toWidget(d[at + 2 * i + 1] / 100.0, d[at + 2 * i] / 100.0);
                if (!inner.contains(q)) { continue; }
                ++seen;
                int best = 0;
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        best = std::max(best, img.pixelColor(int((q.x() + dx) * dpr),
                                                             int((q.y() + dy) * dpr)).lightness());
                    }
                }
                if (best >= 60) { ++lit; }
            }
            at += 2 * n;
        }
        QVERIFY2(seen > 40, qPrintable(QString::number(seen)));
        QVERIFY2(lit * 10 >= seen * 9,
                 qPrintable(QStringLiteral("%1 von %2").arg(lit).arg(seen)));
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

    void kartenKnopfBlendetDieKarteAus()
    {
        KiwiPublicReceiverPicker dlg;
        dlg.showReceiversForTest({rx("Linz", "linz.example", "(48.30, 14.30)", 0, 4, 4)});
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg, 15000));
        QPushButton* toggle = nullptr;
        for (QPushButton* b : dlg.findChildren<QPushButton*>()) {
            if (b->isCheckable() && b->text() == KiwiPublicReceiverPicker::tr("Map")) { toggle = b; }
        }
        QVERIFY(toggle);
        QVERIFY(dlg.mapForTest()->isVisible());
        toggle->click();
        QVERIFY(!dlg.mapForTest()->isVisible());
        QVERIFY(dlg.tableForTest()->isVisible());   // die Liste bleibt
        toggle->click();
        QVERIFY(dlg.mapForTest()->isVisible());
    }

    // 2026-11-28 an Martins Station: ein Kiwi mit 4 Programm-Plaetzen bei
    // 4 Nutzern warf Longpath nach 9 Sekunden hinaus.
    void volleProgrammPlaetzeStehenHinten()
    {
        KiwiPublicReceiverPicker dlg;
        dlg.setPreselection(QStringLiteral("JN67VV"), 0.0);
        dlg.setEvictingEndpoints({QStringLiteral("raus.example:8073")});
        dlg.showReceiversForTest({
            // Name, Host, Ort, Nutzer, Plaetze, davon fuer Programme
            rx("Raus",  "raus.example",  "(48.00, 13.90)", 1, 8, 8),   // am naechsten
            rx("Knapp", "knapp.example", "(48.30, 14.30)", 4, 8, 4),   // 4 >= 4
            rx("Frei",  "frei.example",  "(48.90, 2.40)",  2, 8, 4),
            rx("Offen", "offen.example", "(43.70, 10.40)", 5, 8, 8),
        });
        QTableWidget* table = dlg.tableForTest();
        QCOMPARE(table->rowCount(), 4);
        QStringList order;
        for (int r = 0; r < table->rowCount(); ++r) { order << table->item(r, 0)->text(); }
        // Zuerst die brauchbaren nach Entfernung, dann der knappe, ganz hinten
        // wer schon getrennt hat.
        QCOMPARE(order, QStringList({"Offen", "Frei", "Knapp", "Raus"}));
        const int api = 4;
        QVERIFY2(table->item(2, api)->text().contains(QStringLiteral("probably full")),
                 qPrintable(table->item(2, api)->text()));
        QVERIFY2(table->item(3, api)->text().contains(QStringLiteral("disconnected")),
                 qPrintable(table->item(3, api)->text()));
        QVERIFY(!table->item(0, api)->text().contains(QStringLiteral("full")));
        QVERIFY(!table->item(2, api)->toolTip().isEmpty());
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
