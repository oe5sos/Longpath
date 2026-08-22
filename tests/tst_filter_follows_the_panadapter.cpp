// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Bandfilter steht dort, wo der Panadapter steht.
//
// Der Betreiber am 2026-08-22, nach einer Vorfuehrung von Zeus Link:
// "und der filter sollte natürlich genau dort sein, wo auch ich
// panadapter bin. er sollte mir ja auch das signal zeigen" und
// "genau wo ich im panadapter bin soll auch der bandwith filter sein."
//
// Auf seinem Bild stand der Bandfilter auf 14,22 MHz, waehrend das
// Geraet auf 7,1156 MHz empfing.

#include <QtTest>

#include "gui/applets/BandwidthFilterApplet.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFilterFollowsThePanadapter : public QObject
{
    Q_OBJECT

private slots:
    void theAxisFollowsTheVfo()
    {
        RadioModel model;
        BandwidthFilterApplet applet(&model);
        applet.resize(600, 260);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        // Ohne Geraet legt das Modell keine Scheibe an — hier eine
        // anlegen, damit der Bandfilter etwas zu zeigen hat.
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        const QList<SliceModel*> slices = model.slices();
        QVERIFY2(!slices.isEmpty(), "Keine Scheibe im Modell");
        SliceModel* s = slices.first();

        auto paneFreq = [&]() -> double {
            const auto panes = applet.findChildren<BandwidthFilterPane*>();
            return panes.isEmpty() ? -1.0 : panes.first()->vfoFrequency();
        };

        s->setFrequency(7'115'600.0);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        // Ueber das SIGNAL, nicht ueber ein erzwungenes syncFromModel:
        // genau darin lag der Fehler. Ein erzwungenes Auffrischen
        // lieferte am 2026-08-22 sofort den richtigen Wert, das Signal
        // kam nie an — die Verbindung war nie geknuepft worden.
        QCOMPARE(paneFreq(), 7'115'600.0);

        s->setFrequency(14'222'000.0);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(qFuzzyCompare(paneFreq(), 14'222'000.0),
                 qPrintable(QStringLiteral(
                     "Der Bandfilter steht auf %1 Hz, das Geraet auf "
                     "14222000 — genau das Bild vom 2026-08-22")
                     .arg(paneFreq())));
    }
    void theSignalShowsUpInThePane()
    {
        // "er sollte mir ja auch das signal zeigen" (Betreiber,
        // 2026-08-22, nach einer Vorfuehrung von Zeus Link).
        //
        // Gemessen wird am BILD, nicht an einer Fahne: gezaehlt werden
        // Punkte, die weder Grund noch Raster sind — ohne Kurve gibt
        // es sie nicht.
        RadioModel model;
        BandwidthFilterApplet applet(&model);
        applet.resize(600, 260);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        model.slices().first()->setFrequency(7'100'000.0);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto litPixels = [&]() {
            QImage img(applet.size(), QImage::Format_ARGB32);
            img.fill(Qt::black);
            applet.render(&img);
            int lit = 0;
            for (int y = 0; y < img.height(); y += 2) {
                for (int x = 0; x < img.width(); x += 2) {
                    const QColor c = img.pixelColor(x, y);
                    // Alles, was heller ist als Grund und Raster.
                    // (Die erste Fassung suchte GRUEN — die Kurve ist
                    // aber grau, kTextScale. Der Zaehler zaehlte nie
                    // etwas und haette jede Behebung fuer wirkungslos
                    // erklaert.)
                    const int lum = (c.red() + c.green() + c.blue()) / 3;
                    if (lum > 45) { ++lit; }
                }
            }
            return lit;
        };

        const int before = litPixels();

        // Eine kuenstliche Quelle: ein Buckel in der Mitte.
        applet.setSpectrumSource(
            [](int, double, double, int points) -> QVector<float> {
            QVector<float> v(points, -120.0f);
            for (int i = 0; i < points; ++i) {
                const double d = std::abs(i - points / 2.0);
                if (d < points / 8.0) { v[i] = -60.0f; }
            }
            return v;
        });
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(140);          // der Zeitgeber laeuft mit 20 Hz
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        const int after = litPixels();
        qInfo() << "Kurvenpunkte vorher:" << before << "nachher:" << after;
        QVERIFY2(after > before + 40,
                 qPrintable(QStringLiteral(
                     "Kein Signal im Bandfilter: %1 -> %2 Punkte")
                     .arg(before).arg(after)));
    }
};

QTEST_MAIN(TstFilterFollowsThePanadapter)
#include "tst_filter_follows_the_panadapter.moc"
