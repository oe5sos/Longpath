// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Kiwi-Wasserfaelle oben (2026-11-25): wachsen mit dem Fenster, zeigen die
// Frequenz, lassen sich schliessen und im Pegelbereich einstellen.
#include <QtTest>
#include <QSignalSpy>
#include <QToolButton>
#include <QLabel>
#include <QMenu>
#include <memory>

#include "core/AppSettings.h"
#include "core/KiwiSdrClient.h"
#include "gui/KiwiOwnSignalStrip.h"
#include "gui/KiwiWaterfallBackground.h"
#include "gui/StyleConstants.h"
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
    // 2026-11-28, Betreiber: hellere Gruende nur beim Kiwi-Wasserfall, nur
    // als zusaetzliche Wahl.
    void hellererGrundIstEineWahl()
    {
        KiwiBackground::set(QStringLiteral("Black"));
        RadioModel model;
        KiwiWaterfallPanel panel(&model);
        panel.setStripEnabled(QStringLiteral("a"), true, QStringLiteral("Eins"));
        panel.resize(500, 300);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel, 15000));
        auto* strip = panel.stripForTest(QStringLiteral("a"));
        strip->setRange(-110.0f, -40.0f);
        QVector<float> row = bins(-130.0f);                 // unter dem Bereich: Grund
        for (int i = 380; i < 420; ++i) { row[i] = -40.0f; } // volles Signal
        panel.pushRow(QStringLiteral("a"), row, 14185000.0, 14225000.0);

        // Bildpunkte: im Wasserfall neben dem Signal, im Signal, in der Kopfzeile.
        auto grab = [strip]() { return strip->grab().toImage(); };
        auto at = [](const QImage& img, double fx, int y) {
            const double dpr = img.devicePixelRatio();
            const int x = 9 + int(fx * (img.width() / dpr - 18));
            return img.pixelColor(int(x * dpr), int(y * dpr));
        };
        const QImage dark = grab();
        const QColor darkGround = at(dark, 0.25, 31);
        const QColor signal = at(dark, 400.0 / 512.0, 30);
        QVERIFY2(darkGround.lightness() < 20, qPrintable(darkGround.name()));

        KiwiBackground::set(QStringLiteral("Anthracite"));
        const QImage grey = grab();
        const QColor want(Style::kKiwiBgAnthracite);
        QCOMPARE(at(grey, 0.25, 31).name(), want.name());   // schon gemalte Zeile
        QCOMPARE(at(grey, 0.25, 5).name(), want.name());    // Kopfzeile
        QCOMPARE(at(grey, 400.0 / 512.0, 30).name(), signal.name()); // Signal bleibt

        KiwiBackground::set(QStringLiteral("Black"));
        QCOMPARE(at(grab(), 0.25, 31).name(), darkGround.name());
    }

    // Derselbe Grund im Streifen "Mein Signal", gewaehlt ueber sein Zahnrad.
    void meinSignalNimmtDenselbenGrund()
    {
        KiwiBackground::set(QStringLiteral("Black"));
        KiwiOwnSignalStrip strip;
        strip.resize(500, 200);
        // Unten rechts: Grund des Streifens, ausserhalb von Kopf und Bild.
        auto ground = [&strip]() {
            const QImage img = strip.grab().toImage();
            return img.pixelColor(img.width() - 3, img.height() - 3);
        };
        const QColor before = ground();
        std::unique_ptr<QMenu> menu(strip.buildOptionsMenu(nullptr));
        QAction* anthracite = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("Anthracite")) { anthracite = a; }
        }
        QVERIFY(anthracite);
        anthracite->trigger();
        QCOMPARE(KiwiBackground::current(), QStringLiteral("Anthracite"));
        QCOMPARE(ground().name(),
                 QColor(Style::kKiwiBgAnthracite).name());
        KiwiBackground::set(QStringLiteral("Black"));
        QCOMPARE(ground().name(), before.name());
    }

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
            panel.stripForTest(QStringLiteral("a"))->setRowIntervalMs(0);
            panel.stripForTest(QStringLiteral("b"))->setRowIntervalMs(0);
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

    // Betreiber: "laeuft viel zu schnell". Ein Kiwi schickt 10-14 Zeilen/s; der
    // Streifen nimmt in der Voreinstellung hoechstens 4/s.
    void dieGeschwindigkeitIstGebremst()
    {
        KiwiWaterfallStripWidget strip(QStringLiteral("a"), QStringLiteral("Eins"));
        QCOMPARE(strip.rowIntervalMs(), 250);
        for (int i = 0; i < 30; ++i) {
            strip.pushRow(bins(-100.0f), 14185000.0, 14225000.0);
        }
        QCOMPARE(strip.acceptedRowsForTest(), 1);    // der Rest kam im selben Augenblick
        QTest::qWait(300);
        strip.pushRow(bins(-100.0f), 14185000.0, 14225000.0);
        QCOMPARE(strip.acceptedRowsForTest(), 2);    // nach dem Abstand wieder eine

        // Gegenprobe: "jede Zeile" laesst alles durch.
        KiwiWaterfallStripWidget fast(QStringLiteral("b"), QStringLiteral("Zwei"));
        fast.setRowIntervalMs(0);
        for (int i = 0; i < 30; ++i) {
            fast.pushRow(bins(-100.0f), 14185000.0, 14225000.0);
        }
        QCOMPARE(fast.acceptedRowsForTest(), 30);
    }

    // "Als Option generell aendern": eine Aenderung gilt fuer alle Streifen und
    // bleibt gespeichert (neuer Streifen liest sie).
    void einstellungGiltFuerAlle()
    {
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/RowIntervalMs"));
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/RangeAuto"));
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/MinDbm"));
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/MaxDbm"));

        RadioModel model;
        KiwiWaterfallPanel panel(&model);
        panel.setStripEnabled(QStringLiteral("a"), true, QStringLiteral("Eins"));
        panel.setStripEnabled(QStringLiteral("b"), true, QStringLiteral("Zwei"));
        auto* a = panel.stripForTest(QStringLiteral("a"));
        auto* b = panel.stripForTest(QStringLiteral("b"));
        QCOMPARE(a->rowIntervalMs(), 250);

        // Was das Menue tut: setzen, speichern, melden.
        a->setRowIntervalMs(1000);
        a->setRange(-110.0f, -20.0f);
        a->saveOptions();
        emit a->optionsChanged();
        QCOMPARE(b->rowIntervalMs(), 1000);
        QCOMPARE(b->minDbm(), -110.0f);
        QCOMPARE(b->maxDbm(), -20.0f);

        // Ein spaeter geoeffneter Streifen (oder ein neuer Start) bekommt dasselbe.
        panel.setStripEnabled(QStringLiteral("c"), true, QStringLiteral("Drei"));
        QCOMPARE(panel.stripForTest(QStringLiteral("c"))->rowIntervalMs(), 1000);
        QCOMPARE(panel.stripForTest(QStringLiteral("c"))->maxDbm(), -20.0f);

        // aufraeumen, damit andere Laeufe die Voreinstellung sehen
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/RowIntervalMs"));
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/MinDbm"));
        AppSettings::instance().remove(QStringLiteral("KiwiWaterfall/MaxDbm"));
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

    // Betreiber 2026-11-25: oe3akb zeigte Bildzeilen und darueber "kein
    // Wasserfall-Kanal -- nur Ton" (Platz 4, wf_chans=3). Was ankommt, zaehlt.
    void bildzeilenWiderlegenKeinKanal()
    {
        KiwiSdrClient client;
        QSignalSpy spy(&client, &KiwiSdrClient::waterfallAvailabilityChanged);
        client.feedWaterfallTextForTest(QStringLiteral("MSG rx_chan=3"));
        client.feedWaterfallTextForTest(QStringLiteral("MSG wf_chans=3"));
        QVERIFY(!client.waterfallAvailable());
        QCOMPARE(spy.count(), 1);

        // Ein W/F-Rahmen mit erweitertem Kopf (Start 0, Zoom 8), 1024 Werte.
        QByteArray frame("W/F\0", 4);
        frame.append(QByteArray(4, '\0'));
        frame.append('\x08');
        frame.append(QByteArray(7, '\0'));
        frame.append(QByteArray(1024, '\x80'));
        client.feedWaterfallFrameForTest(frame);
        client.feedWaterfallFrameForTest(frame);
        QVERIFY2(!client.waterfallAvailable(), "zwei Zeilen reichen noch nicht");
        client.feedWaterfallFrameForTest(frame);
        QVERIFY(client.waterfallAvailable());
        QVERIFY(client.waterfallAvailabilityDetail().isEmpty());
        QCOMPARE(spy.count(), 2);
        // Eine spaetere Meldung des Servers dreht es nicht zurueck.
        client.feedWaterfallTextForTest(QStringLiteral("MSG wf_chans=3"));
        QVERIFY(client.waterfallAvailable());
    }

    // Betreiber 2026-11-27: "kiwi hat sich gerade aufgehaengt" -- ohne
    // Wasserfall-Kanal darf kein alter Verlauf stehen bleiben.
    void hinweisRaeumtDasAlteBildWeg()
    {
        KiwiWaterfallStripWidget strip(QStringLiteral("a"), QStringLiteral("Eins"));
        strip.pushRow(bins(-100.0f), 14185000.0, 14225000.0);
        QVERIFY(!strip.frequencyText().isEmpty());
        strip.setNotice(QStringLiteral("kein Kanal"));
        QVERIFY(strip.frequencyText().isEmpty());
        QCOMPARE(strip.spanHz(), 0.0);
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
