// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_real_menus_hang_on_the_window.cpp  (Longpath)
// =================================================================
// Die Menues der nativen Felder haengen am Fenster -- im ECHTEN
// Hauptfenster.
//
// Qt macht die Geschwister eines nativen Widgets ebenfalls nativ. Im
// Standardlayout sind so Panadapter, Panadapterapplet, Behaelter der
// Applet-Spalte, Titelzeile u. a. nativ. Ein QMenu, dessen naechster
// nativer Vorfahr kein Fenster ist, bekommt von Qt keinen Fensterbezug:
// „QWidgetWindow(..., name="<Klasse>ClassWindow") must be a top level
// window." (so in Martins Protokoll fuer ProfileRail, Rotor/Log und die
// Befehlsleiste, #136/#142/#143). Nachgebaute Einzelwidgets taugen dafuer
// nicht: welches Feld nativ wird, entscheidet das Layout des Fensters.
//
// Geprueft werden die Menues, die am 2026-09-30 umgehaengt wurden --
// jeweils ueber den echten Ausloeser, ohne Warnung, und mit dem Fenster
// als Elternteil. Bei 13 davon ist die Farbe gleich geblieben (Klon am
// alten Feld gegen Klon am Fenster: 0 abweichende Pixel). Bei acht --
// Applet-Titelzeile, Frequenz, Instrument (zweimal), Endstufe, Tuner,
// RF2K-S, Rotor-Rose -- werden sie heller: statt des geerbten #08080a
// das Menuegrau #1a1a1e. Martin, 2026-09-30, am Vorher/Nachher-Blatt:
// „hell wie alle anderen".
//
// Modification history (Longpath):
//   2026-09-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================
#include <QtTest>
#include <QApplication>
#include <QContextMenuEvent>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabBar>
#include <QTimer>

#include <functional>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AmpApplet.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/FrequencyApplet.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/InstrumentApplet.h"
#include "gui/applets/QsoRecorderApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/TunerApplet.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/widgets/GlobeWidget.h"
#include "gui/widgets/RotorDialWidget.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

QRegularExpression notTopLevel()
{
    return QRegularExpression(QStringLiteral("must be a top level window"));
}

struct Opened {
    QWidget*    parent{nullptr};
    QStringList entries;
    bool        seen{false};
};

// Oeffnet ueber `trigger` ein Menue -- exec() oder popup() -- und schliesst
// es, sobald es offen ist.
Opened openMenu(const std::function<void()>& trigger)
{
    Opened o;
    QTimer::singleShot(200, qApp, [&o]() {
        if (auto* m = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            o.parent = m->parentWidget();
            for (QAction* a : m->actions()) { o.entries << a->text(); }
            o.seen = true;
            m->close();
        }
    });
    trigger();
    QTest::qWaitFor([&o]() { return o.seen; }, 2000);
    QTest::qWait(50);
    return o;
}

void contextAt(QWidget* w, const QPoint& p)
{
    QContextMenuEvent ev(QContextMenuEvent::Mouse, p, w->mapToGlobal(p));
    QApplication::sendEvent(w, &ev);
}

// Die Klickfelder der Spots entstehen erst beim Malen -- derselbe Weg
// wie in tst_spot_double_click.
void renderSpots(SpectrumWidget* sw, const QRect& specRect)
{
    QImage img(specRect.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    p.translate(-specRect.topLeft());
    sw->drawSpotMarkersForTest(p, specRect);
    p.end();
}

SpectrumWidget::SpotMarker spot(int idx, const QString& call, double freqMhz)
{
    SpectrumWidget::SpotMarker m;
    m.index    = idx;
    m.callsign = call;
    m.freqMhz  = freqMhz;
    m.source   = QStringLiteral("DXCluster");
    return m;
}

} // namespace

class TstRealMenusHangOnTheWindow : public QObject
{
    Q_OBJECT

    MainWindow* m_mw{nullptr};

private slots:
    void initTestCase()
    {
        // Das Rotor/Log-Feld liest RotorLogbookPanel::logbookPath(); eine
        // gesetzte Sandbox-Variable lenkte das auf einen echten Ordner.
        qunsetenv("LONGPATH_CONFIG_DIR");
        m_mw = new MainWindow();
        m_mw->resize(1800, 1100);
        m_mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mw));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(400);
    }

    void cleanupTestCase()
    {
        // close() vor delete: das Anhalten der Arbeitsfaeden steht im
        // closeEvent (siehe tst_real_notch_rightclick).
        m_mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        delete m_mw;
    }

    void thePanadapterMenusHangOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        auto* pan = m_mw->findChild<SpectrumWidget*>();
        QVERIFY(pan);
        QVERIFY2(pan->windowHandle(), "der Panadapter ist nicht mehr nativ -- dann prueft das hier nichts");
        auto* radio = m_mw->findChild<RadioModel*>();
        QVERIFY(radio && radio->notchModel());

        // Notch: Rechtsklick auf den Balken
        NotchModel* notches = radio->notchModel();
        const int id = notches->addNotch(pan->centerFrequency(), 400.0);
        QTest::qWait(120);
        int x = -1;
        for (int probe = 0; probe < pan->width(); ++probe) {
            if (pan->notchAtPixelForTest(probe) == id) { x = probe; break; }
        }
        QVERIFY(x >= 0);
        Opened o = openMenu([&]() {
            QTest::mouseClick(pan, Qt::RightButton, Qt::NoModifier, QPoint(x, 60));
        });
        QVERIFY(o.entries.contains(QStringLiteral("Remove Notch")));
        QCOMPARE(o.parent, pan->window());
        notches->removeNotch(id);

        // Spot: Rechtsklick aufs Etikett (nach dem Wiederholschutz)
        QTest::qWait(400);
        pan->setShowSpots(true);
        const double c = pan->centerFrequency() / 1.0e6;
        pan->setSpotMarkers({spot(1, QStringLiteral("DL1ABC"), c + 0.010)});
        const QRect spec(0, 0, pan->width(), 150);
        renderSpots(pan, spec);
        QVERIFY(!pan->spotClickRectsForTest().isEmpty());
        const QPoint label = pan->spotClickRectsForTest().first().rect.center();
        o = openMenu([&]() {
            QTest::mouseClick(pan, Qt::RightButton, Qt::NoModifier, label);
        });
        QVERIFY(o.entries.contains(QStringLiteral("Tune to DL1ABC")));
        QCOMPARE(o.parent, pan->window());

        // Sammelabzeichen „+N": Linksklick
        QTest::qWait(400);
        pan->setSpotMaxLevels(2);
        QVector<SpectrumWidget::SpotMarker> many;
        for (int i = 0; i < 8; ++i) {
            many << spot(i + 1, QStringLiteral("OVR%1").arg(i + 1), c + 0.010 + i * 0.0002);
        }
        pan->setSpotMarkers(many);
        renderSpots(pan, spec);
        QVERIFY(!pan->spotClustersForTest().isEmpty());
        const QPoint badge = pan->spotClustersForTest().first().rect.center();
        o = openMenu([&]() {
            QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, badge);
        });
        QVERIFY(!o.entries.isEmpty() && o.entries.first().startsWith(QStringLiteral("OVR")));
        QCOMPARE(o.parent, pan->window());
        pan->setSpotMarkers({});
        pan->setShowSpots(false);

        // Panadapterapplet: Rechtsklick und Zahnrad
        auto* applet = m_mw->findChild<PanadapterApplet*>();
        QVERIFY(applet);
        o = openMenu([&]() { contextAt(applet, QPoint(30, 30)); });
        QVERIFY(o.entries.contains(QStringLiteral("Float this pan")));
        QCOMPARE(o.parent, applet->window());

        QPushButton* gear = nullptr;
        for (QPushButton* b : applet->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("⚙")) { gear = b; }
        }
        QVERIFY(gear);
        o = openMenu([&]() { gear->click(); });
        QVERIFY(!o.entries.isEmpty());
        QCOMPARE(o.parent, applet->window());
    }

    void theOverlayOverflowMenuHangsOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        auto* overlay = m_mw->findChild<SpectrumOverlayPanel*>();
        QVERIFY(overlay);
        // Flach gemacht, damit Gruppen hinter das „…" rutschen.
        m_mw->resize(1800, 420);
        QTest::qWait(400);
        QPushButton* more = nullptr;
        for (QPushButton* b : overlay->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("…") && b->isVisible()) { more = b; }
        }
        QVERIFY2(more, "kein „…“ -- die Flaeche war hoch genug fuer alle Gruppen");
        const Opened o = openMenu([&]() { more->click(); });
        QVERIFY(!o.entries.isEmpty());
        QCOMPARE(o.parent, overlay->window());
        m_mw->resize(1800, 1100);
        QTest::qWait(300);
    }

    void theContainerAndSpeakerMenusHangOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        auto* panel = m_mw->findChild<AppletPanelWidget*>();
        QVERIFY(panel);
        ContainerWidget* cont = nullptr;
        for (QWidget* a = panel; a; a = a->parentWidget()) {
            if (auto* c = qobject_cast<ContainerWidget*>(a)) { cont = c; break; }
        }
        QVERIFY(cont);
        Opened o = openMenu([&]() { contextAt(cont, QPoint(10, 10)); });
        QVERIFY(o.entries.contains(QStringLiteral("Own window")));
        QCOMPARE(o.parent, cont->window());

        // Reiterleiste: erst ab zwei Reitern da
        auto* extra = new QWidget;
        cont->addTab(extra, QStringLiteral("Zweiter"));
        QTest::qWait(100);
        QTabBar* tabs = cont->findChild<QTabBar*>();
        QVERIFY(tabs && tabs->count() >= 2);
        const QPoint onTab = tabs->tabRect(0).center();
        o = openMenu([&]() { emit tabs->customContextMenuRequested(onTab); });
        QVERIFY(!o.entries.isEmpty() && o.entries.first().endsWith(QStringLiteral("als eigenes Fenster")));
        QCOMPARE(o.parent, cont->window());

        auto* master = m_mw->findChild<MasterOutputWidget*>();
        QVERIFY(master);
        QPushButton* speaker = nullptr;
        for (QPushButton* b : master->findChildren<QPushButton*>()) {
            if (b->contextMenuPolicy() == Qt::CustomContextMenu) { speaker = b; }
        }
        QVERIFY(speaker);
        o = openMenu([&]() { emit speaker->customContextMenuRequested(QPoint(4, 4)); });
        QVERIFY(o.seen);
        QCOMPARE(o.parent, master->window());
    }

    void theAppletColumnMenusHangOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        auto* rx = m_mw->findChild<RxApplet*>();
        QVERIFY(rx);
        // Mit Alex, sonst bleibt das Antennenmenue leer und geht nicht auf.
        rx->setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::OrionMKII));
        rx->setHpsdrSku(HPSDRModel::ANAN_G2);
        for (const char* name : {"m_rxAntBtn", "m_txAntBtn"}) {
            auto* b = rx->findChild<QPushButton*>(QString::fromLatin1(name));
            QVERIFY(b);
            const Opened o = openMenu([&]() { b->click(); });
            QVERIFY2(o.entries.contains(QStringLiteral("ANT1")), name);
            QCOMPARE(o.parent, rx->window());
        }

        auto* rec = m_mw->findChild<QsoRecorderApplet*>();
        QVERIFY(rec);
        auto* list = rec->findChild<QListWidget*>();
        QVERIFY(list);
        auto* item = new QListWidgetItem(QStringLiteral("probe.wav"), list);
        item->setData(Qt::UserRole, QStringLiteral("/nonexistent/probe.wav"));
        list->show();
        QTest::qWait(100);
        const QPoint at = list->visualItemRect(item).center();
        QCOMPARE(list->itemAt(at), item);
        const Opened o = openMenu([&]() { emit list->customContextMenuRequested(at); });
        QVERIFY(o.entries.contains(QStringLiteral("Delete recording")));
        QCOMPARE(o.parent, rec->window());
        delete item;
    }

    // Die acht, die dabei heller werden (Martin: „hell wie alle anderen").
    void theLighterAppletMenusHangOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        auto* rx = m_mw->findChild<RxApplet*>();
        QVERIFY(rx);
        auto* cell = qobject_cast<GridCellWidget*>(rx->parentWidget());
        QVERIFY(cell && cell->titleBar());
        Opened o = openMenu([&]() { contextAt(cell->titleBar(), QPoint(10, 5)); });
        QVERIFY(o.entries.contains(QStringLiteral("Als Fenster ablösen")));
        QCOMPARE(o.parent, rx->window());

        auto* freq = m_mw->findChild<FrequencyApplet*>();
        QVERIFY(freq);
        o = openMenu([&]() { contextAt(freq, QPoint(10, 10)); });
        QVERIFY(o.entries.contains(QStringLiteral("A/B-Zeile anzeigen")));
        QCOMPARE(o.parent, freq->window());

        auto* instr = m_mw->findChild<InstrumentApplet*>();
        QVERIFY(instr);
        o = openMenu([&]() { contextAt(instr, QPoint(10, 10)); });
        QVERIFY(o.entries.contains(QStringLiteral("Quelle")));
        QCOMPARE(o.parent, instr->window());
        o = openMenu([&]() { instr->openExtendedSettings(); });
        QVERIFY(o.entries.contains(QStringLiteral("Quelle")));
        QCOMPARE(o.parent, instr->window());

        // Ohne Geraet ausgeblendet, das Menue geht trotzdem auf.
        auto* amp = m_mw->findChild<AmpApplet*>();
        QVERIFY(amp);
        o = openMenu([&]() { contextAt(amp, QPoint(10, 10)); });
        QVERIFY(o.entries.contains(QStringLiteral("Open PGXL Advanced...")));
        QCOMPARE(o.parent, amp->window());

        auto* tuner = m_mw->findChild<TunerApplet*>();
        QVERIFY(tuner);
        o = openMenu([&]() { contextAt(tuner, QPoint(10, 10)); });
        QVERIFY(o.entries.contains(QStringLiteral("Open TGXL Advanced...")));
        QCOMPARE(o.parent, tuner->window());

        auto* rf = m_mw->findChild<Rf2ksApplet*>();
        QVERIFY(rf);
        o = openMenu([&]() { contextAt(rf, QPoint(10, 10)); });
        QVERIFY(o.entries.contains(QStringLiteral("Open RF-Kit Advanced...")));
        QCOMPARE(o.parent, rf->window());
    }

    void theRotorFieldMenusHangOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        QMetaObject::invokeMethod(m_mw, "detachRotorPanel");
        QTest::qWait(400);
        auto* rose = m_mw->findChild<RotorDialWidget*>();
        QVERIFY(rose);
        Opened o = openMenu([&]() { contextAt(rose, QPoint(20, 20)); });
        QVERIFY(o.entries.contains(QStringLiteral("Vollkreis")));
        QCOMPARE(o.parent, rose->window());

        auto* globe = m_mw->findChild<GlobeWidget*>();
        QVERIFY(globe);
        o = openMenu([&]() { contextAt(globe, QPoint(20, 20)); });
        QVERIFY(o.entries.contains(QStringLiteral("Reset view")));
        QCOMPARE(o.parent, globe->window());
    }
};

QTEST_MAIN(TstRealMenusHangOnTheWindow)
#include "tst_real_menus_hang_on_the_window.moc"
