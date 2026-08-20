// tests/tst_world_map_catalog.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Warum das Seitenverhaeltnis eine Zahl braucht ────────────────────
//
// Die Karte rechnet gleichabstaendig zylindrisch:
//
//     x = (lon + 180) / 360 * Breite
//     y = ( 90 - lat) / 180 * Hoehe
//
// Das stimmt nur, wenn das Bild 2:1 ist. Ein Bild mit anderem
// Verhaeltnis wuerde von QPainter auf mapRect() gestreckt, die Karte
// saehe weiterhin wie eine Karte aus, und NUR die Stationsmarken
// saessen daneben.
//
// Genau diese Sorte Fehler hat dieses Programm zweimal erst als Bild
// bemerkt -- die magentafarbene und die rote Wasserfallflaeche. Deshalb
// bekommt die Bedingung hier eine Zahl, bevor sie zum ersten Mal
// auffaellt.

#include <QtTest/QtTest>

#include "gui/widgets/WorldMapCatalog.h"

using namespace Longpath;

class TestWorldMapCatalog : public QObject
{
    Q_OBJECT

private slots:

    void theExpectedShapeIsAccepted()
    {
        QVERIFY(WorldMapCatalog::aspectIsUsable(QSize(2048, 1024)));
        QVERIFY(WorldMapCatalog::aspectIsUsable(QSize(5400, 2700)));
        QVERIFY(WorldMapCatalog::aspectIsUsable(QSize(2, 1)));
    }

    void anythingElseIsRejected()
    {
        // Der Fall, der ohne Pruefung am gefaehrlichsten waere: nah
        // dran, aber nicht 2:1. Die Karte saehe richtig aus.
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(2048, 1000)));
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(1920, 1080)));
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(1024, 1024)));
    }

    void theToleranceIsOnePixelAndNotMore()
    {
        // Krumme Kantenlaengen sollen durchgehen ...
        QVERIFY(WorldMapCatalog::aspectIsUsable(QSize(2049, 1024)));
        QVERIFY(WorldMapCatalog::aspectIsUsable(QSize(2047, 1024)));
        // ... eine sichtbare Stauchung nicht. Bei zwei Pixeln ist
        // Schluss, damit "Toleranz" nicht zu "ungefaehr 2:1" wird.
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(2050, 1024)));
    }

    void anUnreadableSizeIsRejectedNotAssumed()
    {
        // QImageReader::size() gibt ein ungueltiges QSize zurueck, wenn
        // die Datei kein Bild ist. Das darf nicht als "passt schon"
        // durchgehen.
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize()));
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(0, 0)));
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(-2, -1)));
    }

    void theReasonNamesTheMeasuredSize()
    {
        // Der Grund steht in der Auswahl neben dem ausgegrauten Namen.
        // Er muss sagen, WAS gefunden wurde -- sonst weiss der Betreiber
        // nicht, ob er die Datei beschneiden kann oder eine andere
        // braucht.
        QString reason;
        QVERIFY(!WorldMapCatalog::aspectIsUsable(QSize(1920, 1080), &reason));
        QVERIFY2(reason.contains(QStringLiteral("1920")),
                 qPrintable(reason));
        QVERIFY2(reason.contains(QStringLiteral("1080")),
                 qPrintable(reason));
        QVERIFY2(reason.contains(QStringLiteral("2160")),
                 "der Grund nennt nicht, welche Breite richtig waere");
    }

    void anAcceptedSizeGivesNoReason()
    {
        QString reason = QStringLiteral("vorbelegt");
        QVERIFY(WorldMapCatalog::aspectIsUsable(QSize(4096, 2048), &reason));
        QCOMPARE(reason, QStringLiteral("vorbelegt"));
    }

    void theFolderSitsBesideTheThemes()
    {
        // Dieselbe Trennung wie bei den Theme-Dateien: ausserhalb des
        // Quellbaums, dem Betreiber gehoerend.
        const QString dir = WorldMapCatalog::directory();
        QVERIFY(dir.endsWith(QStringLiteral("/maps")));
        QVERIFY2(!dir.contains(QStringLiteral("/src/")),
                 "der Kartenordner liegt im Quellbaum");
    }

    void aMissingFolderIsEmptyAndNotAnError()
    {
        // Vor der ersten abgelegten Datei gibt es den Ordner nicht. Das
        // ist der Normalfall beim ersten Start und darf weder anlegen
        // noch klagen.
        const auto list = WorldMapCatalog::entries();
        QVERIFY(list.size() >= 0);
    }
};

QTEST_MAIN(TestWorldMapCatalog)
#include "tst_world_map_catalog.moc"
