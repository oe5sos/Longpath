// =================================================================
// tests/tst_aufnahme_zeitplan.cpp  (Longpath)
// =================================================================
//
// Die Bedienung des Zeitplans. `RecordingScheduler` laeuft seit
// August, speichert und feuert -- aber bis zum 2026-10-22 gab es
// keinen Weg, einen Eintrag ANZULEGEN: das Applet zeigte „none" und
// konnte nur zusehen.
//
// Was hier festgenagelt wird:
//   * „Add" legt wirklich einen Eintrag im Scheduler an
//   * die Zeile traegt Zeit, Art und Dauer, und zwar in UTC
//   * „Remove" loescht den AUSGEWAEHLTEN, nicht den ersten
//   * ohne Radio sagt der Hinweis, dass nichts feuern wird
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>

#include "gui/RecordingScheduleDialog.h"
#include "models/RadioModel.h"
#include "core/audio/RecordingScheduler.h"
#include "models/RecordingModel.h"

using namespace Longpath;

class TstAufnahmeZeitplan : public QObject
{
    Q_OBJECT

private:
    // Der Zeitplan ist ECHT persistent (add() speichert, der Scheduler
    // laedt beim attach()) -- also teilen sich die Prueffaelle sonst
    // die Eintraege des vorigen, und der zweite misst den ersten.
    static void leere(RecordingModel& rec)
    {
        for (const ScheduledRecording& e : rec.schedule().entries()) {
            rec.schedule().remove(e.id);
        }
        rec.schedule().saveToSettings();
        QCOMPARE(rec.scheduleCount(), 0);
    }

private slots:

    void derKnopfLegtWirklichEinenEintragAn()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);
        leere(rec);
        const int vorher = rec.scheduleCount();

        RecordingScheduleDialog dlg(&rec);
        QVERIFY(dlg.addButtonForTest());
        dlg.startForTest()->setDateTime(
            QDateTime(QDate(2026, 12, 24), QTime(3, 5), QTimeZone::UTC));
        dlg.dauerForTest()->setValue(20);
        dlg.artForTest()->setCurrentIndex(1);   // I/Q

        dlg.addButtonForTest()->click();

        QCOMPARE(rec.scheduleCount(), vorher + 1);
        QCOMPARE(dlg.listeForTest()->count(), vorher + 1);

        // Die Zeile muss die drei Angaben tragen -- sonst steht dort
        // „Eintrag 1" und niemand weiss, was er geplant hat.
        const QString zeile = dlg.listeForTest()->item(vorher)->text();
        QVERIFY2(zeile.contains(QStringLiteral("2026-12-24")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("03:05")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("I/Q")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("20 min")), qPrintable(zeile));
        // UTC, nicht Ortszeit: eine Aufnahme ueber Mitternacht traegt
        // sonst zweimal denselben Namen.
        QVERIFY2(zeile.contains(QStringLiteral("Z")), qPrintable(zeile));
    }

    void dauerNullHeisstOffenesEnde()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);
        leere(rec);

        RecordingScheduleDialog dlg(&rec);
        dlg.dauerForTest()->setValue(0);
        dlg.addButtonForTest()->click();

        const QString zeile = dlg.listeForTest()->item(0)->text();
        QVERIFY2(zeile.contains(QStringLiteral("open end")), qPrintable(zeile));
    }

    void loeschenTrifftDenAusgewaehlten()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);
        leere(rec);

        RecordingScheduleDialog dlg(&rec);
        dlg.startForTest()->setDateTime(
            QDateTime(QDate(2026, 12, 24), QTime(3, 0), QTimeZone::UTC));
        dlg.addButtonForTest()->click();
        dlg.startForTest()->setDateTime(
            QDateTime(QDate(2026, 12, 25), QTime(4, 0), QTimeZone::UTC));
        dlg.addButtonForTest()->click();
        QCOMPARE(dlg.listeForTest()->count(), 2);

        // Den ZWEITEN auswaehlen und loeschen. Haengt die Loeschung an
        // der Zeilennummer statt an der Kennung, verschwindet hier der
        // falsche -- und das faellt erst auf, wenn eine Aufnahme
        // ausbleibt.
        dlg.listeForTest()->setCurrentRow(1);
        dlg.removeButtonForTest()->click();

        QCOMPARE(dlg.listeForTest()->count(), 1);
        QCOMPARE(rec.scheduleCount(), 1);
        const QString rest = dlg.listeForTest()->item(0)->text();
        QVERIFY2(rest.contains(QStringLiteral("2026-12-24")), qPrintable(rest));
    }

    void ohneAuswahlLoeschtErNichts()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);
        leere(rec);

        RecordingScheduleDialog dlg(&rec);
        dlg.addButtonForTest()->click();
        dlg.listeForTest()->setCurrentRow(-1);
        dlg.removeButtonForTest()->click();

        QCOMPARE(rec.scheduleCount(), 1);
    }

    void einBlattZumAnsehen()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);
        leere(rec);

        RecordingScheduleDialog dlg(&rec);
        dlg.resize(420, 360);
        dlg.show();
        QTest::qWait(40);
        dlg.startForTest()->setDateTime(
            QDateTime(QDate(2026, 12, 24), QTime(3, 0), QTimeZone::UTC));
        dlg.dauerForTest()->setValue(30);
        dlg.addButtonForTest()->click();
        dlg.startForTest()->setDateTime(
            QDateTime(QDate(2026, 12, 25), QTime(5, 30), QTimeZone::UTC));
        dlg.dauerForTest()->setValue(0);
        dlg.artForTest()->setCurrentIndex(1);
        dlg.addButtonForTest()->click();
        QTest::qWait(40);

        const QString pfad = QDir::tempPath()
                             + QStringLiteral("/longpath-zeitplan.png");
        QVERIFY(dlg.grab().save(pfad));
        qInfo() << "Blatt:" << pfad;
    }

    void ohneRadioSagtErDassNichtsFeuert()
    {
        RecordingModel rec;            // kein attach()
        RecordingScheduleDialog dlg(&rec);
        dlg.addButtonForTest()->click();

        bool gefunden = false;
        for (QLabel* l : dlg.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("no radio"))) {
                gefunden = true;
            }
        }
        QVERIFY2(gefunden,
                 "der Hinweis fehlt — dann plant man um 03:00 ins Leere");
    }
};

QTEST_MAIN(TstAufnahmeZeitplan)
#include "tst_aufnahme_zeitplan.moc"
