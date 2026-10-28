// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_kein_kind_auf_dem_stapel.cpp  (Longpath)
// =================================================================
//
// Fortsetzung von tests/tst_scoped_child_widget.cpp. Dort steht,
// WARUM ein Qt-Kind auf dem Stapel gefaehrlich ist: es dreht in
// exec() seine eigene Ereignisschleife, und stirbt das Elternteil
// darin, raeumt Qt das Kind ab — der Rahmenausgang gibt dann ein
// zweites Mal frei und das Programm bricht ab.
//
// Die Menues wurden am 2026-09-22 umgestellt. Die DIALOGE nicht:
// sechs QMessageBox-Instanzen standen am 2026-10-28 noch auf dem
// Stapel, mit Elternteil. Dazu sechs Menues auf dem Haufen, die
// nach exec() von Hand freigegeben wurden — derselbe Fehler, nur
// andersherum: ist das Elternteil in exec() gestorben, zeigt der
// Zeiger ins Leere, und `delete` oder `deleteLater()` darauf ist
// ein Zugriff nach der Freigabe.
//
// Dieser Lauf liest die Quellen und haelt beide Decken auf NULL.
// Wer einen neuen Dialog braucht, baut ihn durch ScopedChildWidget;
// wer ein Menue auf den Haufen legt, haelt es in einem QPointer.
// =================================================================

#include <QtTest/QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace {

QStringList quelldateien()
{
    QStringList dateien;
    const QString wurzel =
        QStringLiteral(LONGPATH_SRC_DIR) + QStringLiteral("/src");
    QDirIterator it(wurzel, {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        dateien << it.next();
    }
    dateien.sort();
    return dateien;
}

QString kurz(const QString& pfad)
{
    const QString wurzel = QStringLiteral(LONGPATH_SRC_DIR) + QLatin1Char('/');
    return pfad.startsWith(wurzel) ? pfad.mid(wurzel.size()) : pfad;
}

} // namespace

class TstKeinKindAufDemStapel : public QObject
{
    Q_OBJECT

private slots:

    // Ein Dialog dieser Art auf dem Stapel. Die statischen Kuerzel
    // (QMessageBox::warning(...)) sind nicht gemeint und treffen das
    // Muster nicht: dort steht ein `::` statt eines Leerzeichens.
    void keinDialogAufDemStapel()
    {
        static const QRegularExpression muster(
            QStringLiteral("^\\s*(QMessageBox|QColorDialog|QFileDialog|"
                           "QInputDialog|QFontDialog|QProgressDialog)\\s+"
                           "[a-z][A-Za-z0-9_]*\\s*\\("));

        QStringList funde;
        for (const QString& datei : quelldateien()) {
            QFile f(datei);
            QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(datei));
            int nr = 0;
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QString zeile = in.readLine();
                ++nr;
                if (muster.match(zeile).hasMatch()) {
                    funde << QStringLiteral("%1:%2: %3")
                                 .arg(kurz(datei))
                                 .arg(nr)
                                 .arg(zeile.trimmed());
                }
            }
        }

        if (!funde.isEmpty()) {
            QFAIL(qPrintable(
                QStringLiteral(
                    "Dialog auf dem Stapel (%1). Durch ScopedChildWidget "
                    "bauen -- siehe src/gui/ScopedChildWidget.h:\n  %2")
                    .arg(funde.size())
                    .arg(funde.join(QStringLiteral("\n  ")))));
        }
    }

    // Ein Menue auf dem Haufen, das von Hand freigegeben wird. Nach
    // exec() kann das Elternteil es schon geholt haben.
    void keinMenueVonHandFreigeben()
    {
        static const QRegularExpression muster(
            QStringLiteral("^\\s*delete\\s+[A-Za-z0-9_]*[Mm]enu[A-Za-z0-9_]*\\s*;"));

        QStringList funde;
        for (const QString& datei : quelldateien()) {
            QFile f(datei);
            QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(datei));
            int nr = 0;
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QString zeile = in.readLine();
                ++nr;
                if (muster.match(zeile).hasMatch()) {
                    funde << QStringLiteral("%1:%2: %3")
                                 .arg(kurz(datei))
                                 .arg(nr)
                                 .arg(zeile.trimmed());
                }
            }
        }

        if (!funde.isEmpty()) {
            QFAIL(qPrintable(
                QStringLiteral(
                    "Menue von Hand freigegeben (%1). In einen QPointer "
                    "legen und nur freigeben, wenn es noch da ist:\n  %2")
                    .arg(funde.size())
                    .arg(funde.join(QStringLiteral("\n  ")))));
        }
    }

    // Der Lauf muss die Quellen wirklich gelesen haben. Ohne diese
    // Probe waere er gruen, WEIL er nichts fand -- der Fehler, den
    // tst_sonde_bleibt_offline am 2026-10-28 sichtbar gemacht hat.
    void dieQuellenWurdenWirklichGelesen()
    {
        const QStringList dateien = quelldateien();
        QVERIFY2(dateien.size() > 500,
                 qPrintable(QStringLiteral("nur %1 Quelldateien gefunden unter %2")
                                .arg(dateien.size())
                                .arg(QStringLiteral(LONGPATH_SRC_DIR))));
        QVERIFY(dateien.contains(
            QStringLiteral(LONGPATH_SRC_DIR) + QStringLiteral("/src/gui/MainWindow.cpp")));
    }
};

QTEST_MAIN(TstKeinKindAufDemStapel)
#include "tst_kein_kind_auf_dem_stapel.moc"
