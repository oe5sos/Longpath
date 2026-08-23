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
#include "gui/StyleConstants.h"
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
    void theSpanFollowsTheChosenWidth()
    {
        // Der Betreiber am 2026-08-22: "der bandfilter sollte auch
        // genau den bereich zeigen, den man ausgewählt hat, siehe
        // zeus" — und dazu: "kann auch danach ein größerer bereich
        // sein".
        //
        // Zeus zeigt bei 2,9 kHz Filter rund 10 kHz Fenster. Also
        // folgt die Spanne der Wahl, mit Umgebung drumherum.
        RadioModel model;
        BandwidthFilterApplet applet(&model);
        applet.resize(600, 260);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        SliceModel* s = model.slices().first();

        auto span = [&]() {
            const auto panes = applet.findChildren<BandwidthFilterPane*>();
            return panes.isEmpty() ? -1 : panes.first()->spanHz();
        };

        s->setFilterByHand(-2900, -100);      // 2,8 kHz
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        const int narrow = span();

        s->setFilterByHand(-6000, -100);      // 5,9 kHz
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        const int wide = span();

        qInfo() << "Spanne bei 2,8 kHz:" << narrow << "bei 5,9 kHz:" << wide;
        QVERIFY2(wide > narrow,
                 qPrintable(QStringLiteral(
                     "Die Spanne folgt der Wahl nicht: %1 -> %2")
                     .arg(narrow).arg(wide)));
        // Der Durchlass soll ein gutes Drittel fuellen, nicht die
        // Briefmarke im Riesenfenster sein.
        QVERIFY2(narrow < 2800 * 5,
                 qPrintable(QStringLiteral(
                     "Bei 2,8 kHz Filter zeigt der Bandfilter %1 Hz — "
                     "viel zu weit").arg(narrow)));
    }

    void aNewCarrierAppearsAtOnceButNoiseStaysCalm()
    {
        // Der Betreiber am 2026-08-23, nach der ersten Glaettung: "die
        // form bleibt auch immer leicht zu sehen, zeitversetzt" — ein
        // blasses Nachbild, das der Kurve hinterherlaeuft. Genau das
        // macht eine SYMMETRISCHE Glaettung: sie verzoegert das
        // Steigen ebenso wie das Fallen.
        //
        // Richtig ist ungleich: steigen sofort, fallen gemaechlich.
        // Beides wird hier gemessen — die Behauptung "jetzt ist es
        // schnell" ist sonst nur eine Behauptung.
        BandwidthFilterPane pane;
        pane.setSpan(10000);
        pane.setFilter(-2900, -100);
        pane.resize(600, 200);

        const int n = 200;
        QVector<float> quiet(n, -120.0f);
        for (int f = 0; f < 40; ++f) { pane.setTrace(quiet); }
        const float restingMid = pane.traceForTest().at(n / 2);
        QVERIFY2(qAbs(restingMid + 120.0f) < 0.5f,
                 "Die Ruhelage stimmt nicht");

        // Ein Traeger geht auf — EIN Bild.
        QVector<float> carrier = quiet;
        carrier[n / 2] = -60.0f;
        pane.setTrace(carrier);
        const float afterOne = pane.traceForTest().at(n / 2);
        qInfo() << "Nach EINEM Bild:" << afterOne << "dBm (Ziel -60)";
        QVERIFY2(afterOne > -80.0f,
                 qPrintable(QStringLiteral(
                     "Der Traeger steht nach einem Bild erst bei %1 dBm "
                     "— das ist das Nachbild, das der Betreiber sieht")
                     .arg(afterOne)));

        // Und wieder weg: das darf gemaechlich gehen, aber nicht
        // ewig.
        for (int f = 0; f < 25; ++f) { pane.setTrace(quiet); }
        const float afterFall = pane.traceForTest().at(n / 2);
        qInfo() << "Nach 25 Bildern Stille:" << afterFall;
        QVERIFY2(afterFall < -115.0f,
                 qPrintable(QStringLiteral(
                     "Nach 25 Bildern steht der Traeger noch bei %1 dBm")
                     .arg(afterFall)));
    }

    void theNoiseFloorKeepsItsTexture()
    {
        // Der Betreiber am 2026-08-23: "wo kein signal ist, ist alles
        // flach" — und unmittelbar danach: "so will ich es haben."
        //
        // ICH HATTE DEN ERSTEN SATZ ALS BESCHWERDE GELESEN und war
        // dabei, die Glaettung umzustellen. Die Messung kam zuerst und
        // hat die Aenderung verhindert: 1,0 dB Streuung — ruhig, aber
        // nicht tot. Genau der Zustand, den er wollte.
        //
        // Dieser Fall bewacht deshalb NICHT "viel Struktur", sondern
        // nur den Ausartungsfall: eine Kurve, die exakt zur Geraden
        // wird, zeigt ein totes Geraet statt eines leisen Bandes.
        // "Schnell hoch, langsam runter" zieht den Rauschflur auf
        // seine Spitzenwerte; ohne Untergrenze koennte daraus eine
        // Linie werden.
        BandwidthFilterPane pane;
        pane.setSpan(10000);
        pane.setFilter(-2900, -100);
        pane.resize(600, 200);

        const int n = 240;
        for (int f = 0; f < 60; ++f) {
            QVector<float> v(n);
            for (int i = 0; i < n; ++i) {
                const double r = std::sin(i * 12.9898 + f * 4.1414) * 43758.5453;
                v[i] = -120.0f + static_cast<float>((r - std::floor(r)) * 8.0);
            }
            pane.setTrace(v);
        }

        const QVector<float>& t = pane.traceForTest();
        double mean = 0.0;
        for (float v : t) { mean += v; }
        mean /= t.size();
        double var = 0.0;
        for (float v : t) { var += (v - mean) * (v - mean); }
        const double sd = std::sqrt(var / t.size());
        qInfo() << "Streuung des Rauschflurs:" << sd << "dB";

        QVERIFY2(sd > 0.8,
                 qPrintable(QStringLiteral(
                     "Der Rauschflur ist mit %1 dB Streuung praktisch "
                     "flach — genau der Befund des Betreibers")
                     .arg(sd)));
    }

    void withoutSignalTheLineSitsAtTheBottom()
    {
        // Der Betreiber am 2026-08-23, mit drei Bildern von OpenHPSDR
        // Zeus: "wo kein signal ist, ist die linie am boden" und
        // "wenn kein signal ist, linie bei 0, auch bei den
        // sprechpausen".
        //
        // Vorher dehnte die Flaeche IMMER auf Minimum bis Maximum: ist
        // nur Rauschen da, wurde dessen Zappeln von ein paar Dezibel
        // auf die volle Hoehe gezogen. In jeder Sprechpause sprang die
        // Kurve auf, und die Flaeche sah belebt aus, wo nichts war.
        //
        // Gemessen wird AM BILD: wie weit oben steht der hoechste
        // Punkt der Kurve, wenn nur Rauschen anliegt.
        BandwidthFilterPane pane;
        pane.setLabel(QStringLiteral("RX1"));
        pane.setVfoFrequency(7'100'000.0);
        pane.setHasFrequency(true);
        pane.setSpan(10000);
        pane.setFilter(-2900, -100);
        pane.resize(600, 200);

        const int n = 240;
        for (int f = 0; f < 40; ++f) {
            QVector<float> v(n);
            for (int i = 0; i < n; ++i) {
                const double r = std::sin(i * 12.9898 + f * 4.1414) * 43758.5453;
                v[i] = -120.0f + static_cast<float>((r - std::floor(r)) * 6.0);
            }
            pane.setTrace(v);
        }

        QImage img(pane.size(), QImage::Format_ARGB32);
        img.fill(QColor(Style::kAppBg));
        pane.render(&img);

        // Der Kurvenzug ist warm getoent; der Rest ist Grau und Blau.
        int topMost = img.height();
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 4; x < img.width() - 60; x += 2) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > c.blue() + 24 && c.red() > 90) {
                    topMost = qMin(topMost, y);
                    break;
                }
            }
            if (topMost < img.height()) { break; }
        }
        const double frac = double(topMost) / img.height();
        qInfo() << "Hoechster Kurvenpunkt bei" << (frac * 100.0) << "% Hoehe";

        QVERIFY2(frac > 0.62,
                 qPrintable(QStringLiteral(
                     "Ohne Signal steigt die Kurve bis auf %1 %% der "
                     "Hoehe — sie gehoert an den Boden")
                     .arg(frac * 100.0, 0, 'f', 1)));
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
