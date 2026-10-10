// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_eq_one_drag_one_parameter.cpp  (Longpath)
// =================================================================
// „das klicken der linie bewegt teilweise die komplette kurve, was ich
//  oft nicht will. als user sollte das grundsätzlich einfach
//  gestaltet werden."
//
// Zwei Ursachen, beide hier festgenagelt:
//
//  1. Ein Zug aenderte Frequenz UND Pegel zugleich. Wer anheben
//     wollte, verschob das Band nebenbei zur Seite -- bei Q = 0,707
//     ueber rund zwei Oktaven, und das sieht aus wie „die ganze Kurve
//     bewegt sich".
//  2. Schon das DRUECKEN schaltete ein Band ein. Die zehn Griffe
//     liegen ab Werk alle auf der Nulllinie, also war jeder Klick auf
//     die Linie bereits ein Eingriff in den Ton.
//
// Geprueft wird die Wirkung auf die Baender, nicht die Beschriftung
// einer Schaltflaeche.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QMouseEvent>
#include <QWheelEvent>

#include "core/strip/ClientEq.h"
#include "gui/applets/eq/ClientEqEditorCanvas.h"

using namespace Longpath;

namespace {

// Die Maushandler werden direkt gerufen. Dass Qt ein Ereignis beim
// richtigen Widget abliefert, ist Qts Sache; geprueft wird, was der
// Handler daraus macht.
class Probe : public ClientEqEditorCanvas {
public:
    using ClientEqEditorCanvas::mousePressEvent;
    using ClientEqEditorCanvas::mouseMoveEvent;
    using ClientEqEditorCanvas::mouseReleaseEvent;
    using ClientEqEditorCanvas::wheelEvent;
    using ClientEqCurveWidget::freqToX;
    using ClientEqCurveWidget::dbToY;
};

void press(Probe& v, const QPointF& p, Qt::KeyboardModifiers m = Qt::NoModifier)
{
    QMouseEvent ev(QEvent::MouseButtonPress, p, p, Qt::LeftButton,
                   Qt::LeftButton, m);
    v.mousePressEvent(&ev);
}

void move(Probe& v, const QPointF& p, Qt::KeyboardModifiers m = Qt::NoModifier)
{
    QMouseEvent ev(QEvent::MouseMove, p, p, Qt::NoButton, Qt::LeftButton, m);
    v.mouseMoveEvent(&ev);
}

void release(Probe& v, const QPointF& p)
{
    QMouseEvent ev(QEvent::MouseButtonRelease, p, p, Qt::LeftButton,
                   Qt::NoButton, Qt::NoModifier);
    v.mouseReleaseEvent(&ev);
}

// Das vierte Band der Werkskurve: eine Glocke bei 400 Hz, Pegel 0.
constexpr int kBand = 3;

} // namespace

class TstEqOneDragOneParameter : public QObject {
    Q_OBJECT

private:
    ClientEq m_eq;

    QPointF handleOf(Probe& v, int idx)
    {
        const auto bp = m_eq.band(idx);
        return QPointF(v.freqToX(bp.freqHz), v.dbToY(bp.gainDb));
    }

    void build(Probe& v)
    {
        m_eq.setActiveBandCount(ClientEq::kDefaultBandCount);
        for (int i = 0; i < ClientEq::kDefaultBandCount; ++i) {
            m_eq.setBand(i, ClientEq::defaultBand(i));
        }
        v.setEq(&m_eq);
        v.resize(800, 300);
    }

private slots:
    // Senkrecht ziehen hebt an -- und sonst nichts. Die Frequenz des
    // Bandes steht danach da, wo sie stand.
    void draggingUpDoesNotMoveTheFrequency()
    {
        Probe v;
        build(v);
        const float f0 = m_eq.band(kBand).freqHz;

        // Schraeg ziehen, wie es mit der Hand nun einmal passiert:
        // 60 px hoch, 40 px zur Seite.
        const QPointF h = handleOf(v, kBand);
        press(v, h);
        move(v, h + QPointF(40, -60));
        release(v, h + QPointF(40, -60));

        QCOMPARE(m_eq.band(kBand).freqHz, f0);
        QVERIFY2(m_eq.band(kBand).gainDb > 2.0f,
                 qPrintable(QStringLiteral("nicht angehoben: %1 dB")
                                .arg(double(m_eq.band(kBand).gainDb))));
        QVERIFY(m_eq.band(kBand).enabled);
    }

    // Mit ⌥ ist es umgekehrt: die Frequenz wandert, der Pegel bleibt.
    void altDragMovesOnlyTheFrequency()
    {
        Probe v;
        build(v);
        const float g0 = m_eq.band(kBand).gainDb;

        const QPointF h = handleOf(v, kBand);
        press(v, h, Qt::AltModifier);
        move(v, h + QPointF(60, -40), Qt::AltModifier);
        release(v, h + QPointF(60, -40));

        QCOMPARE(m_eq.band(kBand).gainDb, g0);
        QVERIFY2(m_eq.band(kBand).freqHz > 500.0f,
                 qPrintable(QStringLiteral("Frequenz blieb stehen: %1 Hz")
                                .arg(double(m_eq.band(kBand).freqHz))));
    }

    // Ein Klick ist keine Aenderung. Weder schaltet er ein Band ein
    // noch verstellt er etwas -- auch dann nicht, wenn der Zeiger den
    // Griff um ein paar Pixel verfehlt.
    void aClickAloneChangesNothing()
    {
        Probe v;
        build(v);
        const auto before = m_eq.band(kBand);

        const QPointF h = handleOf(v, kBand) + QPointF(2, 2);
        press(v, h);
        move(v, h + QPointF(1, -1));
        release(v, h + QPointF(1, -1));

        const auto after = m_eq.band(kBand);
        QVERIFY2(!after.enabled, "ein Klick hat das Band eingeschaltet");
        QCOMPARE(after.gainDb, before.gainDb);
        QCOMPARE(after.freqHz, before.freqHz);
    }

    // Das Rad über einem Griff macht ihn schmaler oder breiter --
    // ohne Taste, weil eine Taste, die man kennen muss, keine
    // Bedienung ist. („man sollte das auch minimaler erhöhen
    // können, nicht sooo breit.")
    void theWheelNarrowsTheBand()
    {
        Probe v;
        build(v);
        const QPointF h = handleOf(v, kBand);
        const float vorher = m_eq.band(kBand).q;

        QWheelEvent rauf(h, h, QPoint(), QPoint(0, 360), Qt::NoButton,
                         Qt::NoModifier, Qt::NoScrollPhase, false);
        v.wheelEvent(&rauf);
        const float enger = m_eq.band(kBand).q;

        QWheelEvent runter(h, h, QPoint(), QPoint(0, -720), Qt::NoButton,
                           Qt::NoModifier, Qt::NoScrollPhase, false);
        v.wheelEvent(&runter);
        const float weiter = m_eq.band(kBand).q;

        qInfo() << "Güte" << vorher << "->" << enger << "->" << weiter;
        QVERIFY2(enger > vorher * 1.3f,
                 qPrintable(QStringLiteral("%1 -> %2").arg(double(vorher))
                                .arg(double(enger))));
        QVERIFY2(weiter < enger * 0.8f,
                 qPrintable(QStringLiteral("%1 -> %2").arg(double(enger))
                                .arg(double(weiter))));
    }

    // Neben einem Griff tut das Rad nichts -- sonst verstellt ein
    // Rollen über der Kurve zufällig irgendein Band.
    void theWheelDoesNothingBesideAHandle()
    {
        Probe v;
        build(v);
        const auto vorher = m_eq.band(kBand);
        const QPointF weitweg = handleOf(v, kBand) + QPointF(0, 90);
        QWheelEvent w(weitweg, weitweg, QPoint(), QPoint(0, 360),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        v.wheelEvent(&w);
        QCOMPARE(m_eq.band(kBand).q, vorher.q);
    }

    // Und der Zug rechnet vom Griffpunkt aus, nicht vom Zeiger: wer
    // 4 px daneben greift, darf das Band nicht um 4 px springen sehen.
    void grabbingSlightlyOffDoesNotJump()
    {
        Probe v;
        build(v);

        // 6 px ueber dem Griff ansetzen und nur waagrecht wackeln --
        // der Pegel darf dabei nicht auf die Hoehe des Zeigers
        // springen.
        const QPointF h = handleOf(v, kBand) + QPointF(0, -6);
        press(v, h);
        move(v, h + QPointF(5, 0));
        const float g = m_eq.band(kBand).gainDb;
        release(v, h + QPointF(5, 0));

        QVERIFY2(std::abs(g) < 0.5f,
                 qPrintable(QStringLiteral("gesprungen auf %1 dB")
                                .arg(double(g))));
    }
};

QTEST_MAIN(TstEqOneDragOneParameter)
#include "tst_eq_one_drag_one_parameter.moc"
