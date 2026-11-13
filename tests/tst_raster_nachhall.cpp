// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_raster_nachhall.cpp  (Longpath)
// =================================================================
//
// Wann ist ein Sprung des Hauptfensters nur das Nachrasten des Rasters
// (snapToGridAfterSettle) nach einer Bewegung des Betreibers, und wann
// eine Bewegung fuer sich? Davon haengt ab, ob die schwebenden Fenster
// mitgezogen werden (MainWindow::schutzrahmenNachziehen).
//
// Am Geraet am 2026-11-11: das Hauptfenster lag bei y = 33 direkt unter
// der Menueleiste, der Betreiber zog es mit einem schnellen Zug kleiner.
// Das Raster darf nicht auf 32, also auf 48 (+15), und rastete die
// Unterkante ein: Hoehe 738 -> 717 (-21). Mit der alten Schwelle (eine
// Rasterweite auch fuer die Groesse) galt das als eigene Bewegung, und
// die schwebenden Fenster blieben stehen -- „wieder verschoben".
//
// Gegen die alte Schwelle faellt Fall 1 um.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "gui/WindowPlacement.h"

using namespace Longpath;

class TstRasterNachhall : public QObject {
    Q_OBJECT

private slots:
    // Fall 1: genau der Fall vom Geraet.
    void unterDerMenueleisteIstNachhall()
    {
        const QRect vorher(0, 33, 1232, 738);
        const QRect jetzt(0, 48, 1229, 717);
        QVERIFY(istRasterNachhall(jetzt, vorher, 16));
    }

    void kleinesEinrastenIstNachhall()
    {
        QVERIFY(istRasterNachhall(QRect(224, 160, 1504, 800), QRect(221, 155, 1497, 793), 16));
    }

    // Eine echte Bewegung oder Groessenaenderung ist kein Nachhall.
    void echteBewegungIstKeinNachhall()
    {
        QVERIFY(!istRasterNachhall(QRect(131, 119, 1021, 637), QRect(224, 160, 1021, 637), 16));
        QVERIFY(!istRasterNachhall(QRect(0, 48, 1069, 637), QRect(0, 48, 1470, 864), 16));
        QVERIFY(!istRasterNachhall(QRect(0, 48, 1100, 637), QRect(0, 48, 1069, 637), 16));
    }

    // 2026-11-13: Protokoll 12.11., 17:17 -- maximiert, aber 1470x923
    // bei freien 1470x867 (Dock). Das muss neu maximiert werden; die zwei
    // Lagen eines normal maximierten Fensters (vor und nach dem Raster)
    // nicht.
    void ueberDenFreienPlatz()
    {
        const QRect frei(0, 33, 1470, 867);
        QVERIFY(ragtUeberDenFreienPlatz(QRect(0, 33, 1470, 923), frei));
        QVERIFY(!ragtUeberDenFreienPlatz(QRect(1, 33, 1468, 865), frei));
        QVERIFY(!ragtUeberDenFreienPlatz(QRect(0, 33, 1470, 867), frei));
        QVERIFY(!ragtUeberDenFreienPlatz(QRect(0, 33, 1470, 869), frei));   // Raster
        QVERIFY(!ragtUeberDenFreienPlatz(QRect(200, 100, 800, 600), frei)); // kleiner
        QVERIFY(!ragtUeberDenFreienPlatz(QRect(), frei));
    }

    void ungueltigIstKeinNachhall()
    {
        QVERIFY(!istRasterNachhall(QRect(), QRect(0, 0, 100, 100), 16));
        QVERIFY(!istRasterNachhall(QRect(0, 0, 100, 100), QRect(), 16));
    }
};

QTEST_MAIN(TstRasterNachhall)
#include "tst_raster_nachhall.moc"
