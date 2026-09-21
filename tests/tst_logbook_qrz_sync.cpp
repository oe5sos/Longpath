// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbook_qrz_sync.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Sync QRZ im Logbuchfenster, ohne Netz: was QRZ liefert, wird wie ein
// Datei-Import zusammengefuehrt — Neues kommt dazu, ein schon
// vorhandener Kontakt bekommt die QRZ-Kennung und die Bestaetigung, die
// ihm fehlten, und gilt danach als bei QRZ vorhanden, damit der Upload
// ihn nicht noch einmal schickt. Nichts Sichtbares wird ueberschrieben:
// der lokale Name bleibt, auch wenn QRZ einen anderen kennt.

#include <QtTest>

#include "core/AdifLog.h"
#include "gui/LogbookWindow.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace Longpath;

namespace {
QString writeTemp(const QDir& dir, const QString& name, const QByteArray& data)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { return {}; }
    f.write(data);
    return path;
}
}

class TstLogbookQrzSync : public QObject { Q_OBJECT
private slots:
    void fetched_contacts_merge_like_an_import_and_are_marked_as_at_qrz()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());
        // Ein Log mit einem Kontakt, den QRZ auch kennt — mit lokal
        // gepflegtem Namen — und einem, den QRZ nicht hat.
        const QString logPath = writeTemp(dir, QStringLiteral("log.adi"),
            "ADIF Export\n<ADIF_VER:5>3.1.4\n<EOH>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260921 <TIME_ON:6>000600 <BAND:3>80m "
            "<MODE:3>FT8 <NAME:12>Alice (edit) <EOR>\n"
            "<CALL:5>OE3AA <QSO_DATE:8>20260920 <TIME_ON:6>190000 <BAND:3>40m "
            "<MODE:3>SSB <EOR>\n");
        QVERIFY(!logPath.isEmpty());

        LogbookWindow w(logPath);
        QStringList said;
        w.setOperatorHooks([](const QString&) { return true; },
                           [&said](const QString& m) { said << m; });
        w.reload();
        QCOMPARE(w.entryCountForTesting(), 2);

        // Was QRZ liefert: derselbe K1ABC-Kontakt (eine Minute versetzt,
        // mit Kennung, Bestaetigung und einem anderen Namen) und zwei neue.
        const QString adif = QStringLiteral(
            "<CALL:5>K1ABC <QSO_DATE:8>20260921 <TIME_ON:6>000700 <BAND:3>80m "
            "<MODE:3>FT8 <NAME:14>Alice B. Cooke <APP_QRZLOG_LOGID:9>987654321 "
            "<QSL_RCVD:1>Y <APP_QRZLOG_STATUS:1>C <EOR>\n"
            "<CALL:5>W2XYZ <QSO_DATE:8>20260921 <TIME_ON:6>000400 <BAND:3>80m "
            "<MODE:3>FT8 <APP_QRZLOG_LOGID:9>987654322 <EOR>\n"
            "<CALL:5>N3DEF <QSO_DATE:8>20260921 <TIME_ON:6>000200 <BAND:3>80m "
            "<MODE:3>FT8 <APP_QRZLOG_LOGID:9>987654323 <EOR>\n");
        w.mergeQrzAdifForTest(adif);

        QCOMPARE(w.entryCountForTesting(), 4);
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            w.resize(1400, 700);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            QTest::qWait(300);
            const QString out = grabDir + QStringLiteral("/longpath-grab-LogbookWindow-syncqrz.png");
            QVERIFY(w.grab().save(out));
            qInfo() << "grab written to" << out;
        }
        QVERIFY2(!said.isEmpty(), "der Abgleich sagt nicht, was er getan hat");
        QVERIFY2(said.last().contains(QStringLiteral("Added 2")), qPrintable(said.last()));
        QVERIFY2(said.last().contains(QStringLiteral("Marked 1")), qPrintable(said.last()));

        QString err;
        const QVector<LogEntry> onDisk = AdifLog::read(logPath, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(onDisk.size(), 4);

        const LogEntry* kh = nullptr; const LogEntry* oe = nullptr; const LogEntry* w2 = nullptr;
        for (const LogEntry& e : onDisk) {
            if (e.call == QStringLiteral("K1ABC")) { kh = &e; }
            if (e.call == QStringLiteral("OE3AA")) { oe = &e; }
            if (e.call == QStringLiteral("W2XYZ"))  { w2 = &e; }
        }
        QVERIFY(kh && oe && w2);
        // Der lokal gepflegte Name bleibt; Kennung und Bestaetigung kommen dazu.
        QCOMPARE(kh->name, QStringLiteral("Alice (edit)"));
        QVERIFY(kh->uploadedToQrz);
        auto extra = [](const LogEntry& e, const QString& tag) {
            for (const auto& kv : e.extras) { if (kv.first.compare(tag, Qt::CaseInsensitive) == 0) { return kv.second; } }
            return QString();
        };
        QCOMPARE(extra(*kh, QStringLiteral("APP_QRZLOG_LOGID")), QStringLiteral("987654321"));
        QCOMPARE(extra(*kh, QStringLiteral("QSL_RCVD")), QStringLiteral("Y"));
        // Der Kontakt, den QRZ nicht kennt, bleibt unmarkiert.
        QVERIFY(!oe->uploadedToQrz);
        // Ein neuer aus QRZ ist dort ohnehin vorhanden.
        QVERIFY(w2->uploadedToQrz);
        QCOMPARE(extra(*w2, QStringLiteral("APP_QRZLOG_LOGID")), QStringLiteral("987654322"));
    }

    void a_second_sync_changes_nothing_and_says_so()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());
        const QString logPath = dir.filePath(QStringLiteral("log.adi"));
        LogbookWindow w(logPath);
        QStringList said;
        w.setOperatorHooks([](const QString&) { return true; },
                           [&said](const QString& m) { said << m; });
        w.reload();
        const QString adif = QStringLiteral(
            "<CALL:5>W2XYZ <QSO_DATE:8>20260921 <TIME_ON:6>000400 <BAND:3>80m "
            "<MODE:3>FT8 <APP_QRZLOG_LOGID:9>987654322 <EOR>\n");
        w.mergeQrzAdifForTest(adif);
        QCOMPARE(w.entryCountForTesting(), 1);
        said.clear();
        w.mergeQrzAdifForTest(adif);
        QCOMPARE(w.entryCountForTesting(), 1);
        QVERIFY2(!said.isEmpty() && said.last().contains(QStringLiteral("Nothing to do")),
                 qPrintable(said.isEmpty() ? QStringLiteral("(keine Meldung)") : said.last()));
    }
};

QTEST_MAIN(TstLogbookQrzSync)
#include "tst_logbook_qrz_sync.moc"
