// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Frequenz-Widget stimmt wirklich ab — mit dem Rad auf einer
// Stelle und mit der Tastatur.
//
// Der Betreiber am 2026-08-22: "für frequenz bitte auch ein eigenes
// widget bauen". Es GIBT eines (FrequencyApplet mit
// FrequencyInstrument), samt Ziffernrad und Eingabefeld — wie das
// grosse Frequenzfeld bei OpenHPSDR Zeus ("CLICK TO TYPE · WHEEL ON A
// DIGIT").
//
// Heute war allerdings mehrfach etwas verdrahtet und trotzdem tot:
// die Zoomknoepfe sandten ins Leere, die Pfeiltasten kamen nie an,
// der Panadapter erfuhr nie von Frequenzaenderungen. Bevor ich dem
// Betreiber sage "das gibt es schon", wird es gemessen.

#include <QtTest>
#include <QLabel>
#include <QWheelEvent>

#include "gui/applets/FrequencyApplet.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFrequencyWidgetTunes : public QObject
{
    Q_OBJECT

private slots:
    void theWheelOnADigitTunesThatDigit()
    {
        RadioModel model;
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        SliceModel& slice = *model.slices().first();
        slice.setFrequency(7'100'000.0);

        FrequencyApplet applet(&model);
        applet.bindSlice(&slice);
        applet.resize(360, 90);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto* inst = applet.findChild<FrequencyInstrument*>();
        QVERIFY2(inst, "Kein Frequenz-Instrument im Applet");

        // Die Ziffernschilder liegen als QLabel im Instrument. Gesucht
        // wird das, auf dem eine Ziffer steht — das Rad darauf soll
        // GENAU dessen Stelle bewegen.
        QList<QLabel*> digits;
        for (QLabel* l : inst->findChildren<QLabel*>()) {
            const QString t = l->text();
            if (t.size() == 1 && t.at(0).isDigit()) { digits.append(l); }
        }
        QVERIFY2(!digits.isEmpty(), "Keine Ziffern gefunden");

        const double before = slice.frequency();
        QLabel* last = digits.last();          // kleinste Stelle
        QWheelEvent up(QPointF(last->width() / 2.0, last->height() / 2.0),
                       last->mapToGlobal(QPoint(1, 1)),
                       QPoint(0, 0), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier,
                       Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(last, &up);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        QVERIFY2(!qFuzzyCompare(slice.frequency(), before),
                 qPrintable(QStringLiteral(
                     "Das Rad auf der Ziffer bewegt nichts: %1 -> %2")
                     .arg(before).arg(slice.frequency())));
        // Genau EINE Stelle, nicht irgendetwas: der Sprung muss klein
        // sein.
        const double step = qAbs(slice.frequency() - before);
        QVERIFY2(step <= 1000.0,
                 qPrintable(QStringLiteral(
                     "Ein Radschritt auf der kleinsten Stelle springt "
                     "um %1 Hz").arg(step)));
    }

    void theWidgetFollowsTheModel()
    {
        // Die andere Richtung, und heute die haeufigere Fehlerquelle:
        // aendert sich die Frequenz anderswo (Klick im Panadapter,
        // Pfeiltaste, CAT), muss das Widget mitgehen.
        RadioModel model;
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        SliceModel& slice = *model.slices().first();
        slice.setFrequency(7'100'000.0);

        FrequencyApplet applet(&model);
        applet.bindSlice(&slice);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto* inst = applet.findChild<FrequencyInstrument*>();
        QVERIFY(inst);

        slice.setFrequency(14'225'000.0);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        QString shown;
        for (QLabel* l : inst->findChildren<QLabel*>()) {
            const QString t = l->text();
            if (t.size() == 1 && (t.at(0).isDigit() || t == QStringLiteral("."))) {
                shown += t;
            }
        }
        QVERIFY2(shown.contains(QStringLiteral("14")),
                 qPrintable(QStringLiteral(
                     "Das Widget zeigt %1, das Modell steht auf "
                     "14.225.000").arg(shown)));
    }
};

QTEST_MAIN(TstFrequencyWidgetTunes)
#include "tst_frequency_widget_tunes.moc"
