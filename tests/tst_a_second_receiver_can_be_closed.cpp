// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wer einen zweiten Empfaenger aufmacht, muss ihn auch wieder
// schliessen koennen.
//
// Der Betreiber am 2026-08-22: "40 meter hört sich an, als ich 2
// frequenzen gleichzeitig höre und ggf 2 bänder."
//
// Genau so war es: seine Kopfleiste zeigte A 7.144.100 und
// B 14.225.000 — zwei Empfaenger, 40 m und 20 m. Der Mischer nimmt
// JEDE Scheibe, die nicht stumm ist (AudioEngine.cpp:1162). Anlegen
// ging ueber "Add slice" (Strg+R), SCHLIESSEN ging nicht — die Luecke
// stand seit Phase 3F unbehoben im Quelltext.

#include <QtTest>
#include <QAction>
#include <QMenuBar>
#include <QMenu>

#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstASecondReceiverCanBeClosed : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};

    static QAction* findAction(MainWindow* mw, const QString& needle)
    {
        for (QAction* a : mw->findChildren<QAction*>()) {
            if (a->text().contains(needle, Qt::CaseInsensitive)) { return a; }
        }
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        // EIN Hauptfenster fuer alle Faelle, auf dem Haufen und nie
        // geloescht: ein echtes Fenster mit GPU-Spektrum abzureissen
        // ist ein eigenes Kapitel (tst_closing_takes_the_float_along)
        // und wuerde hier nur die Messung verdecken.
        m_mw = new MainWindow();
        m_mw->resize(1200, 800);
        m_mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mw));
        for (int i = 0; i < 8; ++i) { QCoreApplication::processEvents(); }
    }

    void theMenuOffersBothDirections()
    {
        MainWindow& mw = *m_mw;
        QVERIFY2(findAction(&mw, QStringLiteral("Add slice")),
                 "Kein Eintrag zum Anlegen");
        QVERIFY2(findAction(&mw, QStringLiteral("Remove active slice")),
                 "Anlegen geht, Schliessen nicht — genau die Lage, in "
                 "der der Betreiber zwei Baender gleichzeitig hoerte");
    }

    void closingActuallyRemovesIt()
    {
        MainWindow& mw = *m_mw;
        RadioModel* model = mw.findChild<RadioModel*>();
        if (!model) { QSKIP("Kein RadioModel"); }
        while (model->slices().size() < 2) {
            if (model->addSlice() < 0) { break; }
            for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        }
        if (model->slices().size() < 2) { QSKIP("Zweiter Empfaenger nicht anlegbar"); }

        const int before = model->slices().size();
        QAction* rm = findAction(&mw, QStringLiteral("Remove active slice"));
        QVERIFY(rm);
        rm->trigger();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        QVERIFY2(model->slices().size() == before - 1,
                 qPrintable(QStringLiteral(
                     "Der Empfaenger ist nicht weg: %1 -> %2")
                     .arg(before).arg(model->slices().size())));
    }

    void theLastOneStays()
    {
        // Eine App ohne Empfaenger ist kein Zustand, den man
        // versehentlich herstellen koennen soll.
        MainWindow& mw = *m_mw;
        RadioModel* model = mw.findChild<RadioModel*>();
        if (!model) { QSKIP("Kein RadioModel"); }
        if (model->slices().isEmpty()) { model->addSlice(); }
        while (model->slices().size() > 1) {
            model->removeSlice(model->slices().last()->sliceIndex());
            for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        }
        if (model->slices().size() != 1) { QSKIP("Kein Einzelzustand"); }

        QAction* rm = findAction(&mw, QStringLiteral("Remove active slice"));
        QVERIFY(rm);
        rm->trigger();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QCOMPARE(model->slices().size(), 1);
    }
};

QTEST_MAIN(TstASecondReceiverCanBeClosed)
#include "tst_a_second_receiver_can_be_closed.moc"
