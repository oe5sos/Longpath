// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das breite rosa Band quer durch die Sendegruppe.
//
// Im Bildschirmfoto des Betreibers vom 2026-10-15 liegt unter der
// Knopfreihe des PHONE/CW-Feldes ein durchgehend rosa Balken mit der
// Aufschrift „Compression". Er ist nicht rosa, weil etwas zu laut
// waere — er ist IMMER rosa.
//
// HGauge::paintEvent hatte fuer m_reversed genau EINE Zeile: ein
// Rechteck in der Gefahrenfarbe, ohne jede Schwelle. Die
// Kompressionsanzeige ist der einzige umgedrehte Balken im Programm
// (PhoneCwApplet), Bereich -25..0 dB, setRedStart(1.0) — eine
// Schwelle OBERHALB des Maximums. Rot haette er also nie werden
// duerfen.
//
// Hausstil seit der Sendetaste in Messing: „Kein kraeftiges Rot als
// Zustandsfarbe — Rot bleibt der Warnung." Kompression ist ein
// Messwert.
//
// Gegenprobe (2026-10-15): mit der alten Zeile faerbt derselbe Lauf
// den halben Balken in die Gefahrenfarbe und faellt an
// „Gefahrenpixel".

#include <QtTest>
#include <QImage>
#include <QPainter>

#include "gui/HGauge.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstKompressionsbalkenNichtRot : public QObject
{
    Q_OBJECT

private:
    /// Zeichnet die Anzeige so, wie PhoneCwApplet sie aufsetzt.
    static QImage male(double wert)
    {
        HGauge g;
        g.setRange(-25.0, 0.0);
        g.setRedStart(1.0);
        g.setReversed(true);
        g.setTitle(QStringLiteral("Compression"));
        g.setValue(wert);
        g.resize(240, 34);

        QImage bild(g.size(), QImage::Format_RGB32);
        bild.fill(Qt::black);
        g.render(&bild);
        return bild;
    }

    static int zaehle(const QImage& b, const QColor& c)
    {
        int n = 0;
        for (int y = 0; y < b.height(); ++y) {
            for (int x = 0; x < b.width(); ++x) {
                if (b.pixelColor(x, y) == c) { ++n; }
            }
        }
        return n;
    }

private slots:
    void keinRotWoNichtsZuWarnenIst()
    {
        const QColor gefahr(QString::fromLatin1(Style::kGaugeDanger));
        const QColor gemessen(QString::fromLatin1(Style::kAmberDim));

        // Halb ausgesteuert und voll — in BEIDEN Lagen darf nichts
        // warnen, weil die Schwelle ueber dem Maximum liegt.
        for (const double wert : {-12.5, 0.0}) {
            const QImage b = male(wert);
            const int rot = zaehle(b, gefahr);
            const int amber = zaehle(b, gemessen);
            qInfo() << "Wert" << wert << "Gefahrenpixel" << rot
                    << "Messpixel" << amber;

            QVERIFY2(rot == 0,
                     qPrintable(QStringLiteral(
                         "%1 Gefahrenpixel bei %2 dB — die Schwelle steht "
                         "bei 1.0, also ueber dem Maximum 0.0")
                                    .arg(rot).arg(wert)));
            QVERIFY2(amber > 0,
                     qPrintable(QStringLiteral(
                         "gar kein Messbalken bei %1 dB").arg(wert)));
        }
    }

    /// Und der Balken ist nicht einfach weg: er waechst mit dem Wert.
    void derBalkenWaechstMitDemWert()
    {
        const QColor gemessen(QString::fromLatin1(Style::kAmberDim));
        const int wenig = zaehle(male(-20.0), gemessen);
        const int viel  = zaehle(male(-2.0),  gemessen);
        qInfo() << "Messpixel bei -20 dB:" << wenig << "bei -2 dB:" << viel;
        QVERIFY2(viel > wenig,
                 "der umgedrehte Balken reagiert nicht auf den Wert");
    }
};

QTEST_MAIN(TstKompressionsbalkenNichtRot)
#include "tst_kompressionsbalken_nicht_rot.moc"
