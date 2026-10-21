// =================================================================
// tests/tst_zeitplan_fenster.cpp  (Longpath)
// =================================================================
//
// Das Fenster, in dem Zeitplan-Eintraege angelegt werden. Bis zum
// 2026-10-21 gab es keinen Weg dorthin: `RecordingScheduler::add()`
// wurde ausserhalb der Prueflaeufe NIRGENDS gerufen, das Applet zeigte
// „n entries", und niemand konnte einen anlegen.
//
// Was hier festgenagelt wird, ist genau dieser Weg -- und dass der
// Eintrag auch wirklich im Zeitplan ankommt, nicht nur in einer
// Tabellenzeile.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include <QDir>
#include <QFontMetrics>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QtTest>

#include "gui/RecordingScheduleWindow.h"
#include "models/RadioModel.h"
#include "models/RecordingModel.h"

using namespace Longpath;

class TstZeitplanFenster : public QObject
{
    Q_OBJECT

private slots:
    void leerZeigtKeineZeile()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        QVERIFY(w.tabelle() != nullptr);
        QCOMPARE(w.tabelle()->rowCount(), 0);
        QCOMPARE(rec.scheduleCount(), 0);
    }

    void einDruckAufAddLegtWirklichAn()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);

        w.startFeld()->setDateTime(
            QDateTime(QDate(2026, 10, 22), QTime(6, 0), QTimeZone::UTC));
        w.minutenFeld()->setValue(30);
        w.artFeld()->setCurrentIndex(0);          // Audio

        QCOMPARE(rec.scheduleCount(), 0);
        w.anlegenKnopf()->click();

        // DAS ist der Punkt: nicht nur eine Zeile in der Tabelle,
        // sondern ein Eintrag IM Zeitplan. Ohne den Weg hierher war
        // add() ausserhalb der Prueflaeufe ohne Aufrufer.
        QCOMPARE(rec.scheduleCount(), 1);
        QCOMPARE(w.tabelle()->rowCount(), 1);

        const std::vector<ScheduledRecording> l = rec.schedule().entries();
        QCOMPARE(l.size(), size_t(1));
        QCOMPARE(l[0].durationMinutes, 30);
        QCOMPARE(l[0].kind, ScheduledRecordingKind::Audio);
        QVERIFY(l[0].enabled);
        // Leerer Pfad heisst „waehle selbst einen" -- den Namen baut
        // RecordingModel, damit er genauso aussieht wie beim Druck auf
        // den Knopf.
        QVERIFY(l[0].path.isEmpty());
    }

    void dieArtKommtAn()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        w.artFeld()->setCurrentIndex(1);          // I/Q
        w.anlegenKnopf()->click();

        const std::vector<ScheduledRecording> l = rec.schedule().entries();
        QCOMPARE(l.size(), size_t(1));
        QCOMPARE(l[0].kind, ScheduledRecordingKind::Iq);
    }

    void nullMinutenStehenAlsStrich()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        w.minutenFeld()->setValue(0);             // kein automatisches Ende
        w.anlegenKnopf()->click();

        QCOMPARE(w.tabelle()->rowCount(), 1);
        // Hausstil Regel 7: eine Null waere hier eine Aussage („null
        // Minuten lang"), gemeint ist „kein Ende".
        QCOMPARE(w.tabelle()->item(0, 1)->text(), QStringLiteral("—"));
        QCOMPARE(rec.schedule().entries()[0].durationMinutes, 0);
    }

    void dieVorgabeLiegtInDerZukunft()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        // Eine Startzeit in der Vergangenheit wuerde beim naechsten
        // Takt sofort ausloesen -- die Vorgabe darf das nicht tun.
        QVERIFY2(w.startFeld()->dateTime().toUTC()
                     > QDateTime::currentDateTimeUtc(),
                 qPrintable(w.startFeld()->dateTime().toString()));
    }

    void wegDrueckenNimmtDenEintragWirklichWeg()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        w.anlegenKnopf()->click();
        w.anlegenKnopf()->click();
        QCOMPARE(rec.scheduleCount(), 2);

        // Spalte 4 traegt das Kreuz.
        auto* weg = qobject_cast<QPushButton*>(w.tabelle()->cellWidget(0, 4));
        QVERIFY2(weg != nullptr, "kein Loeschkreuz in der Zeile");
        weg->click();

        QCOMPARE(rec.scheduleCount(), 1);
        QCOMPARE(w.tabelle()->rowCount(), 1);
    }

    void einBlattFuerDenBetreiber()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_BLATT_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_BLATT_DIR nicht gesetzt"); }

        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        w.resize(440, 300);

        w.startFeld()->setDateTime(
            QDateTime(QDate(2026, 10, 22), QTime(6, 0), QTimeZone::UTC));
        w.minutenFeld()->setValue(30);
        w.artFeld()->setCurrentIndex(0);
        w.anlegenKnopf()->click();

        w.startFeld()->setDateTime(
            QDateTime(QDate(2026, 10, 23), QTime(18, 0), QTimeZone::UTC));
        w.minutenFeld()->setValue(60);
        w.artFeld()->setCurrentIndex(1);
        w.anlegenKnopf()->click();

        // Den zweiten stilllegen, damit das Blatt beide Zustaende zeigt.
        if (auto* z = qobject_cast<QPushButton*>(
                w.tabelle()->cellWidget(1, 3))) {
            z->click();
        }

        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 20000));
        QDir().mkpath(dir);
        QVERIFY(w.grab().toImage().save(
            dir + QStringLiteral("/zeitplan-fenster.png")));
    }

    void dasLoeschkreuzTraegtSeinenEigenenAnstrich()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        w.anlegenKnopf()->click();

        auto* weg = qobject_cast<QPushButton*>(w.tabelle()->cellWidget(0, 4));
        QVERIFY(weg != nullptr);
        QVERIFY2(!weg->text().isEmpty(), "das Kreuz hat keinen Text");

        // Beim ersten Bau stand im Blatt an dieser Stelle ein fast
        // leeres Kaestchen. Meine erste Erklaerung -- „die Polsterung
        // frisst die Spalte" -- war FALSCH: gemessen wollte der Knopf
        // mit der allgemeinen Knopfmarke 30 px, die Spalte hat 34, es
        // wurde also nichts weggeschnitten. Das Kreuz war nur blass
        // (kTextScale auf Knopfgrund, 13 px).
        //
        // Die Behebung ist eine eigene Marke: durchsichtiger Grund,
        // keine Polsterung, 15 px. Genau DAS nagelt dieser Lauf fest --
        // ohne sie springt die Wunschbreite von 10 auf 30 px.
        //
        // Was er NICHT belegt: dass das Kreuz auf dem Schirm gut
        // aussieht. Das steht im Blatt, nicht in einer Zahl.
        QVERIFY2(weg->sizeHint().width() <= 16,
                 qPrintable(QStringLiteral("der Knopf will %1 px — die "
                                           "eigene Marke greift nicht")
                                .arg(weg->sizeHint().width())));
    }

    void derSchalterLegtDenEintragStill()
    {
        RecordingModel rec;
        RecordingScheduleWindow w(&rec);
        w.anlegenKnopf()->click();

        auto* zustand =
            qobject_cast<QPushButton*>(w.tabelle()->cellWidget(0, 3));
        QVERIFY2(zustand != nullptr, "kein Zustandsschalter in der Zeile");
        QVERIFY(zustand->isChecked());
        QCOMPARE(zustand->text(), QStringLiteral("ARMED"));

        zustand->click();
        QVERIFY(!rec.schedule().entries()[0].enabled);
        // Der Eintrag bleibt stehen -- abgeschaltet ist nicht geloescht.
        QCOMPARE(rec.scheduleCount(), 1);
    }
};

QTEST_MAIN(TstZeitplanFenster)
#include "tst_zeitplan_fenster.moc"
