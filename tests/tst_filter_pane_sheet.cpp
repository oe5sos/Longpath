// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: malt die Bandfilter-Flaeche mit einem
// kuenstlichen Signal, damit man den Entwurf beurteilen kann, ohne
// ein Funkgeraet anzuschliessen.
//
// Anlass, 2026-08-22: "dieser sollte grafisch auch überarbeitet
// werden."

#include <QtTest>
#include <QPainter>
#include <QImage>
#include <cmath>

#include "gui/widgets/BandwidthFilterPane.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstFilterPaneSheet : public QObject
{
    Q_OBJECT

private:
    // Ein Bild aus dem laufenden Empfang: Signal plus RAUSCHEN, das
    // sich von Bild zu Bild aendert. Genau daran entscheidet sich, ob
    // die Kurve ruhig ist — ein stehendes Muster wuerde jede Glaettung
    // gut aussehen lassen.
    static QVector<float> fakeTrace(int n, int spanHz, int frame)
    {
        QVector<float> v(n, -118.0f);
        for (int i = 0; i < n; ++i) {
            const double hz = -spanHz / 2.0 + spanHz * double(i) / (n - 1);
            const double a = std::exp(-std::pow((hz + 1500.0) / 900.0, 2.0));
            const double b = std::exp(-std::pow((hz - 3200.0) / 220.0, 2.0));
            // Pseudozufall ohne Zufallsquelle: der Pruefstand muss
            // wiederholbar bleiben.
            const double n1 = std::sin(i * 12.9898 + frame * 4.1414) * 43758.5453;
            const double noise = (n1 - std::floor(n1)) * 7.0 - 3.5;
            // Lebensecht: ein Sprechsignal rund 30 dB ueber dem Flur, ein
            // schmaler Stoerer daneben. 46 dB waren fuer den neuen
            // 40-dB-Massstab zu stark — das Prueffbild lief oben an und
            // taugte nicht mehr zur Beurteilung.
            v[i] = static_cast<float>(-118.0 + 30.0 * a + 26.0 * b + noise);
        }
        return v;
    }

private slots:
    void drawTheSheet()
    {
        BandwidthFilterPane pane;
        pane.setLabel(QStringLiteral("RX1"));
        pane.setAccent(QColor(Style::kBlueBg));
        pane.setVfoFrequency(7'115'600.0);
        pane.setHasFrequency(true);
        pane.setSpan(9520);
        pane.setFilter(-2900, -100);
        // Dreissig Bilder, wie im Betrieb — erst dann wirkt die
        // zeitliche Glaettung.
        for (int f = 0; f < 30; ++f) {
            pane.setTrace(fakeTrace(340, 9520, f));
        }
        pane.resize(760, 230);   // wie das breite Feld oben

        QImage img(pane.size(), QImage::Format_ARGB32);
        img.fill(QColor(Style::kAppBg));
        pane.render(&img);

        const QString out = QStringLiteral("/tmp/bandfilter_neu.png");
        QVERIFY2(img.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out;
    }
};

QTEST_MAIN(TstFilterPaneSheet)
#include "tst_filter_pane_sheet.moc"
