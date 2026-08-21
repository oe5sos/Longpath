// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_rotor_dial_paint.cpp  (Longpath)
// =================================================================
// Zwei Fehler, die man nur SIEHT.
//
// Am 2026-08-20 hat der Betreiber gebeten, ihm die Rotorgrafik noch
// einmal zu schicken. Beim Rendern fielen zwei Dinge auf, die keine
// Pruefung je haette melden koennen, weil keine je ein Bild angesehen
// hat:
//
//   1. Der Keulensektor stand um seine eigene Breite neben dem
//      Zeiger. Ursache: `90 - Peilung - Breite/2` statt `+ Breite/2`.
//      Qt zaehlt von 3 Uhr gegen den Uhrzeigersinn, eine Peilung von
//      Nord im Uhrzeigersinn; der Sektor muss um die umgerechnete
//      Richtung HERUM liegen.
//
//   2. Zeiger, Nabe und Ablesung sagten Verschiedenes: die Nabe
//      faerbte am Ziel gruen, der Zeiger blieb rot — und Rot
//      behauptet Gefahr, wo „angekommen" steht.
//
// Diese Pruefung malt und schaut nach. Sie ist damit die einzige
// Sorte, die solche Fehler ueberhaupt fangen kann.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QImage>
#include <cmath>
#include "gui/widgets/RotorDialWidget.h"
#include "core/AppSettings.h"
#include <QSignalSpy>

using namespace Longpath;

namespace {

QImage renderDial(RotorDialWidget* w)
{
    QImage img(w->size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    w->render(&img);
    return img;
}

// Ein Punkt auf der Peilung `deg`, `frac` des Radius von der Mitte.
QPoint pointAt(const QSize& sz, double deg, double frac)
{
    const double cx = sz.width() * 0.5;
    const double cy = sz.height() * 0.42;          // roseCentre()
    const double r  = std::min(sz.width() * 0.42, sz.height() * 0.36);
    const double a  = (90.0 - deg) * M_PI / 180.0;
    return QPoint(qRound(cx + r * frac * std::cos(a)),
                  qRound(cy - r * frac * std::sin(a)));
}

double brightness(QRgb c)
{
    return 0.299 * qRed(c) + 0.587 * qGreen(c) + 0.114 * qBlue(c);
}

} // namespace

class TestRotorDialPaint : public QObject
{
    Q_OBJECT
private slots:

    // Der Sektor MUSS auf der Antennenrichtung liegen.
    void theBeamSectorSitsOnTheHeading()
    {
        auto* w = new RotorDialWidget();
        w->resize(360, 400);
        w->setBeamWidth(40);
        w->setActualBearing(45);
        w->setState(RotorDialWidget::State::Idle);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));
        QTest::qWait(80);

        const QImage img = renderDial(w);

        // Knapp NEBEN dem Zeiger, damit der Zeiger selbst nicht
        // gemessen wird: 12° zur Seite liegt noch in der 40°-Keule.
        const double inside  = brightness(img.pixel(pointAt(img.size(), 57, 0.62)));
        // Und weit ausserhalb: 90° daneben.
        const double outside = brightness(img.pixel(pointAt(img.size(), 135, 0.62)));

        QVERIFY2(inside > outside + 2.0,
                 qPrintable(QStringLiteral(
                     "die Keule liegt nicht auf der Peilung — bei 45° ist "
                     "es innen (%1) nicht heller als aussen (%2)")
                     .arg(inside, 0, 'f', 1).arg(outside, 0, 'f', 1)));
        w->hide();
        delete w;
    }

    // Am Ziel ist der Zeiger GRUEN, nicht rot.
    void theNeedleTurnsGreenOnTarget()
    {
        auto* w = new RotorDialWidget();
        w->resize(360, 400);
        w->setBeamWidth(0);
        w->setActualBearing(90);
        w->setTargetBearing(90);
        w->setState(RotorDialWidget::State::OnTarget);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));
        QTest::qWait(80);

        const QImage img = renderDial(w);
        const QRgb ink = img.pixel(pointAt(img.size(), 90, 0.60));

        QVERIFY2(qGreen(ink) > qRed(ink) + 10,
                 qPrintable(QStringLiteral(
                     "der Zeiger ist am Ziel nicht gruen (r=%1 g=%2 b=%3) — "
                     "Nabe und Ablesung faerben dort gruen, ein roter "
                     "Zeiger behauptet Gefahr, wo angekommen steht")
                     .arg(qRed(ink)).arg(qGreen(ink)).arg(qBlue(ink))));
        w->hide();
        delete w;
    }

    // ── Zwei Formen zur Wahl ────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-21, nach dem Entwurfsblatt: „beide zur
    // auswahl, standard vollkreis."
    //
    // Der Grund fuer das Band steht im Entwurf: ein Vollkreis ist so
    // breit wie hoch und kann eine Flaeche von 1180 x 330 nie fuellen.
    // Das Band gibt die Rundform auf und gewinnt dafuer die Breite.
    void thePlainCircleIsTheDefault()
    {
        AppSettings::instance().remove(QStringLiteral("RotorDialShape"));
        RotorDialWidget w;
        QCOMPARE(w.shape(), RotorDialWidget::Shape::Rose);
    }

    void theShapeChoiceSurvivesARestart()
    {
        {
            RotorDialWidget w;
            w.setShape(RotorDialWidget::Shape::Tape);
            QCOMPARE(w.shape(), RotorDialWidget::Shape::Tape);
        }
        // „Neustart": ein neues Zifferblatt liest die gemerkte Wahl.
        RotorDialWidget again;
        QCOMPARE(again.shape(), RotorDialWidget::Shape::Tape);
        AppSettings::instance().remove(QStringLiteral("RotorDialShape"));
    }

    // Im Band steht die Antenne FEST in der Mitte — ein Klick dorthin
    // meint also ihre eigene Richtung, und weiter rechts mehr Grad.
    void clickingTheTapeMeansTheRightBearing()
    {
        auto* w = new RotorDialWidget();
        w->setShape(RotorDialWidget::Shape::Tape);
        w->resize(600, 200);
        w->setActualBearing(90);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));

        QSignalSpy spy(w, &RotorDialWidget::targetPicked);

        auto click = [w](int x) {
            QMouseEvent ev(QEvent::MouseButtonPress, QPointF(x, 100),
                           w->mapToGlobal(QPointF(x, 100)), Qt::LeftButton,
                           Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(w, &ev);
        };

        click(300);                      // die Mitte
        QCOMPARE(spy.count(), 1);
        QVERIFY2(qAbs(spy.at(0).at(0).toDouble() - 90.0) < 2.0,
                 qPrintable(QStringLiteral(
                     "ein Klick auf die Mitte MUSS die eigene Richtung "
                     "meinen, kam aber als %1 an")
                     .arg(spy.at(0).at(0).toDouble())));

        spy.clear();
        click(450);                      // ein Viertel nach rechts
        QCOMPARE(spy.count(), 1);
        const double right = spy.at(0).at(0).toDouble();
        QVERIFY2(right > 90.0 && right < 210.0,
                 qPrintable(QStringLiteral(
                     "rechts der Mitte MUSS mehr Grad heissen, kam als %1")
                     .arg(right)));

        w->hide();
        delete w;
        AppSettings::instance().remove(QStringLiteral("RotorDialShape"));
    }

    // Und es malt wirklich etwas anderes.
    void theTwoShapesLookDifferent()
    {
        auto shot = [](RotorDialWidget::Shape sh) {
            auto* w = new RotorDialWidget();
            w->setShape(sh);
            w->resize(600, 220);
            w->setActualBearing(45);
            w->show();
            QTest::qWaitForWindowExposed(w);
            QTest::qWait(60);
            QImage img(w->size(), QImage::Format_ARGB32);
            img.fill(Qt::black);
            w->render(&img);
            w->hide();
            delete w;
            return img;
        };
        const QImage rose = shot(RotorDialWidget::Shape::Rose);
        const QImage tape = shot(RotorDialWidget::Shape::Tape);
        int diff = 0;
        for (int y = 0; y < rose.height(); y += 3) {
            for (int x = 0; x < rose.width(); x += 3) {
                if (rose.pixel(x, y) != tape.pixel(x, y)) { ++diff; }
            }
        }
        QVERIFY2(diff > 500,
                 qPrintable(QStringLiteral(
                     "die beiden Formen unterscheiden sich in nur %1 "
                     "Bildpunkten — dann ist eine davon nicht gebaut")
                     .arg(diff)));
        AppSettings::instance().remove(QStringLiteral("RotorDialShape"));
    }
};
QTEST_MAIN(TestRotorDialPaint)
#include "tst_rotor_dial_paint.moc"
