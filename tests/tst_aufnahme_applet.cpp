// =================================================================
// tests/tst_aufnahme_applet.cpp  (Longpath)
// =================================================================
//
// Die Flaeche fuer Phase 3M. Was hier festgenagelt wird:
//
//   1. Das Applet IST da und haengt in der Spalte. Genau das fehlte
//      zwei Monate lang -- die vier Klassen dahinter waren gebaut und
//      geprueft, aber niemand konstruierte sie.
//   2. Es traegt EINE Kennung ("Rec"), dieselbe, unter der es in
//      m_appletsById und im Sichtbarkeitsregler steht. Wo das
//      auseinanderlief, gab jede Nachschlagung still nullptr zurueck
//      (siehe scripts/check-applet-ids.py).
//   3. Ohne Radio sind die Knoepfe aus und die Uhren zeigen einen
//      Strich, keine Null -- Hausstil Regel 7: „Unbekannt ist ein
//      Strich. `——` statt `0.00` -- eine Null ist eine Aussage."
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include <QApplication>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QDir>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QtTest>

#include "FensterFrist.h"
#include "gui/MainWindow.h"
#include "gui/RecordingScheduleWindow.h"
#include "gui/applets/RecordingApplet.h"
#include "models/RadioModel.h"
#include "models/RecordingModel.h"

using namespace Longpath;

class TstAufnahmeApplet : public QObject
{
    Q_OBJECT

private slots:
    void dasAppletHaengtInDerSpalte()
    {
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));

        RecordingApplet* a = mw.findChild<RecordingApplet*>();
        QVERIFY2(a != nullptr,
                 "kein RecordingApplet im Fenster — die Flaeche fehlt "
                 "wieder, genau wie zwei Monate lang");

        // EINE Schreibweise. Weicht sie von der Panelkennung ab, findet
        // der Sichtbarkeitsregler das Applet nicht mehr.
        QCOMPARE(a->appletId(), QStringLiteral("Rec"));
        QCOMPARE(a->appletTitle(), QStringLiteral("Recording"));
    }

    void ohneRadioIstAllesAusUndZeigtEinenStrich()
    {
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));

        RecordingApplet* a = mw.findChild<RecordingApplet*>();
        QVERIFY(a != nullptr);

        RadioModel* model = mw.findChild<RadioModel*>();
        if (!model) { QSKIP("kein RadioModel"); }
        if (model->recording().ready()) {
            QSKIP("das Modell meldet sich bereit — nicht der Fall, "
                  "den dieser Lauf prueft");
        }

        const QList<QPushButton*> knoepfe = a->findChildren<QPushButton*>();
        QVERIFY2(knoepfe.size() >= 2,
                 qPrintable(QStringLiteral("nur %1 Knopf/Knoepfe")
                                .arg(knoepfe.size())));
        for (QPushButton* b : knoepfe) {
            QVERIFY2(!b->isEnabled(),
                     qPrintable(QStringLiteral("Knopf '%1' ist an, obwohl "
                                               "kein Radio da ist")
                                    .arg(b->text())));
        }

        // Regel 7 des Hausstils. Eine Null waere eine Aussage ueber
        // eine Aufnahme, die es nicht gibt.
        int striche = 0;
        for (QLabel* l : a->findChildren<QLabel*>()) {
            if (l->text() == QStringLiteral("——")) { ++striche; }
            QVERIFY2(l->text() != QStringLiteral("00:00"),
                     "eine Uhr zeigt 00:00 statt eines Strichs");
        }
        QCOMPARE(striche, 2);   // Ton und I/Q

        // Und sie muessen LESBAR sein. Beim ersten Bau waren diese
        // Uhren 2 px breit: die Grundschrift der Applets ist ueber
        // setPixelSize gesetzt, also liefert pointSizeF() == -1, und
        // „-1 + 2" ergibt eine Ein-Punkt-Schrift. Der Text stimmte,
        // das Bild war leer -- ein Lauf, der nur den Text prueft,
        // haette das durchgelassen. (HAUSSTIL.md: „Die Falle, die
        // einen Monat Typografie unsichtbar gemacht hat".)
        for (QLabel* l : a->findChildren<QLabel*>()) {
            if (l->text() != QStringLiteral("——")) { continue; }
            QVERIFY2(l->width() >= 20,
                     qPrintable(QStringLiteral("Uhr nur %1 px breit")
                                    .arg(l->width())));
            const QFontMetrics fm(l->font());
            const int breite =
                fm.horizontalAdvance(QStringLiteral("00:00"));
            QVERIFY2(breite >= 20,
                     qPrintable(QStringLiteral("Ziffern messen nur %1 px "
                                               "- Schriftgroesse verloren")
                                    .arg(breite)));
        }
    }

    void einBlattFuerDenBetreiber()
    {
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));

        RecordingApplet* a = mw.findChild<RecordingApplet*>();
        QVERIFY(a != nullptr);

        const QString dir = qEnvironmentVariable("LONGPATH_BLATT_DIR");
        if (dir.isEmpty()) {
            QSKIP("LONGPATH_BLATT_DIR nicht gesetzt — kein Blatt "
                  "gewuenscht");
        }
        QDir().mkpath(dir);
        const QImage bild = a->grab().toImage();
        QVERIFY(!bild.isNull());
        QVERIFY(bild.save(dir + QStringLiteral("/aufnahme-applet.png")));
        QVERIFY(mw.grab().toImage().save(
            dir + QStringLiteral("/aufnahme-im-fenster.png")));

        // Und das Zeitplan-Fenster AUS DER ECHTEN APP heraus. Allein
        // gebaut traegt es Qts Standardanstrich, weil ohne MainWindow
        // kein Hausstil geladen ist -- so wuerde der Betreiber es nie
        // sehen.
        a->openExtendedSettings();
        QWidget* plan = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->inherits("Longpath::RecordingScheduleWindow")) {
                plan = w; break;
            }
        }
        QVERIFY2(plan != nullptr,
                 "das Zahnrad hat kein Zeitplan-Fenster geoeffnet");
        plan->resize(440, 300);
        QVERIFY(QTest::qWaitForWindowExposed(plan, kFensterFrist));

        // Zwei Eintraege ueber die Bedienteile des echten Fensters --
        // das prueft zugleich, dass sich die Liste auffrischt, und
        // macht das Blatt aussagekraeftig.
        auto* fenster = qobject_cast<RecordingScheduleWindow*>(plan);
        QVERIFY(fenster != nullptr);
        fenster->startFeld()->setDateTime(
            QDateTime(QDate(2026, 10, 22), QTime(6, 0), QTimeZone::UTC));
        fenster->minutenFeld()->setValue(30);
        fenster->artFeld()->setCurrentIndex(0);
        fenster->anlegenKnopf()->click();

        fenster->startFeld()->setDateTime(
            QDateTime(QDate(2026, 10, 23), QTime(18, 0), QTimeZone::UTC));
        fenster->minutenFeld()->setValue(60);
        fenster->artFeld()->setCurrentIndex(1);
        fenster->anlegenKnopf()->click();

        QCOMPARE(fenster->tabelle()->rowCount(), 2);
        if (auto* z = qobject_cast<QPushButton*>(
                fenster->tabelle()->cellWidget(1, 3))) {
            z->click();   // der zweite still, damit beide Zustaende da sind
        }
        QTest::qWait(100);
        QVERIFY(plan->grab().toImage().save(
            dir + QStringLiteral("/zeitplan-im-hausstil.png")));
        plan->close();
    }

    void dasZahnradOeffnetDenZeitplan()
    {
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));

        RecordingApplet* a = mw.findChild<RecordingApplet*>();
        QVERIFY(a != nullptr);

        // Vorher gibt es keins.
        int vorher = 0;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->inherits("Longpath::RecordingScheduleWindow")) { ++vorher; }
        }
        QCOMPARE(vorher, 0);

        a->openExtendedSettings();

        int nachher = 0;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->inherits("Longpath::RecordingScheduleWindow")) { ++nachher; }
        }
        QCOMPARE(nachher, 1);

        // Zweimal druecken gibt kein zweites Fenster: zwei Ansichten
        // auf dieselbe Liste laufen auseinander.
        a->openExtendedSettings();
        int nochmal = 0;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->inherits("Longpath::RecordingScheduleWindow")) { ++nochmal; }
        }
        QCOMPARE(nochmal, 1);
    }
};

QTEST_MAIN(TstAufnahmeApplet)
#include "tst_aufnahme_applet.moc"
