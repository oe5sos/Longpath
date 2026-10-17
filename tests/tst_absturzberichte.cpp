// =================================================================
// tests/tst_absturzberichte.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. Am 2026-10-14 lagen sieben Absturzberichte
// zu Longpath in ~/Library/Logs/DiagnosticReports, und keiner von
// ihnen stand im Protokoll des Programms: ein Programm, das
// abstuerzt, schreibt seinen letzten Satz nicht mehr selbst.
//
// CrashReportDigest holt das beim naechsten Start nach. Geprueft wird
// hier mit erfundenen .ips-Dateien in einem Wegwerf-Verzeichnis --
// ohne echten Absturz, ohne die Berichte des Betreibers anzufassen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/CrashReportDigest.h"

using namespace Longpath;

namespace {

// So sieht eine .ips aus: eine JSON-Kopfzeile, danach der Bericht.
void schreibeBericht(const QDir& dir, const QString& name,
                     const QString& zeit, const QString& version,
                     const QString& signal, const QString& grund)
{
    QFile f(dir.filePath(name));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    const QString kopf =
        QStringLiteral("{\"app_name\":\"Longpath\",\"timestamp\":\"%1\","
                       "\"app_version\":\"%2\"}\n")
            .arg(zeit, version);
    const QString leib =
        QStringLiteral("{\"exception\":{\"signal\":\"%1\"},"
                       "\"termination\":{\"indicator\":\"%2\","
                       "\"reasons\":[\"erster Grund\",\"zweiter\"]}}\n")
            .arg(signal, grund);
    f.write((kopf + leib).toUtf8());
    f.close();
}

} // namespace

class TstAbsturzberichte : public QObject {
    Q_OBJECT

private slots:
    void eineDateiWirdZuEinerZeile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir d(tmp.path());
        schreibeBericht(d, QStringLiteral("Longpath-2026-10-13-213636.ips"),
                        QStringLiteral("2026-10-13 21:36:36.00 +0200"),
                        QStringLiteral("0.6.8"), QStringLiteral("SIGSEGV"),
                        QStringLiteral("Segmentation fault: 11"));

        const QString zeile = CrashReportDigest::summariseFile(
            d.filePath(QStringLiteral("Longpath-2026-10-13-213636.ips")));
        QVERIFY2(zeile.contains(QStringLiteral("SIGSEGV")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("0.6.8")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("2026-10-13")), qPrintable(zeile));
        // Der erste Grund ist der sprechende und gehoert dazu.
        QVERIFY2(zeile.contains(QStringLiteral("erster Grund")), qPrintable(zeile));
    }

    // Der Kern: nur was NEU ist. Sonst meldet jeder Start dieselben
    // alten Abstuerze und die Zeile wird zur Tapete.
    void nurWasSeitDemLetztenLaufDazukam()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir d(tmp.path());
        schreibeBericht(d, QStringLiteral("Longpath-alt.ips"),
                        QStringLiteral("2026-10-01 10:00:00.00 +0200"),
                        QStringLiteral("0.6.5"), QStringLiteral("SIGABRT"),
                        QStringLiteral("alt"));
        // Zeitstempel im Dateisystem setzen -- danach richtet sich der Filter.
        // setFileTime verlangt eine GEOEFFNETE Datei -- sonst tut es
        // stillschweigend nichts, und der Prueflauf misst nichts.
        const QDateTime alt = QDateTime::currentDateTimeUtc().addDays(-5);
        {
            QFile f(d.filePath(QStringLiteral("Longpath-alt.ips")));
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY2(f.setFileTime(alt, QFileDevice::FileModificationTime),
                     "Zeitstempel liess sich nicht setzen");
            f.close();
        }

        schreibeBericht(d, QStringLiteral("Longpath-neu.ips"),
                        QStringLiteral("2026-10-14 18:19:59.00 +0200"),
                        QStringLiteral("0.6.8"), QStringLiteral("SIGSEGV"),
                        QStringLiteral("neu"));

        const QDateTime grenze = QDateTime::currentDateTimeUtc().addDays(-1);
        const QStringList zeilen = CrashReportDigest::summarise(d.path(), grenze);
        QCOMPARE(zeilen.size(), 1);
        QVERIFY2(zeilen.first().contains(QStringLiteral("0.6.8")),
                 qPrintable(zeilen.first()));
    }

    void ohneGrenzeKommtAllesUndFremdesBleibtDraussen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir d(tmp.path());
        schreibeBericht(d, QStringLiteral("Longpath-a.ips"),
                        QStringLiteral("2026-10-14 01:00:00.00 +0200"),
                        QStringLiteral("0.6.9"), QStringLiteral("SIGSEGV"),
                        QStringLiteral("a"));
        // Ein fremdes Programm. Es geht uns nichts an.
        schreibeBericht(d, QStringLiteral("Safari-2026-10-14.ips"),
                        QStringLiteral("2026-10-14 02:00:00.00 +0200"),
                        QStringLiteral("18.0"), QStringLiteral("SIGKILL"),
                        QStringLiteral("fremd"));

        const QStringList zeilen = CrashReportDigest::summarise(d.path(), QDateTime());
        QCOMPARE(zeilen.size(), 1);
        QVERIFY(!zeilen.first().contains(QStringLiteral("fremd")));
    }

    // Ein kaputter Bericht darf den Start nicht aufhalten.
    void einUnlesbarerBerichtWirdUebergangen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir d(tmp.path());
        QFile f(d.filePath(QStringLiteral("Longpath-kaputt.ips")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("kein JSON, keine Zeile");
        f.close();

        QVERIFY(CrashReportDigest::summariseFile(
                    d.filePath(QStringLiteral("Longpath-kaputt.ips"))).isEmpty());
        QVERIFY(CrashReportDigest::summarise(d.path(), QDateTime()).isEmpty());
    }

    void einFehlendesVerzeichnisIstKeinFehler()
    {
        QVERIFY(CrashReportDigest::summarise(
                    QStringLiteral("/gibt/es/nicht"), QDateTime()).isEmpty());
        QVERIFY(CrashReportDigest::summarise(QString(), QDateTime()).isEmpty());
    }
};

QTEST_MAIN(TstAbsturzberichte)
#include "tst_absturzberichte.moc"
