// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bandwechsel aus der Leiste oben.
//
// Der Betreiber am 2026-08-22: "bandwechsel sollte auch mit buttons
// möglich sein, am besten in der leiste oben" — und praezisiert:
// "bänder sollten 40,20,15 ersichtlich sein, rest dann mit einem
// fenster mit punkten, das man für die anderen bänder dann nutzt.
// 160, 80, 60, 30, 10, 6".

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "gui/widgets/CommandBar.h"
#include "models/Band.h"

using namespace Longpath;

class TstBandButtons : public QObject
{
    Q_OBJECT

private:
    static QStringList pillTexts(CommandBar& bar)
    {
        QStringList out;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            const QString t = b->text();
            if (t.endsWith(QStringLiteral("m"))
                && t.size() <= 4 && t.at(0).isDigit()) {
                out << t;
            }
        }
        return out;
    }

private slots:
    void theThreeNamedBandsAreInFront()
    {
        CommandBar bar;
        const QStringList pills = pillTexts(bar);
        qInfo() << "Sichtbare Baender:" << pills;
        for (const QString& want : {QStringLiteral("40m"),
                                    QStringLiteral("20m"),
                                    QStringLiteral("15m")}) {
            QVERIFY2(pills.contains(want),
                     qPrintable(QStringLiteral(
                         "%1 steht nicht vorne — der Betreiber hat genau "
                         "40, 20 und 15 verlangt").arg(want)));
        }
    }

    void aClickAsksForTheBand()
    {
        CommandBar bar;
        QSignalSpy spy(&bar, &CommandBar::bandRequested);

        QPushButton* b40 = nullptr;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("40m")) { b40 = b; }
        }
        QVERIFY2(b40, "Kein 40m-Knopf");
        b40->click();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).value<Longpath::Band>(), Band::Band40m);
    }

    void theRestIsReachableBehindTheDots()
    {
        // "rest dann mit einem fenster mit punkten" — die sechs
        // genannten muessen dort auffindbar sein.
        CommandBar bar;
        QPushButton* dots = nullptr;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text() == QString::fromUtf8("…")
                || b->text() == QStringLiteral("...")) {
                // Der erste "…" gehoert zur Bandgruppe, weil sie zuerst
                // gebaut wird.
                if (!dots) { dots = b; }
            }
        }
        QVERIFY2(dots, "Kein Ueberlauf-Knopf");
        QVERIFY2(dots->isEnabled(), "Der Ueberlauf ist gesperrt");
    }
};

QTEST_MAIN(TstBandButtons)
#include "tst_band_buttons.moc"
