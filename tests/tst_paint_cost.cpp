// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: was kostet das Zeichnen der Flaechen, die
// NICHT auf der Grafikkarte laufen?
//
// Anlass, 2026-08-23. Der Betreiber meldete 97 % CPU. Eine Probe der
// laufenden App (macOS `sample`) zeigte den groessten Posten ausserhalb
// von Wartezustaenden in blend_untransformed_argb -> _platform_memmove,
// also im Qt-Threadpool beim Mischen von Bildern im Hauptspeicher —
// und SpectrumWidget::renderGpuFrame mit nur 136 Proben. Der Panadapter
// selbst kostet also fast nichts, er laeuft auf der Grafikkarte.
//
// Damit stand ein VERDACHT im Raum: die Last kommt aus den mit QPainter
// gezeichneten Flaechen daneben — Instrumente, Bandfilter,
// Spot-Beschriftungen. Ein Verdacht ist aber kein Befund, und der
// Betreiber sollte nicht als Messgeraet herhalten muessen.
//
// Dieses Werkzeug misst darum jede Flaeche einzeln: wie lange braucht
// ein Bild, und was macht das bei der Bildrate, mit der sie im Betrieb
// laeuft? Erst diese zweite Zahl sagt etwas — eine Flaeche, die 3 ms
// kostet, ist harmlos bei 5 Hz und ein Problem bei 60 Hz.

#include <QtTest>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>

#include "gui/StyleConstants.h"
#include "gui/applets/BandwidthFilterApplet.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstPaintCost : public QObject
{
    Q_OBJECT

private:
    struct Result { QString name; double msPerFrame; double hz; };
    QVector<Result> m_results;

    // Ein Bild oft genug malen, dass der Mittelwert etwas aussagt.
    double measure(QWidget* w, QSize size, int rounds = 60)
    {
        QImage img(size, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(Style::kAppBg));
        // Ein Aufwaermdurchgang: der erste Anstrich legt Schriftgitter
        // und Farbverlaeufe an und ist nie stellvertretend.
        w->render(&img);

        QElapsedTimer t;
        t.start();
        for (int i = 0; i < rounds; ++i) { w->render(&img); }
        return t.nsecsElapsed() / 1.0e6 / rounds;
    }

    void note(const QString& name, double ms, double hz)
    {
        m_results.append({name, ms, hz});
    }

private slots:
    void zeigerinstrument()
    {
        NeedleInstrument n;
        n.setPrimary(MeterBinding::SignalAvg);
        n.onReading(MeterBinding::SignalAvg, -73.0);
        n.resize(220, 150);
        // Die Instrumente haengen am MeterPoller; der laeuft im Betrieb
        // mit 10 Hz.
        note(QStringLiteral("NeedleInstrument 220x150"),
             measure(&n, QSize(220, 150)), 10.0);
    }

    void balkeninstrument()
    {
        BarInstrument b;
        b.setPrimary(MeterBinding::TxSwr);
        b.onReading(MeterBinding::TxSwr, 1.6);
        b.resize(300, 72);
        note(QStringLiteral("BarInstrument flach 300x72"),
             measure(&b, QSize(300, 72)), 10.0);

        b.setSegmented(true);
        b.setTube(true);
        note(QStringLiteral("BarInstrument Roehre+Segmente"),
             measure(&b, QSize(300, 72)), 10.0);
    }

    void bandfilterflaeche()
    {
        // Der Verdaechtige Nummer eins: er zeichnet mit 20 Hz eine
        // geglaettete Kurve ueber die volle Breite, dazu Hof,
        // Fuellung, Skala und Zellen.
        BandwidthFilterPane pane;
        pane.setLabel(QStringLiteral("RX1"));
        pane.setVfoFrequency(7'131'300.0);
        pane.setHasFrequency(true);
        pane.setSpan(9520);
        pane.setFilter(-2900, -100);
        pane.resize(1180, 420);

        QVector<float> trace(qBound(48, 1180 / 12, 200));
        for (int i = 0; i < trace.size(); ++i) {
            trace[i] = -120.0f + 30.0f * std::sin(i * 0.3f);
        }
        for (int f = 0; f < 5; ++f) { pane.setTrace(trace); }

        note(QStringLiteral("BandwidthFilterPane 1180x420"),
             measure(&pane, QSize(1180, 420), 40), 20.0);
    }

    void zusammenfassung()
    {
        qInfo().noquote() << "";
        qInfo().noquote() << QStringLiteral(
            "Flaeche                              ms/Bild    Hz    %% eines Kerns");
        qInfo().noquote() << QString(66, QLatin1Char('-'));
        double total = 0.0;
        for (const Result& r : m_results) {
            const double load = r.msPerFrame * r.hz / 10.0;   // % eines Kerns
            total += load;
            qInfo().noquote()
                << QStringLiteral("%1 %2 %3 %4")
                       .arg(r.name, -36)
                       .arg(QString::number(r.msPerFrame, 'f', 2), 7)
                       .arg(QString::number(r.hz, 'f', 0), 5)
                       .arg(QString::number(load, 'f', 1) + QStringLiteral(" %"), 12);
        }
        qInfo().noquote() << QString(66, QLatin1Char('-'));
        qInfo().noquote() << QStringLiteral("Summe: %1 %% eines Kerns")
                                 .arg(total, 0, 'f', 1);
        qInfo().noquote() << "";
        qInfo().noquote() << QStringLiteral(
            "Zur Einordnung: gemessen wird OHNE Grafikkarte, in einen "
            "Speicherpuffer.");
        qInfo().noquote() << QStringLiteral(
            "Im Betrieb kommt der Weg zum Bildschirm dazu; die Zahlen sind "
            "also eine Untergrenze.");
    }
};

QTEST_MAIN(TstPaintCost)
#include "tst_paint_cost.moc"
