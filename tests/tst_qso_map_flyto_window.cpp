// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_qso_map_flyto_window.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Das Kartenfenster als Ganzes: ein Rufzeichen im Feld, Enter, und die
// flache Karte fliegt dorthin, wo der Rueckfall die Station verortet —
// ohne QRZ-Konto und ohne Log, wie beim allerersten Start. Dazu die
// Stationskarte mit Entfernung und Peilung vom eigenen Standort.
//
// Mit LONGPATH_GRAB_DIR gesetzt wird zusaetzlich mit ECHTEN Kacheln aus
// dem Netz gewartet und das Fenster als PNG abgelegt — das ist das Bild,
// das kein Pruefstand ersetzt. Ohne die Variable bleibt das Netz aus.

#include <QtTest>

#include "gui/QsoMapWindow.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GibsTileLayer.h"

#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>

using namespace Longpath;

class TstQsoMapFlyToWindow : public QObject { Q_OBJECT
private slots:
    void enter_in_the_call_field_flies_the_flat_map_there()
    {
        QsoMapWindow w;
        w.resize(1100, 620);
        w.setHomeGrid(QStringLiteral("JN78GH"));   // Linz
        // Der Rueckfall spielt cty.dat: W-Rufzeichen liegen bei Kansas.
        w.setPositionFallback([](const QString& call, double& lat, double& lon) {
            if (!call.startsWith(QLatin1Char('W'))) { return false; }
            lat = 39.0; lon = -98.0; return true;
        });
        w.imagery()->setNetworkEnabled(false);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        auto* edit = w.findChild<QLineEdit*>();
        QVERIFY(edit);
        FlatMapWidget* flat = w.flatMapForTest();
        QVERIFY(flat);
        QSignalSpy done(flat, &FlatMapWidget::flightFinished);

        QTest::keyClicks(edit, QStringLiteral("w1aw"));
        QTest::keyClick(edit, Qt::Key_Return);

        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 6000);
        QCOMPARE(flat->focusStation(), QStringLiteral("W1AW"));
        double lat = 0.0, lon = 0.0;
        QVERIFY(flat->viewCentre(lat, lon));
        QVERIFY2(std::abs(lat - 39.0) < 0.1, qPrintable(QString::number(lat)));
        QVERIFY2(std::abs(lon + 98.0) < 0.1, qPrintable(QString::number(lon)));
        QVERIFY(flat->zoom() > 100.0);   // nah dran, nicht Weltansicht

        // Die Stationskarte nennt Rufzeichen, Entfernung und Peilung.
        QLabel* card = nullptr;
        for (QLabel* l : w.findChildren<QLabel*>()) {
            if (l->isVisible() && l->text().contains(QStringLiteral("W1AW"))) { card = l; break; }
        }
        QVERIFY(card);
        QVERIFY2(card->text().contains(QStringLiteral(" km")), qPrintable(card->text()));
        QVERIFY2(card->text().contains(QStringLiteral("°")), qPrintable(card->text()));

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (grabDir.isEmpty()) { return; }

        // Mit Netz: Kacheln kommen lassen, dann das Bild ablegen.
        w.imagery()->setNetworkEnabled(true);
        flat->flyTo(lat, lon, flat->zoom(), 0);   // erneut zeichnen, jetzt mit Abruf
        QSignalSpy tiles(w.imagery(), &GibsTileLayer::tileReady);
        QTRY_VERIFY_WITH_TIMEOUT(tiles.count() >= 4, 30000);
        QTest::qWait(1500);
        flat->repaint();
        const QString out = grabDir + QStringLiteral("/longpath-grab-QsoMapWindow-flyto.png");
        QVERIFY(w.grab().save(out));
        qInfo() << "grab written to" << out << "tiles:" << tiles.count()
                << "imagery painted:" << flat->imageryPainted();
        QVERIFY(flat->imageryPainted());
    }
};

QTEST_MAIN(TstQsoMapFlyToWindow)
#include "tst_qso_map_flyto_window.moc"
