// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: malt die Bandfilter-Flaeche mit einem
// kuenstlichen Signal, damit man den Entwurf beurteilen kann, ohne
// ein Funkgeraet anzuschliessen.
//
// Anlass, 2026-08-22: "dieser sollte grafisch auch überarbeitet
// werden."
//
// Umbau 2026-09-17, Anlass: "der bandfilter könnte noch genauer und
// besser sein, schaut eher flach aus." Das Blatt hatte bis dahin mit
// 760×230 Punkten gemalt — eine Flaeche, die es beim Betreiber nie
// gab. Sein Bildschirmfoto vom selben Tag zeigt die Flaeche bei rund
// 620×105 Punkten, und DORT war die Kurve flach: nicht wegen der
// Zahlen, sondern weil das Zeichenfeld in dieser Hoehe nur noch
// 27 Punkte fuer 40 dB uebrig liess. Ein Blatt, das in einer anderen
// Groesse malt als der Betrieb, zeigt einen Fehler dieser Art nie.
//
// Deshalb jetzt:
//   * GENAU die Groesse aus dem Bildschirmfoto (620×105), dazu die
//     Vorgabegroesse eines frisch abgeloesten Fensters;
//   * in Bildschirmpunkten eines Retina-Schirms (devicePixelRatio 2),
//     damit sich das Blatt 1:1 neben das Foto legen laesst;
//   * ein Rauschen, das sich wie das des Panadapters verhaelt: je
//     FFT-Eimer exponentialverteilte Leistung, je Stuetzstelle das
//     MAXIMUM ueber die Eimer darunter — so rechnet
//     SpectrumWidget::dbmOverRange. Das gleichverteilte ±3,5-dB-
//     Zittern von vorher hatte weder den Boden noch die Streuung eines
//     echten Rauschflurs.

#include <QtTest>
#include <QPainter>
#include <QImage>
#include <QDir>
#include <cmath>

#include "gui/widgets/BandwidthFilterPane.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstFilterPaneSheet : public QObject
{
    Q_OBJECT

private:
    // Pseudozufall ohne Zufallsquelle: der Pruefstand muss
    // wiederholbar bleiben. Liefert [0,1).
    static double rnd(int i, int salt, int frame)
    {
        const double r = std::sin(i * 12.9898 + salt * 78.233 + frame * 4.1414)
                       * 43758.5453;
        return r - std::floor(r);
    }

    // Ein Bild aus dem laufenden Empfang, so wie es
    // BandwidthFilterApplet vom Panadapter bekommt:
    //
    //   1. FFT-Eimer von 11,7 Hz (192 kHz / 16384, die Vorgabe des
    //      FFTEngine) ueber die ganze Spanne, mit exponentialverteilter
    //      Rauschleistung um -125 dBm je Eimer;
    //   2. darauf ein LSB-Sprechsignal (drei Formanten, Silbenhuellkurve,
    //      Tonhoehen-Kamm), ein Traeger ausserhalb des Durchlasses und
    //      eine schwache zweite Station;
    //   3. je Stuetzstelle das MAXIMUM ueber die Eimer darunter, in dBm
    //      — Zeile fuer Zeile wie SpectrumWidget::dbmOverRange.
    static QVector<float> realisticTrace(int pts, int spanHz, int frame)
    {
        constexpr double kPi = 3.14159265358979323846;
        const double binW = 192000.0 / 16384.0;
        const int bins = static_cast<int>(spanHz / binW) + 2;
        const double noiseLin = std::pow(10.0, -125.0 / 10.0);

        // Silbenhuellkurve: 20 Bilder je Sekunde, eine Silbe rund 0,3 s.
        const double syl = 0.5 + 0.5 * std::sin(frame * 0.9)
                         * std::sin(frame * 0.37 + 1.3);
        const double voiceDb = -100.0 + 14.0 * (syl - 1.0);   // -114..-100

        QVector<double> lin(bins);
        for (int b = 0; b < bins; ++b) {
            const double hz = -spanHz / 2.0 + b * binW;

            // Rauschen: -ln(U) ist exponentialverteilt mit Mittel 1.
            const double u = std::max(1e-6, rnd(b, 1, frame));
            double p = -std::log(u) * noiseLin;

            // LSB-Sprache, Durchlass -2900..-100: Formanten bei
            // -500 / -1400 / -2400 Hz, dazu ein Kamm mit 130 Hz
            // Tonhoehe, der von Bild zu Bild leicht wandert.
            double formant = 1.0 * std::exp(-std::pow((hz + 500.0) / 260.0, 2.0))
                           + 0.55 * std::exp(-std::pow((hz + 1400.0) / 320.0, 2.0))
                           + 0.35 * std::exp(-std::pow((hz + 2400.0) / 300.0, 2.0));
            const double pitch = 130.0 + 6.0 * std::sin(frame * 0.5);
            const double comb = 0.35 + 0.65 * std::pow(
                0.5 + 0.5 * std::cos(2.0 * kPi * hz / pitch), 3.0);
            if (hz > -3000.0 && hz < 0.0 && formant > 1e-3) {
                const double vdb = voiceDb + 10.0 * std::log10(formant * comb);
                p += std::pow(10.0, vdb / 10.0)
                   * (0.5 + rnd(b, 2, frame));   // Sprache flackert selbst
            }

            // Ein Traeger bei +3200 Hz, zwei Eimer breit, -88 dBm.
            if (std::abs(hz - 3200.0) < binW) {
                p += std::pow(10.0, -88.0 / 10.0) * (0.85 + 0.3 * rnd(b, 3, frame));
            }

            // Eine schwache Gegenstation bei -4200 Hz, 8 dB ueber dem Flur.
            const double weak = std::exp(-std::pow((hz + 4200.0) / 350.0, 2.0));
            if (weak > 1e-3) {
                p += std::pow(10.0, -112.0 / 10.0) * weak * rnd(b, 4, frame) * 2.0;
            }
            lin[b] = p;
        }

        // Max je Stuetzstelle, wie dbmOverRange.
        QVector<float> out(pts, -200.0f);
        const double step = double(spanHz) / (pts - 1);
        for (int i = 0; i < pts; ++i) {
            const double f0 = -spanHz / 2.0 + step * (i - 0.5);
            const double f1 = -spanHz / 2.0 + step * (i + 0.5);
            int b0 = static_cast<int>(std::floor((f0 + spanHz / 2.0) / binW));
            int b1 = static_cast<int>(std::ceil((f1 + spanHz / 2.0) / binW));
            b0 = std::clamp(b0, 0, bins - 1);
            b1 = std::clamp(b1, 0, bins - 1);
            double peak = 0.0;
            for (int b = b0; b <= b1; ++b) { peak = std::max(peak, lin[b]); }
            out[i] = static_cast<float>(10.0 * std::log10(peak));
        }
        return out;
    }

    // Ein Blatt: Flaeche in der angegebenen Groesse (Qt-Punkte), mit
    // Retina-Dichte gerendert, nach `frames` Bildern Betrieb.
    static void drawSheet(const QString& name, int w, int h, int dotsPerPoint,
                          int spanHz)
    {
        BandwidthFilterPane pane;
        pane.setLabel(QStringLiteral("RX1"));
        pane.setAccent(QColor(Style::kAccent));
        // Wie auf dem Foto des Betreibers vom 2026-09-17: 7.192.500,
        // LSB 100–3000. Die halbe Kilohertz-Stelle ist kein Zufall —
        // an ihr sieht man, was die Achsenrundung anrichtet.
        pane.setVfoFrequency(7'192'500.0);
        pane.setHasFrequency(true);
        pane.setSpan(spanHz);
        pane.setFilter(-3000, -100);
        pane.resize(w, h);

        // Stuetzstellen wie im Betrieb: BandwidthFilterApplet rechnet
        // je zwoelf BILDSCHIRMpunkte eine, bei devicePixelRatio 2 also
        // je sechs Qt-Punkte. Wer die Formel dort aendert, aendert sie
        // hier mit — sonst zeigt das Blatt eine Dichte, die es in der
        // App nie gab (schon einmal passiert, 2026-08-23).
        constexpr qreal dpr = 2.0;
        const int pts = qBound(64, static_cast<int>(std::lround(w * dpr / dotsPerPoint)), 400);

        // Vierzig Bilder, wie zwei Sekunden Betrieb — erst dann wirkt
        // die zeitliche Glaettung, und die Bezugslinie hat sich gesetzt.
        for (int f = 0; f < 40; ++f) {
            pane.setTrace(realisticTrace(pts, spanHz, f));
        }

        QImage img(pane.size() * dpr, QImage::Format_ARGB32);
        img.setDevicePixelRatio(dpr);
        img.fill(QColor(Style::kAppBg));
        pane.render(&img);

        const QString out = QDir::temp().filePath(name);
        QVERIFY2(img.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out << pts << "Stuetzstellen";
    }

private slots:
    void drawTheSheets()
    {
        // Wie auf dem Foto des Betreibers vom 2026-09-17, 15:11: der
        // Bandfilter ueber die volle Fensterbreite gedockt, Flaeche
        // rund 1130×158 Punkte (Retina).
        drawSheet(QStringLiteral("bandfilter_1130x158.png"), 1130, 158, 12, 10000);
        // Die knappe Form von frueher (620×105) — dort muessen die
        // dBm-Zahlen weichen und die Wortmarken bleiben.
        drawSheet(QStringLiteral("bandfilter_620x105.png"), 620, 105, 12, 10000);
    }
};

QTEST_MAIN(TstFilterPaneSheet)
#include "tst_filter_pane_sheet.moc"
