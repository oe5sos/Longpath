// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_needle_fits.cpp  (Longpath)
// =================================================================
// Der Zeigerbogen muss in jedes Seitenverhaeltnis passen.
//
// Der Betreiber, 2026-08-20: „RX1 laesst sich zwar verkleinern, der
// inhalt aendert sich aber nicht im massstab" — auf seinem Bild lief
// der Stehwellenzeiger oben aus dem Feld heraus. Ursache: der Radius
// wurde nur aus der BREITE gerechnet.
//
// Gemalt wird in ein Bild und nachgesehen, ob am oberen Rand noch
// unbemalte Zeilen stehen. Das ist die Eigenschaft, um die es geht —
// nicht eine Zahl im Quelltext, sondern ob der Bogen im Feld bleibt.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QImage>
#include <QPainter>
#include "gui/applets/InstrumentApplet.h"
#include "models/RadioModel.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

class TestNeedleFits : public QObject
{
    Q_OBJECT
private slots:
    void theArcStaysInsideAtEveryAspectRatio()
    {
        RadioModel model;
        auto* inst = new InstrumentApplet(QStringLiteral("swr"),
                                          QStringLiteral("Stehwelle"),
                                          &model, nullptr);
        // Eine Groesse mit Skala waehlen — ohne belegte Skala malt das
        // Instrument nichts, und der Test maesse ein leeres Bild.
        QVERIFY2(inst->setPrimary(MeterBinding::SignalPeak),
                 "die Groesse muss eine Skala haben");
        inst->onReading(MeterBinding::SignalPeak, -80.0);

        // Schmal und hoch, quadratisch, und die Breite des Betreibers.
        const QList<QSize> sizes = {
            QSize(260, 300), QSize(400, 400), QSize(702, 300),
            QSize(900, 220), QSize(1200, 260)
        };

        for (const QSize& sz : sizes) {
            inst->resize(sz);
            inst->show();
            QVERIFY(QTest::qWaitForWindowExposed(inst));
            QTest::qWait(60);

            QImage img(sz, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            inst->render(&img);

            // Wie viele Zeilen am oberen Rand zeigen NUR den
            // Hintergrund?
            //
            // Nicht „unbemalt": das Applet fuellt seine Flaeche, jede
            // Zeile traegt also Farbe. Gesucht ist die erste Zeile, in
            // der etwas ANDERES steht als der Grund — der Bogen, die
            // Teilung, der Zeiger. Als Grund gilt die linke obere Ecke;
            // dort liegt nie ein Bogen.
            const QRgb ground = img.pixel(0, 0);
            auto differs = [ground](QRgb c) {
                return qAbs(qRed(c)   - qRed(ground))   > 12
                    || qAbs(qGreen(c) - qGreen(ground)) > 12
                    || qAbs(qBlue(c)  - qBlue(ground))  > 12;
            };
            int emptyTop = 0;
            for (int y = 0; y < img.height(); ++y) {
                bool anyInk = false;
                for (int x = 0; x < img.width(); ++x) {
                    if (differs(img.pixel(x, y))) { anyInk = true; break; }
                }
                if (anyInk) { break; }
                ++emptyTop;
            }
            qDebug() << sz << "-> unbemalte Zeilen oben:" << emptyTop;

            // Der Bogen darf den oberen Rand nicht beruehren. Im
            // Entwurf bleiben 20 von 190 px Luft; wir verlangen
            // wenigstens eine Zeile, damit „laeuft hinaus" sicher
            // erkannt wird, ohne den Entwurf nachzurechnen.
            QVERIFY2(emptyTop >= 1,
                     qPrintable(QStringLiteral(
                         "bei %1x%2 stoesst der Inhalt an den oberen Rand — "
                         "der Bogen laeuft aus dem Feld")
                         .arg(sz.width()).arg(sz.height())));
        }
        inst->hide();
    }
};
QTEST_MAIN(TestNeedleFits)
#include "tst_needle_fits.moc"
