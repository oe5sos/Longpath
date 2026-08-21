// SPDX-License-Identifier: GPL-3.0-or-later
//
// Rechtsklick auf einen Notch-Balken gehoert dem Notch — nicht dem
// Pan-Menue darunter.
//
// Der Betreiber, 2026-08-21, mit einem Bildschirmfoto: Rechtsklick
// genau auf einen gelben Balken, aufgegangen ist „Add slice on this
// pan / Float this pan / Extended view".
//
// Der Grund liegt nicht im Notch-Code, sondern in Qt: ein Rechtsklick
// liefert einen mousePressEvent UND, davon unabhaengig, einen
// QContextMenuEvent. Das Druckereignis haben wir angenommen, das
// zweite nicht — es wanderte zum Elternteil, und dessen Menue geht mit
// exec() auf. exec() haelt die Runde an und legt sich obenauf.
//
// UND DAS IST DER PUNKT DIESES TESTS: der Test von gestern
// (tst_notch_removable_offline) klickte auf einen FREISTEHENDEN
// SpectrumWidget. Ohne Elternteil gibt es niemanden, zu dem das zweite
// Ereignis wandern koennte — der Fehler konnte dort gar nicht
// auftreten. Gruen fuer nichts.
//
// Hier steht der Panadapter deshalb in einem Elternteil, der mitzaehlt,
// wie oft ihn ein Kontextmenue-Ereignis erreicht. Das ist genau die
// Groesse, um die es geht.

#include <QtTest>
#include <QContextMenuEvent>
#include <QVBoxLayout>
#include <QWidget>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

/// Steht fuer PanadapterApplet: er faengt Kontextmenue-Ereignisse ab,
/// die das Kind nicht beantwortet hat, und macht sein eigenes Menue auf.
class CountingParent : public QWidget {
public:
    int reached {0};

protected:
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        ++reached;
        e->accept();
    }
};

} // namespace

class TstNotchMenuWinsOverPanMenu : public QObject
{
    Q_OBJECT

private:
    CountingParent   m_host;
    SpectrumWidget*  m_pan {nullptr};
    int              m_notchX {-1};

private slots:
    void initTestCase()
    {
        auto* col = new QVBoxLayout(&m_host);
        col->setContentsMargins(0, 0, 0, 0);
        m_pan = new SpectrumWidget(&m_host);
        col->addWidget(m_pan, 1);
        m_host.resize(1200, 700);
        m_host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&m_host));

        m_pan->setFrequencyRange(7'131'200.0, 200'000.0);

        QVector<SpectrumWidget::NotchMarker> notches;
        SpectrumWidget::NotchMarker n;
        n.id = 5; n.freqMhz = 7.1312; n.widthHz = 400.0; n.active = true;
        notches.append(n);
        m_pan->setNotchMarkers(notches);
        QCoreApplication::processEvents();

        for (int probe = 0; probe < m_pan->width(); ++probe) {
            if (m_pan->notchAtPixelForTest(probe) == 5) {
                m_notchX = probe;
                break;
            }
        }
        QVERIFY2(m_notchX >= 0, "Der Notch liegt nirgends auf dem Bild");
    }

    /// Auf dem Balken: der Elternteil darf NICHTS abbekommen.
    void onANotchTheParentMenuStaysShut()
    {
        m_host.reached = 0;

        QContextMenuEvent ev(QContextMenuEvent::Mouse,
                             QPoint(m_notchX, 80),
                             m_pan->mapToGlobal(QPoint(m_notchX, 80)));
        QApplication::sendEvent(m_pan, &ev);
        QCoreApplication::processEvents();

        QVERIFY2(ev.isAccepted(),
                 "Der Panadapter laesst das Ereignis durch — das Pan-Menue "
                 "legt sich dann ueber das Notch-Menue");
        QCOMPARE(m_host.reached, 0);
    }

    /// Auf freier Flaeche muss das Pan-Menue weiter aufgehen.
    ///
    /// Ohne diese Haelfte waere die Behebung eine Verschlimmbesserung:
    /// ein Panadapter, der jeden Rechtsklick schluckt, haette gar kein
    /// Menue mehr.
    void onEmptySpectrumTheParentMenuStillOpens()
    {
        m_host.reached = 0;

        // Weit weg vom Notch — und weit weg vom Rand.
        int freeX = m_notchX + 200;
        if (freeX > m_pan->width() - 60) { freeX = m_notchX - 200; }
        QVERIFY(freeX > 20);
        QCOMPARE(m_pan->notchAtPixelForTest(freeX), -1);

        QContextMenuEvent ev(QContextMenuEvent::Mouse,
                             QPoint(freeX, 80),
                             m_pan->mapToGlobal(QPoint(freeX, 80)));
        QApplication::sendEvent(m_pan, &ev);
        QCoreApplication::processEvents();

        QCOMPARE(m_host.reached, 1);
    }
};

QTEST_MAIN(TstNotchMenuWinsOverPanMenu)
#include "tst_notch_menu_wins_over_pan_menu.moc"
