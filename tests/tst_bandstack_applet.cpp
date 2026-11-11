// no-port-check: Pruefstand fuer Longpath-eigene Oberflaeche.
// =================================================================
// tests/tst_bandstack_applet.cpp  (Longpath)
// =================================================================
//
// Das Bandstapel-Applet (Entwurf A) und die Marken an der Skala
// (Entwurf D1), am echten RadioModel mit verbundenem Geraet. Gefahren
// wird die BEDIENUNG: echte QMouseEvents per sendEvent an die Liste und
// an das Spektrum (QTest::mouseClick verpufft ohne Schirm).
//
//   · der Stand in der Kopfleiste und die offene Gruppe des Bandes
//   · Klick auf eine Zeile fuehrt hin, Klick auf den Knopf legt ab
//   · Doppelklick benennt um, Escape verwirft
//   · Rechtsklick-Menue der Zeile: Sperren
//   · Klick auf eine Gruppe klappt auf und zu
//   · Marken: Treffer an der Skala, nichts beim Senden, nichts ohne Liste
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/BandStackApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackController.h"

using namespace Longpath;

namespace {

void klick(QWidget* w, const QPoint& pos, Qt::MouseButton knopf = Qt::LeftButton)
{
    const QPointF lokal(pos);
    const QPointF global(w->mapToGlobal(pos));
    QMouseEvent druck(QEvent::MouseButtonPress, lokal, global, knopf, knopf, Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, lokal, global, knopf, Qt::NoButton,
                    Qt::NoModifier);
    QApplication::sendEvent(w, &los);
}

void doppelklick(QWidget* w, const QPoint& pos)
{
    klick(w, pos);
    const QPointF lokal(pos);
    QMouseEvent dk(QEvent::MouseButtonDblClick, lokal, QPointF(w->mapToGlobal(pos)),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &dk);
}

struct Stand {
    std::unique_ptr<RadioModel> radio;
    SliceModel* scheibe{nullptr};
    BandStackController* stapel{nullptr};
    std::unique_ptr<BandStackApplet> applet;
    int id{-1};
};

Stand aufbauen()
{
    Stand s;
    s.radio = std::make_unique<RadioModel>();
    RadioInfo info;
    info.macAddress = QStringLiteral("02:00:00:00:BA:11");
    s.radio->setLastRadioInfoForTest(info);
    s.radio->addSlice();
    s.scheibe = s.radio->activeSlice();
    s.scheibe->setFrequency(14225000.0);
    s.scheibe->setDspMode(DSPMode::USB);
    s.radio->setConnectionStateForTest(ConnectionState::Connected);
    s.stapel = s.radio->bandStack();
    s.id = s.scheibe->sliceIndex();
    s.applet = std::make_unique<BandStackApplet>(s.radio.get());
    s.applet->kopfleistenWidget();
    s.applet->resize(320, 700);
    s.applet->show();
    QCoreApplication::processEvents();
    return s;
}

QString idBei(const Stand& s, Band band, qint64 hz)
{
    for (const BandStackEntry& e : s.stapel->store().entries(band)) {
        if (e.frequencyHz == hz) { return e.id; }
    }
    return {};
}

} // namespace

class TstBandStackApplet : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("Region"), QStringLiteral("Europe"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    void ohneGeraetSagtEsWarum()
    {
        RadioModel radio;
        radio.addSlice();
        BandStackApplet applet(&radio);
        applet.kopfleistenWidget();
        QVERIFY(applet.erklaerungForTest()->text().contains(QStringLiteral("Connect")));
        QVERIFY(applet.listeForTest()->isHidden());
        QVERIFY(applet.standForTest()->isHidden());
    }

    void standUndOffeneGruppe()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->press(s.id, Band::Band40m, BandStackDirection::Next, {}, nullptr));
        QCoreApplication::processEvents();
        QCOMPARE(s.applet->standForTest()->text(), QStringLiteral("40m 4/5"));
        BandStackListe* liste = s.applet->listeForTest();
        QVERIFY(liste->istOffen(Band::Band40m));
        QVERIFY(!liste->istOffen(Band::Band20m));
        QCOMPARE(liste->stelleForTest(), idBei(s, Band::Band40m, 7090000));
        QVERIFY(liste->zeileRectForTest(idBei(s, Band::Band40m, 7005000)).isValid());
        // Zeilen anderer Baender sind zugeklappt, also nicht da.
        QVERIFY(!liste->zeileRectForTest(idBei(s, Band::Band20m, 14155000)).isValid());
        // Die Rundfunkbaender stehen nicht in der Liste (Entwurf A), GEN
        // und WWV schon.
        QVERIFY(!liste->gruppeRectForTest(Band::Band31m).isValid());
        QVERIFY(liste->gruppeRectForTest(Band::WWV).isValid());
        QVERIFY(liste->gruppeRectForTest(Band::GEN).isValid());
    }

    void klickAufZeileFuehrtHin()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->press(s.id, Band::Band40m, BandStackDirection::Next, {}, nullptr));
        QCoreApplication::processEvents();
        BandStackListe* liste = s.applet->listeForTest();
        const QRect r = liste->zeileRectForTest(idBei(s, Band::Band40m, 7005000));
        QVERIFY(r.isValid());
        klick(liste, r.center());
        QCoreApplication::processEvents();
        QCOMPARE(qRound64(s.scheibe->frequency()), qint64(7005000));
        QCOMPARE(s.applet->standForTest()->text(), QStringLiteral("40m 1/5"));
    }

    void knopfLegtAb()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->press(s.id, Band::Band40m, BandStackDirection::Next, {}, nullptr));
        s.scheibe->setFrequency(7061000.0);
        QCoreApplication::processEvents();
        QSignalSpy hinweis(s.applet.get(), &BandStackApplet::hinweis);
        BandStackListe* liste = s.applet->listeForTest();
        const QRect k = liste->knopfRectForTest();
        QVERIFY(k.isValid());
        klick(liste, k.center());
        QCoreApplication::processEvents();
        QVERIFY(!idBei(s, Band::Band40m, 7061000).isEmpty());
        QCOMPARE(s.applet->standForTest()->text(), QStringLiteral("40m 4/6"));
        QCOMPARE(hinweis.size(), 1);
        QCOMPARE(hinweis.first().at(1).toBool(), false);
    }

    void doppelklickBenenntUm()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->press(s.id, Band::Band40m, BandStackDirection::Next, {}, nullptr));
        QCoreApplication::processEvents();
        BandStackListe* liste = s.applet->listeForTest();
        const QString id = idBei(s, Band::Band40m, 7100000);
        doppelklick(liste, liste->zeileRectForTest(id).center());
        QLineEdit* feld = s.applet->namensfeldForTest();
        QVERIFY(feld != nullptr);
        QVERIFY(feld->isVisible());
        feld->setText(QStringLiteral("Runde Sonntag"));
        QTest::keyClick(feld, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(s.stapel->store().entry(id)->name, QStringLiteral("Runde Sonntag"));
        QVERIFY(!feld->isVisible());

        // Escape verwirft.
        doppelklick(liste, liste->zeileRectForTest(id).center());
        feld->setText(QStringLiteral("Verworfen"));
        QTest::keyClick(feld, Qt::Key_Escape);
        QCoreApplication::processEvents();
        QCOMPARE(s.stapel->store().entry(id)->name, QStringLiteral("Runde Sonntag"));
    }

    void zeilenMenueSperrt()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->press(s.id, Band::Band40m, BandStackDirection::Next, {}, nullptr));
        const QString id = idBei(s, Band::Band40m, 7045000);
        std::unique_ptr<QMenu> menue(s.applet->baueZeilenMenue(id, nullptr));
        QAction* sperren = nullptr;
        for (QAction* a : menue->actions()) {
            if (a->text() == QStringLiteral("Lock")) { sperren = a; }
        }
        QVERIFY(sperren != nullptr);
        QVERIFY(sperren->isEnabled());
        sperren->trigger();
        QVERIFY(s.stapel->store().entry(id)->locked);
    }

    void gruppeKlapptAuf()
    {
        Stand s = aufbauen();
        QVERIFY(s.stapel->press(s.id, Band::Band40m, BandStackDirection::Next, {}, nullptr));
        QCoreApplication::processEvents();
        BandStackListe* liste = s.applet->listeForTest();
        const QRect g = liste->gruppeRectForTest(Band::Band20m);
        QVERIFY(g.isValid());
        klick(liste, g.center());
        QVERIFY(liste->istOffen(Band::Band20m));
        QVERIFY(liste->zeileRectForTest(idBei(s, Band::Band20m, 14155000)).isValid());
        klick(liste, liste->gruppeRectForTest(Band::Band20m).center());
        QVERIFY(!liste->istOffen(Band::Band20m));
    }

    // ── D1: Marken an der Skala ────────────────────────────────────────
    void markeAnDerSkalaWirdGetroffen()
    {
        SpectrumWidget sw;
        sw.resize(1000, 500);
        sw.setFrequencyRange(7100000.0, 200000.0);
        sw.setVfoFrequency(7150000.0);
        QVector<SpectrumWidget::BandStackMark> marken;
        marken.append({QStringLiteral("a"), 7050000, false, false, QStringLiteral("7.050 LSB")});
        marken.append({QStringLiteral("b"), 7090000, true, true, QStringLiteral("7.090 LSB")});
        sw.setBandStackMarks(marken);
        QCOMPARE(sw.bandStackMarks().size(), 2);

        // Die Marke b am unteren Rand des Spektrums suchen.
        QString treffer;
        QPoint wo;
        for (int y = sw.height() - 1; y > 0 && treffer.isEmpty(); --y) {
            for (int x = 0; x < sw.width() && treffer.isEmpty(); ++x) {
                if (sw.bandStackMarkAtForTest(x, y) == QStringLiteral("b")) {
                    treffer = QStringLiteral("b");
                    wo = QPoint(x, y);
                }
            }
        }
        QCOMPARE(treffer, QStringLiteral("b"));

        QSignalSpy geklickt(&sw, &SpectrumWidget::bandStackMarkClicked);
        klick(&sw, wo);
        QCOMPARE(geklickt.size(), 1);
        QCOMPARE(geklickt.first().at(0).toString(), QStringLiteral("b"));

        // Beim Senden: nicht anklickbar (und nicht gezeichnet).
        sw.setMoxOverlay(true);
        QVERIFY(sw.bandStackMarkAtForTest(wo.x(), wo.y()).isEmpty());
        sw.setMoxOverlay(false);
        QCOMPARE(sw.bandStackMarkAtForTest(wo.x(), wo.y()), QStringLiteral("b"));

        // Ohne Liste: nichts.
        sw.setBandStackMarks({});
        QVERIFY(sw.bandStackMarkAtForTest(wo.x(), wo.y()).isEmpty());
    }
};

QTEST_MAIN(TstBandStackApplet)
#include "tst_bandstack_applet.moc"
