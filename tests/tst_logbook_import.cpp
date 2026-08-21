// SPDX-License-Identifier: GPL-3.0-or-later
//
// Prueft das Einlesen einer ADIF-Datei ins Logbuch — am echten
// Fenster, mit einer echten Datei aus einem fremden Programm.
//
// Vorgeschichte: „logbuch leer" (2026-08-21). Der Einlese-Knopf war da
// und verdrahtet, aber ob der Weg dahinter traegt, liess sich nicht
// nachsehen — QFileDialog und QMessageBox warten auf eine Maus, und
// kein Test kam daran vorbei. Deshalb wurde der Weg aufgetrennt
// (importAdifFile + setOperatorHooks), und deshalb steht dieser Test
// hier.
//
// Die Probedatei ist absichtlich KEINE selbstgeschriebene: sie ist ein
// Ausdruck aus Zeus, mit dessen Kopfzeile, dessen Feldreihenfolge und
// dessen Eigenheiten. Eine selbstgebaute Datei prueft nur, ob wir
// lesen koennen, was wir selbst schreiben.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>

#include "gui/LogbookWindow.h"
#include "core/AdifLog.h"

using namespace Longpath;

namespace {

/// Ein Ausdruck aus Zeus 1.1.0, Feld fuer Feld wie das Programm ihn
/// schreibt — samt der Kopfzeile ohne fuehrendes Zeichen und den
/// Feldlaengen in spitzen Klammern.
QByteArray zeusExport()
{
    return QByteArray(
        "ADIF Export from Zeus\n"
        "<ADIF_VER:5>3.1.4\n"
        "<PROGRAMID:4>Zeus\n"
        "<PROGRAMVERSION:5>1.1.0\n"
        "<EOH>\n"
        "\n"
        "<CALL:6>OE5SOS <QSO_DATE:8>20260731 <TIME_ON:6>150104 "
        "<FREQ:9>14.203700 <BAND:3>20m <MODE:3>USB <RST_SENT:2>59 "
        "<RST_RCVD:2>59 <NAME:14>Martin Fischer <COUNTRY:7>Austria <EOR>\n"
        "<CALL:6>KB2UKA <QSO_DATE:8>20260731 <TIME_ON:6>143710 "
        "<FREQ:9>14.202000 <BAND:3>20m <MODE:3>USB <RST_SENT:2>59 "
        "<RST_RCVD:2>59 <NAME:17>DOUGLAS J CERRATO <COUNTRY:13>United States "
        "<STATE:2>NY <EOR>\n"
        "<CALL:6>OE5AOO <QSO_DATE:8>20260731 <TIME_ON:6>150126 "
        "<FREQ:9>14.203700 <BAND:3>20m <MODE:3>USB <RST_SENT:2>59 "
        "<RST_RCVD:2>59 <NAME:14>Michael Wagner <COUNTRY:7>Austria <EOR>\n");
}

QString writeTemp(const QDir& dir, const QString& name,
                  const QByteArray& data)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { return {}; }
    f.write(data);
    return path;
}

} // namespace

class TstLogbookImport : public QObject
{
    Q_OBJECT

private slots:
    /// Der ganze Weg: leeres Logbuch, fremde Datei, drei Verbindungen
    /// drin — und beim naechsten Oeffnen immer noch drin.
    void aZeusExportLandsInTheLog()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());

        const QString logPath = dir.filePath(QStringLiteral("mein-log.adi"));
        const QString adif = writeTemp(dir, QStringLiteral("zeus.adi"),
                                       zeusExport());
        QVERIFY(!adif.isEmpty());

        LogbookWindow w(logPath);
        QStringList said;
        w.setOperatorHooks([](const QString&) { return true; },
                           [&said](const QString& m) { said << m; });

        w.reload();
        QCOMPARE(w.entryCountForTesting(), 0);   // vorher leer

        w.importAdifFile(adif);

        QCOMPARE(w.entryCountForTesting(), 3);
        QVERIFY2(!said.isEmpty(), "Der Import sagt nicht, was er getan hat");
        QVERIFY2(said.last().contains(QStringLiteral("3")),
                 qPrintable(QStringLiteral("Meldung nennt die Zahl nicht: %1")
                                .arg(said.last())));

        // Und es steht wirklich auf der Platte, nicht nur im Fenster.
        QString err;
        const QVector<LogEntry> onDisk = AdifLog::read(logPath, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(onDisk.size(), 3);
    }

    /// Die Felder muessen ankommen, nicht nur die Zeilen gezaehlt werden.
    ///
    /// Eine Einlesefunktion, die drei leere Verbindungen anlegt, besteht
    /// jeden Zaehltest und ist trotzdem wertlos.
    void theFieldsSurviveTheTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());
        const QString logPath = dir.filePath(QStringLiteral("l.adi"));
        const QString adif = writeTemp(dir, QStringLiteral("z.adi"),
                                       zeusExport());

        LogbookWindow w(logPath);
        w.setOperatorHooks([](const QString&) { return true; },
                           [](const QString&) {});
        w.reload();
        w.importAdifFile(adif);

        QString err;
        const QVector<LogEntry> e = AdifLog::read(logPath, &err);
        QCOMPARE(e.size(), 3);

        bool foundDx = false;
        for (const LogEntry& q : e) {
            if (q.call != QLatin1String("KB2UKA")) { continue; }
            foundDx = true;
            QCOMPARE(q.band, QStringLiteral("20m"));
            QCOMPARE(q.mode, QStringLiteral("USB"));
            QCOMPARE(q.rstSent, QStringLiteral("59"));
            QVERIFY2(q.name.contains(QLatin1String("CERRATO")),
                     qPrintable(QStringLiteral("Name verloren: '%1'")
                                    .arg(q.name)));
            QVERIFY2(q.freqMHz > 14.2 && q.freqMHz < 14.21,
                     qPrintable(QStringLiteral("Frequenz falsch: %1")
                                    .arg(q.freqMHz)));
        }
        QVERIFY2(foundDx, "KB2UKA fehlt nach dem Einlesen");
    }

    /// Zweimal dieselbe Datei darf das Logbuch nicht verdoppeln.
    ///
    /// Das ist kein erfundener Fall: im Download-Ordner des Betreibers
    /// liegen fuenf Zeus-Ausdrucke, von denen drei Byte fuer Byte
    /// gleich sind.
    void importingTwiceDoesNotDouble()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());
        const QString logPath = dir.filePath(QStringLiteral("l.adi"));
        const QString a = writeTemp(dir, QStringLiteral("a.adi"), zeusExport());
        const QString b = writeTemp(dir, QStringLiteral("b.adi"), zeusExport());

        LogbookWindow w(logPath);
        QStringList said;
        w.setOperatorHooks([](const QString&) { return true; },
                           [&said](const QString& m) { said << m; });
        w.reload();
        w.importAdifFile(a);
        QCOMPARE(w.entryCountForTesting(), 3);

        w.importAdifFile(b);
        QCOMPARE(w.entryCountForTesting(), 3);
        QVERIFY2(said.last().contains(QLatin1String("already")),
                 qPrintable(QStringLiteral("Kein Hinweis auf Doppelte: %1")
                                .arg(said.last())));
    }

    /// Der Knopf muss auch erreichbar sein, nicht nur verdrahtet.
    ///
    /// Genau diese Luecke — vorhanden, verbunden, aber nicht zu
    /// erwischen — hat in dieser Sitzung schon zweimal Zeit gekostet.
    void theImportButtonCanActuallyBeReached()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        LogbookWindow w(QDir(tmp.path()).filePath(QStringLiteral("l.adi")));
        w.resize(1100, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QPushButton* imp = nullptr;
        for (QPushButton* b : w.findChildren<QPushButton*>()) {
            if (b->text().contains(QLatin1String("mport"))) { imp = b; break; }
        }
        QVERIFY2(imp, "Kein Einlese-Knopf im Logbuch");
        QVERIFY2(imp->isVisible(), "Der Einlese-Knopf ist nicht zu sehen");
        QVERIFY2(imp->isEnabled(), "Der Einlese-Knopf ist tot");

        // Und er liegt im Fenster, nicht rechts daneben abgeschnitten.
        const QPoint tl = imp->mapTo(&w, QPoint(0, 0));
        const QRect inWindow(tl, imp->size());
        QVERIFY2(w.rect().contains(inWindow),
                 qPrintable(QStringLiteral(
                     "Knopf liegt ausserhalb: %1,%2 %3x%4 im Fenster %5x%6")
                     .arg(inWindow.x()).arg(inWindow.y())
                     .arg(inWindow.width()).arg(inWindow.height())
                     .arg(w.width()).arg(w.height())));
    }
};

QTEST_MAIN(TstLogbookImport)
#include "tst_logbook_import.moc"
