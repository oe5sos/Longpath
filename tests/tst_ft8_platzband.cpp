// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf, kein Port.
//
// ── Das Band der laufenden QSOs ───────────────────────────────────────
//
// Gehoert zur Gestaltungsrunde vom 2026-10-24 („uebersichtlich,
// professionell, einfach, high end"). Mit Multi Answering laufen bis zu
// sechs Gespraeche nebeneinander; ohne diese Flaeche sieht der Bediener
// nur die Fundliste und weiss nicht, mit wem er spricht.
//
// Festgenagelt wird das Verhalten, nicht das Aussehen:
//   * leer -> unsichtbar (kostet im Einzelbetrieb keine Zeile)
//   * ein Wartender zeigt keine Tonlage, denn er sendet nicht
//   * der Fortschritt ist auf kSchritteBisQso begrenzt
//
// Mit LONGPATH_GRAB_DIR + LONGPATH_ENTWURF=1 legt er zusaetzlich ein
// Blatt ab.

#include <QtTest>
#include <QApplication>
#include <QStyleFactory>

#include "gui/styles/Theme.h"
#include "gui/styles/AppTheme.h"
#include "gui/widgets/Ft8PlatzBand.h"

using namespace Longpath;

class TstFt8PlatzBand : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
    }

    void leerIstUnsichtbar()
    {
        Ft8PlatzBand band;
        QVERIFY(!band.isVisible());
        QCOMPARE(band.sizeHint().height(), 0);

        band.setzePlaetze({{QStringLiteral("DL1ABC"), QStringLiteral("R-12"),
                            1500, 3, false}}, 6);
        QCOMPARE(band.plaetzeForTest().size(), 1);
        QVERIFY(band.sizeHint().height() > 0);

        band.setzePlaetze({}, 6);
        QVERIFY(!band.isVisible());
    }

    void malenStuerztNichtAb()
    {
        Ft8PlatzBand band;
        band.resize(880, 70);
        band.setzePlaetze({
            {QStringLiteral("DL1ABC"), QStringLiteral("R-12"), 1500, 3, false},
            {QStringLiteral("G0XYZ"),  QStringLiteral("RR73"), 1560, 4, false},
            {QStringLiteral("F5QRP"),  QString(),              1620, 0, true },
        }, 6);
        band.show();
        QTest::qWait(150);

        const QImage bild = band.grab().toImage();
        QCOMPARE(bild.size(), QSize(880, 70));

        // Nicht ganz leer: ein Band, das nichts malt, sieht aus wie
        // keins. Zaehlt die Punkte, die nicht Hintergrund sind.
        int gemalt = 0;
        for (int y = 0; y < bild.height(); y += 2) {
            for (int x = 0; x < bild.width(); x += 2) {
                if (qGray(bild.pixel(x, y)) > 40) { ++gemalt; }
            }
        }
        QVERIFY2(gemalt > 100,
                 qPrintable(QStringLiteral("nur %1 gemalte Punkte").arg(gemalt)));

        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()
            && qEnvironmentVariable("LONGPATH_ENTWURF") == QStringLiteral("1")) {
            const QString pfad = dir + QStringLiteral("/ft8-platzband.png");
            QVERIFY(bild.save(pfad));
            qInfo() << "Blatt:" << pfad << bild.size();
        }
    }

    void derFortschrittLaeuftNichtUeber()
    {
        Ft8PlatzBand band;
        band.resize(400, 70);
        // 99 Schritte gibt es nicht -- die Linie darf trotzdem nicht
        // ueber den Rand laufen.
        band.setzePlaetze({{QStringLiteral("DL1ABC"), QStringLiteral("73"),
                            1500, 99, false}}, 1);
        band.show();
        QTest::qWait(100);
        const QImage bild = band.grab().toImage();
        QCOMPARE(bild.width(), 400);
    }
};

QTEST_MAIN(TstFt8PlatzBand)
#include "tst_ft8_platzband.moc"
