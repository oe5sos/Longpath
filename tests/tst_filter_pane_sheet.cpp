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
    static QVector<float> fakeTrace(int n, int spanHz)
    {
        QVector<float> v(n, -118.0f);
        for (int i = 0; i < n; ++i) {
            const double hz = -spanHz / 2.0 + spanHz * double(i) / (n - 1);
            // Ein Sprechsignal im Durchlass und ein Stoerer daneben.
            const double a = std::exp(-std::pow((hz + 1500.0) / 900.0, 2.0));
            const double b = std::exp(-std::pow((hz - 3200.0) / 220.0, 2.0));
            v[i] = static_cast<float>(-118.0 + 46.0 * a + 34.0 * b
                                      + 3.0 * std::sin(i * 0.7));
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
        pane.setTrace(fakeTrace(340, 9520));
        pane.resize(520, 200);

        QImage img(pane.size(), QImage::Format_ARGB32);
        img.fill(QColor(Style::kAppBg));
        pane.render(&img);

        const QString out = QStringLiteral("/tmp/bandfilter_flaeche.png");
        QVERIFY2(img.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out;
    }
};

QTEST_MAIN(TstFilterPaneSheet)
#include "tst_filter_pane_sheet.moc"
