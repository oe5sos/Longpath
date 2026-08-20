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

    // ── Und er muss sie auch FUELLEN ────────────────────────────────
    //
    // Die Pruefung darueber sagt nur, dass nichts herauslaeuft. Ein
    // Bogen von einem Zehntel der Breite liefe auch nicht heraus.
    //
    // Der Betreiber, 2026-08-20, nach dem ersten Anlauf: „s meter und
    // swr noch immer nicht formatfuellend." Der Radius kam damals aus
    // dem Entwurfsverhaeltnis 148/520; der Bogen nahm damit rund 55 %
    // der Breite ein und liess links und rechts je gut 20 % leer.
    //
    // Gemessen wird an der TINTE — die aeusserste gemalte Spalte auf
    // jeder Seite. Was gezeichnet wird, zaehlt, nicht was gerechnet
    // wurde.
    void theArcActuallyFillsTheWidth()
    {
        const QList<QSize> sizes = {
            QSize(440, 300), QSize(260, 200), QSize(700, 260)};
        for (const QSize& sz : sizes) {
            auto* inst = new InstrumentApplet(QStringLiteral("swr"),
                                              QStringLiteral("Stehwelle"),
                                              nullptr);
            QVERIFY(inst->setPrimary(MeterBinding::SignalPeak));
            inst->onReading(MeterBinding::SignalPeak, -80.0);
            inst->resize(sz);
            inst->show();
            QVERIFY(QTest::qWaitForWindowExposed(inst));
            QTest::qWait(120);

            QImage img(sz, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            inst->render(&img);

            // ── Nur der BOGEN, nicht die Fusszeile ───────────────
            //
            // Erster Anlauf: 100 % bei jeder Groesse. Zu schoen — die
            // Fusszeile („SIGNAL AVERAGE · Spitze · S-45 dBm") laeuft
            // ueber die ganze Breite und wurde als Tinte mitgezaehlt.
            // Der Test waere gruen gewesen, egal wie klein der Bogen
            // ist: genau die Sorte Pruefung, die nichts beweist.
            //
            // Also nur das obere Drittel bis zur Mitte: dort liegt der
            // Scheitel des Bogens und seine breiteste Stelle, und
            // nichts anderes.
            int left = sz.width(), right = -1, top = sz.height();
            const int scanTo = sz.height() * 55 / 100;
            for (int y = 0; y < scanTo; ++y) {
                for (int x = 0; x < sz.width(); ++x) {
                    const QRgb c = img.pixel(x, y);
                    if (qAlpha(c) < 8) { continue; }
                    // ── Nur KRAEFTIGE Tinte ──────────────────────────
                    //
                    // Erst stand hier 90 als Schwelle, und der Test war
                    // auch mit der ALTEN Rechnung gruen — er mass gar
                    // nicht den Bogen. Schuld war paintGlow: ein
                    // breiter Verlauf ueber die ganze Flaeche, dunkel,
                    // aber ueber 90.
                    //
                    // Der Bogen selbst (Mulde, Teilung, Beschriftung)
                    // ist deutlich heller. 200 trennt beides sauber —
                    // nachgewiesen mit der Gegenprobe: mit der alten
                    // Rechnung faellt der Test durch, mit der neuen
                    // nicht.
                    if (qRed(c) + qGreen(c) + qBlue(c) < 200) { continue; }
                    if (x < left)  { left = x; }
                    if (x > right) { right = x; }
                    if (y < top)   { top = y; }
                }
            }

            // ── Was man verlangen DARF ───────────────────────────────
            //
            // Ein Bogen um die Senkrechte ist hoechstens doppelt so
            // breit wie hoch. In einem Feld von 700 x 260 KANN er die
            // Breite nicht fuellen, egal wie man rechnet — dort bindet
            // die Hoehe, und das ist richtig so.
            //
            // Verlangt wird deshalb: er schlaegt an EINER Achse an.
            // Entweder fuellt er die Breite, oder er reicht bis dicht
            // unter den oberen Rand. Sitzt er an keiner von beiden an,
            // ist Platz verschenkt.
            //
            // Die Hoehe der Flaeche kennt der Test nicht (die Fusszeile
            // nimmt einen Teil, und wie viel, ist ihre Sache) — deshalb
            // wird nicht gerechnet, sondern nachgesehen, wie weit oben
            // die Tinte anfaengt.
            const double used = (right - left + 1) / double(sz.width());
            const bool widthBound  = used > 0.78;
            // Nicht 0: gemessen wird KRAEFTIGE Tinte, und die
            // aeusserste Kante der Mulde am Scheitel ist dunkler als
            // die Schwelle. Der oberste helle Bildpunkt liegt deshalb
            // ein Stueck unter dem echten Bogenrand — bei 700x260
            // gemessene 23 px bei rund 6 px tatsaechlichem Abstand.
            // Ein Achtel der Feldhoehe trennt „schlaegt oben an" von
            // „sitzt in der Mitte" zuverlaessig.
            const bool heightBound = top <= sz.height() / 8;
            qDebug().noquote()
                << QStringLiteral("%1x%2 -> %3 % der Breite, Tinte ab y=%4")
                       .arg(sz.width()).arg(sz.height())
                       .arg(used * 100.0, 0, 'f', 1).arg(top);
            QVERIFY2(widthBound || heightBound,
                     qPrintable(QStringLiteral(
                         "bei %1x%2 nur %3 %% der Breite bemalt und erst ab "
                         "y=%4 — der Bogen schlaegt an keiner Achse an, es "
                         "bleibt Platz ungenutzt")
                         .arg(sz.width()).arg(sz.height())
                         .arg(used * 100.0, 0, 'f', 1).arg(top)));

            inst->hide();
            delete inst;
        }
    }
};
QTEST_MAIN(TestNeedleFits)
#include "tst_needle_fits.moc"
