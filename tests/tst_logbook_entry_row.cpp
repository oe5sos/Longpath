// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Loggen im Logbuch-Fenster (2026-09-26). Betreiber: "ich sehe hier
// nirgendwo, wo ich das QSO loggen koennte, die Frequenz und die
// Betriebsart geht auch ab. Ich will dieses auch zum Loggen verwenden."
// Gewaehlt: Blatt A, eine Eingabezeile ueber der Suche. Das Rufzeichen
// filtert das Log, Enter loggt ueber das Rotor/Log-Feld (eine Datei, eine
// Doppelt-Regel), Esc leert die Zeile und schliesst NICHT das Logbuch.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/AdifLog.h"
#include "gui/LogbookWindow.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

namespace {
QString writeAdif(const QDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("log.adi"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:6>OE5VVM <QSO_DATE:8>20260610 <TIME_ON:6>150104 <BAND:2>6m <MODE:3>FT8 "
            "<GRIDSQUARE:4>JN67 <MY_GRIDSQUARE:6>JN67VV <EOR>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260902 <TIME_ON:6>143710 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>FN42 <MY_GRIDSQUARE:6>JN67VV <EOR>\n");
    f.close();
    return path;
}
} // namespace

class TstLogbookEntryRow : public QObject { Q_OBJECT
private slots:
    void initTestCase()
    {
        // Das Rotor/Log-Feld schreibt ins echte Logbuch -- hier nie.
        QStandardPaths::setTestModeEnabled(true);
    }

    // Tippen filtert das Log darunter: war er schon da?
    void typingFiltersTheLog()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QLineEdit* call = w.entryCallForTest();
        QVERIFY(call);
        call->setFocus();
        QTest::keyClicks(call, QStringLiteral("oe5vvm"));
        QCOMPARE(call->text(), QStringLiteral("OE5VVM"));      // gross geschrieben
        QCOMPARE(w.searchForTest()->text(), QStringLiteral("OE5VVM"));
    }

    // Enter loggt: das Logbuch gibt Rufzeichen, Rapporte und Kommentar weiter.
    void enterAsksToLog()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QSignalSpy spy(&w, &LogbookWindow::logQsoRequested);
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("OE3AA"));
        QTest::keyClicks(w.entryCommentForTest(), QStringLiteral("Stuhleck"));
        QTest::keyClick(w.entryCommentForTest(), Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        const LogEntry e = spy.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("OE3AA"));
        QCOMPARE(e.rstSent, QStringLiteral("59"));
        QCOMPARE(e.rstRcvd, QStringLiteral("59"));
        QCOMPARE(e.comment, QStringLiteral("Stuhleck"));
        QVERIFY(w.isVisible());
    }

    // Esc leert die Zeile und laesst das Logbuch offen (QDialog schliesst
    // sonst bei Esc).
    void escClearsAndKeepsTheWindow()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("OE3AA"));
        QTest::keyClicks(w.entryCommentForTest(), QStringLiteral("x"));
        QTest::keyClick(w.entryCommentForTest(), Qt::Key_Escape);
        QVERIFY(w.entryCallForTest()->text().isEmpty());
        QVERIFY(w.entryCommentForTest()->text().isEmpty());
        QVERIFY(w.searchForTest()->text().isEmpty());
        QVERIFY(w.isVisible());
    }

    // Ohne leeres Rufzeichen kein Loggen, aber eine Meldung.
    void emptyCallIsNotLogged()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        QSignalSpy spy(&w, &LogbookWindow::logQsoRequested);
        w.entryLogButtonForTest()->click();
        QCOMPARE(spy.count(), 0);
        QVERIFY(w.entryHintForTest()->text().contains(QStringLiteral("callsign")));
    }

    // Ohne Funkgeraet sagt die Zeile das, statt eine Frequenz zu erfinden.
    void withoutARadioTheFrequencyIsADash()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.setRadio(nullptr);
        QCOMPARE(w.entryFreqForTest()->text(), QStringLiteral("—"));
        QVERIFY(!w.entryModeForTest()->isVisible());
    }

    // Der ganze Weg: Eingabezeile -> Rotor/Log-Feld -> Datei -> Tabelle.
    void theRotorPanelWritesWhatTheLogbookAsks()
    {
        QFile::remove(RotorLogbookPanel::logbookPath());
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        panel.showLogbook();
        LogbookWindow* w = panel.findChild<LogbookWindow*>();
        QVERIFY(w);
        QVERIFY(QTest::qWaitForWindowExposed(w));
        QTest::keyClicks(w->entryCallForTest(), QStringLiteral("OE5VVM"));
        QTest::keyClicks(w->entryCommentForTest(), QStringLiteral("6m Es"));
        QTest::keyClick(w->entryCallForTest(), Qt::Key_Return);

        QString err;
        const QVector<LogEntry> all = AdifLog::read(RotorLogbookPanel::logbookPath(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().call, QStringLiteral("OE5VVM"));
        QCOMPARE(all.first().comment, QStringLiteral("6m Es"));
        // Die Zeile ist fuer die naechste Station leer, die Meldung sagt es.
        QVERIFY(w->entryCallForTest()->text().isEmpty());
        QVERIFY(w->entryHintForTest()->text().startsWith(QStringLiteral("Logged OE5VVM")));
        w->hide();
        QFile::remove(RotorLogbookPanel::logbookPath());
    }
};

QTEST_MAIN(TstLogbookEntryRow)
#include "tst_logbook_entry_row.moc"
