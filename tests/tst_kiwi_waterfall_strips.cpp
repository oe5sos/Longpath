// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Kiwi-Wasserfaelle oben (2026-11-25): wachsen mit dem Fenster, zeigen die
// Frequenz, lassen sich schliessen und im Pegelbereich einstellen.
#include <QtTest>
#include <QToolButton>
#include <QLabel>

#include "gui/KiwiWaterfallPanel.h"
#include "gui/KiwiWaterfallStripWidget.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {
QVector<float> bins(float level, int n = 512)
{
    return QVector<float>(n, level);
}
}

class TstKiwiWaterfallStrips : public QObject
{
    Q_OBJECT
private slots:
    void streifenWachsenMitDemFenster()
    {
        RadioModel model;
        KiwiWaterfallPanel panel(&model);
        panel.setStripEnabled(QStringLiteral("a"), true, QStringLiteral("Eins"));
        panel.setStripEnabled(QStringLiteral("b"), true, QStringLiteral("Zwei"));
        panel.resize(500, 300);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel, 15000));
        const int klein = panel.stripForTest(QStringLiteral("a"))->height();
        panel.resize(500, 900);
        QTest::qWait(60);
        const int gross = panel.stripForTest(QStringLiteral("a"))->height();
        qInfo() << "Streifenhoehe klein" << klein << "gross" << gross;
        QVERIFY2(gross > klein + 100, "ein hoeheres Fenster muss hoehere Streifen geben");
    }

    void frequenzSteht_dabei()
    {
        RadioModel model;
        KiwiWaterfallPanel panel(&model);
        panel.setStripEnabled(QStringLiteral("a"), true, QStringLiteral("Eins"));
        auto* strip = panel.stripForTest(QStringLiteral("a"));
        QVERIFY(strip->frequencyText().isEmpty());     // noch keine Zeile
        panel.pushRow(QStringLiteral("a"), bins(-100.0f), 14185000.0, 14225000.0);
        QCOMPARE(strip->frequencyText(), QStringLiteral("14205.0 kHz"));
        auto* label = strip->findChild<QLabel*>(QStringLiteral("kiwiStripFrequency"));
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("14205.0 kHz"));

        const QString out = qEnvironmentVariable("LONGPATH_STRIP_PNG");
        if (!out.isEmpty()) {
            panel.setStripEnabled(QStringLiteral("b"), true, QStringLiteral("Zwei"));
            panel.resize(700, 420);
            panel.show();
            QVERIFY(QTest::qWaitForWindowExposed(&panel, 15000));
            for (int i = 0; i < 40; ++i) {
                QVector<float> b = bins(-115.0f + (i % 3));
                for (int k = 250; k < 262; ++k) { b[k] = -70.0f; }
                panel.pushRow(QStringLiteral("a"), b, 14185000.0, 14225000.0);
                panel.pushRow(QStringLiteral("b"), b, 21274000.0, 21314000.0);
            }
            QTest::qWait(60);
            panel.grab().save(out);
        }
    }

    void schliessenMeldetDenWunsch()
    {
        RadioModel model;
        KiwiWaterfallPanel panel(&model);
        panel.setStripEnabled(QStringLiteral("a"), true, QStringLiteral("Eins"));
        QSignalSpy spy(&panel, &KiwiWaterfallPanel::removeRequested);
        panel.stripForTest(QStringLiteral("a"))->closeButtonForTest()->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a"));
    }

    void pegelbereichLaesstSichEinstellen()
    {
        KiwiWaterfallStripWidget strip(QStringLiteral("a"), QStringLiteral("Eins"));
        QCOMPARE(strip.minDbm(), -130.0f);
        QCOMPARE(strip.maxDbm(), -40.0f);
        strip.setRange(-110.0f, -20.0f);
        QCOMPARE(strip.minDbm(), -110.0f);
        QVERIFY(!strip.autoRange());
        // Automatik folgt dem Rauschen: ein Rauschen von -120 dBm mit einer
        // Spitze von -60 dBm liegt dann im Bereich.
        strip.setAutoRange(true);
        QVector<float> b = bins(-120.0f);
        b[100] = -60.0f;
        strip.pushRow(b, 14185000.0, 14225000.0);
        QVERIFY(strip.minDbm() < -120.0f + 1.0f);
        QVERIFY(strip.maxDbm() >= -60.0f);
        // Zurueck auf fest: die Voreinstellung.
        strip.setAutoRange(false);
        QCOMPARE(strip.minDbm(), -130.0f);
    }
};

QTEST_MAIN(TstKiwiWaterfallStrips)
#include "tst_kiwi_waterfall_strips.moc"
