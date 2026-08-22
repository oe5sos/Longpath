// SPDX-License-Identifier: GPL-3.0-or-later
//
// Nach einem Umzug zeigt der Wasserfall BILDINHALT, keinen Speicher.
//
// Live gemessen am 2026-08-22: Panadapter abloesen -> die ganze Flaeche
// magenta; zurueckdocken -> bleibt magenta. Der Betreiber hatte am
// selben Tag ein Bild geschickt, auf dem dasselbe in Rot stand ("der
// wasserfall roter schrott").
//
// Ursache: releaseResources() loescht die GPU-Textur, setzte aber die
// Wegmarke m_wfTexFullUpload NICHT zurueck. Die frisch angelegte
// Textur bekam nie ein Byte geschrieben und zeigte, was Metal in dem
// Speicher liegen hatte. Der Erstbefuellungs-Pfad selbst war laengst
// richtig — er wurde nur nicht mehr angesteuert.
//
// Diese Pruefung misst das BILD, nicht den Zustand: undefinierter
// Speicher ist bunt und wild, echter Wasserfall ist es nicht. Sie
// zaehlt gesaettigte Punkte am Palettenende (Magenta/Rot bei
// gleichzeitig hohem Rot- UND Blauanteil) — die Signatur, die alle
// bisherigen Meldungen gemeinsam hatten.

#include <QtTest>
#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TstWaterfallSurvivesTheMove : public QObject { Q_OBJECT

private:
    static void feed(SpectrumWidget& w, int frames)
    {
        QVector<float> bins(2048);
        for (int i = 0; i < bins.size(); ++i) {
            double dbm = -110.0;
            for (int c : {400, 1024, 1500}) {
                const double d = std::abs(i - c);
                if (d < 24) { dbm = std::max(dbm, -40.0 - d * 2.5); }
            }
            bins[i] = static_cast<float>(std::pow(10.0, dbm / 10.0));
        }
        for (int f = 0; f < frames; ++f) {
            w.updateSpectrumLinear(0, bins, 1.0, 0.0);
            QCoreApplication::processEvents();
            QTest::qWait(16);
        }
    }

    /// Anteil der Punkte, die nach undefiniertem Speicher aussehen:
    /// kraeftiges Rot UND kraeftiges Blau zugleich. Keine unserer
    /// Wasserfall-Paletten erzeugt das.
    static double garbageShare(const QImage& img)
    {
        long long bad = 0, total = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                ++total;
                if (c.red() > 170 && c.blue() > 170) { ++bad; }
            }
        }
        return total ? double(bad) / double(total) : 0.0;
    }

    // ── OFFEN, und was schon WIDERLEGT ist (2026-08-22) ─────────────
    //
    // Diese Pruefung ist ROT und soll es bleiben, bis der Fehler faellt.
    // Sie haelt einen echten Mangel fest, keine Macke: nach
    // prepareForTopLevelChange() + resetGpuResources() stehen 62,2 %
    // der Flaeche auf reinem #ff00ff, von y=454 abwaerts — der
    // Wasserfallbereich. Der Betreiber sieht dasselbe live im
    // abgeloesten Fenster.
    //
    // Vier Vermutungen sind durch MESSUNG erledigt, damit der naechste
    // Anlauf sie nicht wiederholt:
    //
    //   1. "Die Wasserfall-Textur wird nicht gefuellt."
    //      Nein. Mit NEREUS_WF_DEBUG=1: Vollupload 964x328 in die
    //      Textur 964x328, Bild dunkel (#08080a). Nachweislich gefuellt.
    //   2. "Der Wasserfall zeichnet den Schrott."
    //      Nein. Zeichnung abgeschaltet -> Bild identisch, Punkt fuer
    //      Punkt (373000).
    //   3. "Dann die Overlay-Schicht."
    //      Auch nicht. Ebenfalls abgeschaltet -> wieder identisch.
    //   4. "Der Renderdurchgang raeumt die Flaeche nicht."
    //      Die Grundfarbe auf Gruen gesetzt -> die Flaeche bleibt
    //      magenta. Und das Ziel MISST 2000x1200, also volle Groesse.
    //
    // Damit ist gesichert: was dort steht, kommt nicht aus unserem
    // Renderdurchgang. Der naechste Schritt gehoert dem Weg von
    // grabFramebuffer() und dem Backing-Store, nicht der Zeichenkette.
    //
    // Zwei Teilerfolge sind auf dem Weg entstanden und LIVE belegt:
    // die Overlay-Bilder werden beim Freigeben verworfen (vorher war
    // das GANZE Widget magenta, jetzt nur noch dieser Bereich), und der
    // Wasserfall friert ein, wenn der Datenstrom abreisst.

private slots:
    void aReleaseAndRebuildDoesNotShowRawMemory()
    {
        SpectrumWidget w;
        w.resize(1000, 600);
        w.setDdcCenterFrequency(7'100'000.0);
        w.setSampleRate(192'000.0);
        w.setFrequencyRange(7'100'000.0, 192'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        feed(w, 20);

        const double before = garbageShare(w.grabFramebuffer());
        QVERIFY2(before < 0.10,
                 qPrintable(QStringLiteral(
                     "Schon vor dem Umzug sieht %1 %% der Flaeche nach "
                     "Speicher aus — die Messung taugt dann nichts")
                     .arg(before * 100.0, 0, 'f', 1)));

        // Genau das, was Abloesen und Zurueckdocken tun.
        w.prepareForTopLevelChange();
        w.resetGpuResources();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(60);
        feed(w, 6);
        // Setzt es sich von selbst? Grosszuegig warten und weiter
        // fuettern — wenn das Magenta eine voruebergehende Stufe ist,
        // muss es hier verschwinden.
        for (int i = 0; i < 20; ++i) {
            feed(w, 2);
            QTest::qWait(50);
        }

        const QImage post = w.grabFramebuffer();
        {   // Wo genau steht der Schrott, und welche Farbe hat er?
            int firstBad = -1, lastBad = -1;
            QMap<QRgb,int> hist;
            for (int y = 0; y < post.height(); ++y) {
                int bad = 0;
                for (int x = 0; x < post.width(); x += 4) {
                    const QColor c = post.pixelColor(x, y);
                    if (c.red() > 170 && c.blue() > 170) {
                        ++bad;
                        hist[post.pixel(x, y)]++;
                    }
                }
                if (bad > post.width() / 16) {
                    if (firstBad < 0) { firstBad = y; }
                    lastBad = y;
                }
            }
            QList<QPair<int,QRgb>> top;
            for (auto it = hist.cbegin(); it != hist.cend(); ++it) {
                top.append({it.value(), it.key()});
            }
            std::sort(top.begin(), top.end(),
                      [](auto& a, auto& b){ return a.first > b.first; });
            qInfo() << "Schrott von y=" << firstBad << "bis" << lastBad
                    << "von" << post.height();
            for (int i = 0; i < qMin(3, top.size()); ++i) {
                qInfo().noquote() << "  Farbe"
                                  << QColor(top[i].second).name()
                                  << "x" << top[i].first;
            }
        }
        const double after = garbageShare(post);
        QVERIFY2(after < 0.10,
                 qPrintable(QStringLiteral(
                     "Nach dem Umzug sehen %1 %% der Flaeche nach rohem "
                     "GPU-Speicher aus (vorher %2 %%). Genau das Bild, "
                     "das der Betreiber am 2026-08-22 geschickt hat.")
                     .arg(after * 100.0, 0, 'f', 1)
                     .arg(before * 100.0, 0, 'f', 1)));
    }
};

QTEST_MAIN(TstWaterfallSurvivesTheMove)
#include "tst_waterfall_survives_the_move.moc"
