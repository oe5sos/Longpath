// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_startmaske.cpp  (Longpath)
// =================================================================
//
// Betreiber, 2026-11-01, mit Bild: „ist wieder alles in der
// startmaske! weg damit."
//
// Die Verbindungsmaske ging bei JEDEM Start auf, legte sich ueber
// alles und raeumte seine sechs schwebenden Fenster hinter sich
// (`hideFloatingWindowsBehindConnectMask`). Wer seine Anordnung ueber
// Tage aufbaut, sieht beim Start erst einmal nichts davon.
//
// Sie gehoert nur dorthin, wo es nichts zu waehlen gibt.
// =================================================================

#include <QtTest/QtTest>

class TstStartmaske : public QObject
{
    Q_OBJECT

private slots:
    // Ein Quelllauf, kein Fensterlauf: `openConnectionPanelOnLaunch`
    // ohne Fenster zu rufen hiesze, das ganze MainWindow zu bauen --
    // und genau dessen Startweg ist hier die Frage.
    void dieMaskeGehtNurOhneGespeichertesGeraetAuf()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        const int a = src.indexOf(
            QStringLiteral("void MainWindow::openConnectionPanelOnLaunch"));
        QVERIFY2(a > 0, "openConnectionPanelOnLaunch ist weg");
        const int e = src.indexOf(QStringLiteral("\n}\n"), a);
        QVERIFY2(e > a, "die Funktion sieht anders aus als erwartet");
        const QString rumpf = src.mid(a, e - a);

        QVERIFY2(rumpf.contains(QStringLiteral("savedRadios()")),
                 "der Start fragt nicht mehr, ob es ueberhaupt ein "
                 "gespeichertes Geraet gibt — dann geht die Maske wieder "
                 "bei jedem Start auf und raeumt die Fenster hinter sich");
        // Und der Ruecksprung muss VOR dem Oeffnen stehen, sonst
        // aendert die Abfrage nichts.
        const int frage  = rumpf.indexOf(QStringLiteral("savedRadios()"));
        const int oeffne = rumpf.indexOf(QStringLiteral("showConnectionPanel()"));
        QVERIFY2(frage > 0 && oeffne > frage,
                 "die Abfrage steht hinter dem Oeffnen — sie bewirkt nichts");
    }
};

QTEST_MAIN(TstStartmaske)
#include "tst_startmaske.moc"
