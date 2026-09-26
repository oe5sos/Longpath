// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// Binaere Werte in AppSettings (2026-09-26). Jeder Wert wurde als Text
// gespeichert (QVariant::toString); ein QByteArray aus saveGeometry()
// oder saveState() wurde dabei als UTF-8 gelesen, und jedes ungueltige
// Byte zu U+FFFD. In der Betreiber-Datei standen zwoelf Fensterzustaende
// als Ersatzzeichen -- Logbuch, Rotor/Log-Fenster, schwebende
// Panadapter, Antennen-, Karten-, Verbindungsfenster ... --, und keines
// dieser Fenster kam je an seine Stelle zurueck.
//
// Geprueft: echte Fensterzustaende ueberleben Speichern + Neuladen Byte
// fuer Byte; Text bleibt auf der Platte, wie er war (auch selbst
// base64-kodierte Zustaende, wie MainWindow sie schreibt); ein alter,
// schon kaputter Wert fuehrt zu keinem Absturz, nur zur Vorgabe.

#include <QtTest/QtTest>
#include <QFile>
#include <QSplitter>
#include <QTemporaryDir>
#include <QWidget>

#include "core/AppSettings.h"

using namespace Longpath;

class TstAppSettingsBinaryValues : public QObject {
    Q_OBJECT

    static QByteArray fileBytes(const QString& path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

private slots:
    void windowGeometrySurvivesSaveAndReload()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("Longpath.settings"));

        QWidget w;
        w.setGeometry(123, 234, 640, 410);
        const QByteArray geom = w.saveGeometry();
        QSplitter split;
        split.addWidget(new QWidget);
        split.addWidget(new QWidget);
        split.addWidget(new QWidget);
        split.resize(900, 300);
        split.setSizes({180, 520, 200});
        const QByteArray state = split.saveState();
        // Genau die Art Bytes, die als Text verloren gingen.
        QVERIFY(QString::fromUtf8(geom).toUtf8() != geom
                || geom.contains('\0'));

        {
            AppSettings s(path);
            s.setValue(QStringLiteral("LogbookGeometryState"), geom);
            s.setValue(QStringLiteral("LogbookSplitState3"), state);
            // Stationswerte werden nur unter einem Stationsnamen
            // gespeichert; hier zaehlt der Weg hinein und heraus.
            s.setStationValue(QStringLiteral("StationGeom"), geom);
            QCOMPARE(s.stationValue(QStringLiteral("StationGeom")).toByteArray(), geom);
            s.setHardwareValue(QStringLiteral("00:11:22:33:44:55"),
                               QStringLiteral("blob"), state);
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("LogbookGeometryState")).toByteArray(), geom);
            QCOMPARE(s.value(QStringLiteral("LogbookSplitState3")).toByteArray(), state);
            QCOMPARE(s.hardwareValue(QStringLiteral("00:11:22:33:44:55"),
                                     QStringLiteral("blob")).toByteArray(), state);
            QCOMPARE(s.hardwareValues(QStringLiteral("00:11:22:33:44:55"))
                         .value(QStringLiteral("blob")).toByteArray(), state);

            // Und das Fenster nimmt es an.
            QWidget back;
            QVERIFY(back.restoreGeometry(
                s.value(QStringLiteral("LogbookGeometryState")).toByteArray()));
            QSplitter split2;
            split2.addWidget(new QWidget);
            split2.addWidget(new QWidget);
            split2.addWidget(new QWidget);
            QVERIFY(split2.restoreState(
                s.value(QStringLiteral("LogbookSplitState3")).toByteArray()));
        }
        // Keine Ersatzzeichen in der Datei.
        QVERIFY(!fileBytes(path).contains("\xef\xbf\xbd"));
    }

    void textStaysTextOnDisk()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("Longpath.settings"));
        const QByteArray b64 = QByteArray("\x01\x02\xd9\xd0 geometry", 14).toBase64();
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("PlainText"), QStringLiteral("Grüße"));
            s.setValue(QStringLiteral("Utf8Bytes"), QByteArray("Grüße aus Laakirchen"));
            s.setValue(QStringLiteral("SelfEncoded"), b64);   // wie MainWindow
            s.setValue(QStringLiteral("Flag"), true);
            s.save();
        }
        const QByteArray disk = fileBytes(path);
        QVERIFY(disk.contains("Grüße aus Laakirchen"));
        QVERIFY(disk.contains(b64));
        QVERIFY(!disk.contains("@ByteArray("));
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("PlainText")).toString(), QStringLiteral("Grüße"));
            QCOMPARE(s.value(QStringLiteral("Utf8Bytes")).toString(),
                     QStringLiteral("Grüße aus Laakirchen"));
            QCOMPARE(s.value(QStringLiteral("SelfEncoded")).toByteArray(), b64);
            QCOMPARE(s.value(QStringLiteral("Flag")).toString(), QStringLiteral("true"));
        }
    }

    // Was vor der Behebung geschrieben wurde: eine Kette von U+FFFD.
    // Kein Absturz, das Fenster geht an der Vorgabe auf -- wie bisher --,
    // und der naechste Schliessen-Vorgang schreibt einen heilen Wert.
    void anOldBrokenValueFallsBackQuietly()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("Longpath.settings"));
        QWidget w;
        w.setGeometry(50, 60, 300, 200);
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("ToolWindowGeometry_RotorLog"),
                       QString::fromUtf8(w.saveGeometry()));   // der alte Weg
            s.save();
        }
        AppSettings s(path);
        s.load();
        QWidget back;
        QVERIFY(!back.restoreGeometry(
            s.value(QStringLiteral("ToolWindowGeometry_RotorLog")).toByteArray()));
        s.setValue(QStringLiteral("ToolWindowGeometry_RotorLog"), w.saveGeometry());
        QVERIFY(back.restoreGeometry(
            s.value(QStringLiteral("ToolWindowGeometry_RotorLog")).toByteArray()));
    }

    // Nichts Neues, nichts zu schreiben: dieselben Bytes zweimal gesetzt
    // machen die Datei nicht schmutzig (der Vergleich laeuft ueber die
    // gespeicherte Form).
    void settingTheSameBytesTwiceIsNotAChange()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("Longpath.settings"));
        QWidget w;
        const QByteArray geom = w.saveGeometry();
        AppSettings s(path);
        s.setValue(QStringLiteral("G"), geom);
        s.save();
        const QByteArray first = fileBytes(path);
        s.setValue(QStringLiteral("G"), geom);
        s.save();
        QCOMPARE(fileBytes(path), first);
    }
};

QTEST_MAIN(TstAppSettingsBinaryValues)
#include "tst_app_settings_binary_values.moc"
