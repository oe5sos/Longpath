// tests/tst_schwebende_folgen_dem_hauptfenster.cpp  (Longpath)
//
// Betreiber am 2026-11-04: "sobald ich das _ oder das + usw. links oben
// verwenden muss sich das ganze!!!! Fenster bewegen! auch die
// freischwebenden im verhältnis!"
//
// Die abgeloesten Applets, der schwebende Panadapter, Rotor/Log, die
// Seitenflaeche und seit heute das Contest-Logbuch sind Qt::Tool-Fenster
// mit dem Hauptfenster als Elternteil. Bewegt sich das Hauptfenster
// (Zug am Titelbalken, das "+" links oben, Vollbild), blieben sie bisher
// stehen, wo sie waren. Jetzt folgen sie: ein Zug verschiebt sie um
// denselben Betrag, eine Groessenaenderung bildet ihre Lage im Verhaeltnis
// zum alten Rahmen auf den neuen ab -- und seit 2026-11-11 auch ihre
// Groesse. Betreiber: „ich hatte es bewusst kleiner gemacht, das
// hauptfenster. wenn ich das mache, müssen alle anderen fenster auch im
// verhältnis kleiner werden."
//
// Alte Fassung: die Faelle 1 und 2 fallen um (das Fenster steht noch).
// Fassung vom 2026-11-04: Fall 2 und 2b fallen um (die Groesse blieb).
//
// Bewusst nicht abgeraeumt (wie tst_dock_ablage_nimmt_fenster_mit).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — Groesse im Verhaeltnis (Fall 2, neuer Fall 2b).
//                 Martin Fischer (OE5SOS), KI-gestuetzt mit Anthropic
//                 Claude Code.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QPushButton>
#include <QScreen>

#include "gui/MainWindow.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/AppletWidget.h"
#include "gui/WindowPlacement.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestSchwebendeFolgenDemHauptfenster : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};
    AppletFloatingWindow* m_abgeloest{nullptr};

    // Der Nachlauf des Hauptfensters (250 ms) plus Luft.
    static constexpr int kNachlaufMs = 700;
    // Nach dem Mitziehen rastet das schwebende Fenster auf Longpaths
    // Raster (16 px) ein -- bis zu einer halben Rasterweite Versatz sind
    // also das Raster, nicht ein Fehler (gemessen: (608,368) statt
    // (600,360)).
    static constexpr int kToleranzPx = kSnapGridPx / 2 + 2;

    AppletFloatingWindow* loeseEinApplet()
    {
        auto* panel = m_mw->findChild<AppletPanelWidget*>();
        if (!panel) { return nullptr; }
        QPushButton* detach = nullptr;
        for (GridCellWidget* c : panel->findChildren<GridCellWidget*>()) {
            if (!c->titleBar() || !c->titleBar()->isVisible()) { continue; }
            for (QPushButton* b : c->titleBar()->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("↗") && b->isVisible()) {
                    detach = b; break;
                }
            }
            if (detach) { break; }
        }
        if (!detach) { return nullptr; }
        const int vorher = m_mw->findChildren<AppletFloatingWindow*>().size();
        QTest::mouseClick(detach, Qt::LeftButton);
        QTest::qWait(200);
        const QList<AppletFloatingWindow*> wins =
            m_mw->findChildren<AppletFloatingWindow*>();
        if (wins.size() <= vorher) { return nullptr; }
        return wins.last();
    }

    // Das Hauptfenster in eine bekannte, NICHT maximierte Lage bringen
    // und warten, bis Raster und Nachlauf durch sind.
    void hauptfensterStellen(const QRect& r)
    {
        if (m_mw->isMaximized() || m_mw->isFullScreen()) { m_mw->showNormal(); }
        m_mw->setGeometry(r);
        QTest::qWait(kNachlaufMs);
    }

private slots:

    void initTestCase()
    {
        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);

        m_abgeloest = loeseEinApplet();
        QVERIFY2(m_abgeloest, "ein Applet muss sich abloesen lassen");
        QTRY_VERIFY_WITH_TIMEOUT(m_abgeloest->isVisible(), 3000);

        // Der Schirm der offscreen-Plattform misst von sich aus 800x800,
        // das Hauptfenster braucht mindestens 800x600: da stoesst jedes
        // schwebende Fenster an den Rand oder an die Bedienleisten, die
        // der Schutzrahmen freihaelt, und misst Falsches. Darum bekommt
        // DIESER Pruefstand in tests/CMakeLists.txt einen 1920x1200-Schirm
        // (tests/offscreen-grosser-schirm.json); hier wird das geprueft,
        // damit ein zu kleiner Schirm als solcher gemeldet wird und nicht
        // als "Fenster folgt nicht".
        const QRect schirm = m_mw->screen()->availableGeometry();
        QVERIFY2(schirm.width() >= 1600 && schirm.height() >= 1000,
                 qPrintable(QStringLiteral("Pruefschirm zu klein: %1x%2")
                                .arg(schirm.width()).arg(schirm.height())));
        hauptfensterStellen(QRect(64, 64, 1024, 640));
        // Das abgeloeste Fenster mitten INS Hauptfenster legen (weg von
        // den Bedienleisten), damit das Verhaeltnis eindeutig ist. Eigene
        // Bewegung: kein Ziehen.
        {
            const EigeneBewegung wirSelbst;
            m_abgeloest->move(m_mw->frameGeometry().topLeft() + QPoint(256, 160));
        }
        QTest::qWait(kNachlaufMs);
        QVERIFY(m_mw->schwebendeMitziehenForTest() == 0);
    }

    // ── Fall 1: ein Zug am Hauptfenster nimmt das Fenster mit ────────────
    void zugVerschiebtDasSchwebendeFensterMit()
    {
        const QRect hauptVorher = m_mw->frameGeometry();
        const QPoint vorher = m_abgeloest->frameGeometry().topLeft();

        m_mw->move(m_mw->pos() + QPoint(160, 96));
        QTest::qWait(kNachlaufMs);

        const QPoint hauptDelta =
            m_mw->frameGeometry().topLeft() - hauptVorher.topLeft();
        QVERIFY2(hauptDelta != QPoint(0, 0), "das Hauptfenster muss sich bewegt haben");
        const QPoint delta = m_abgeloest->frameGeometry().topLeft() - vorher;
        QVERIFY2(qAbs(delta.x() - hauptDelta.x()) <= kToleranzPx
                     && qAbs(delta.y() - hauptDelta.y()) <= kToleranzPx,
                 qPrintable(QStringLiteral(
                     "Das schwebende Fenster ist um (%1,%2) gewandert, das "
                     "Hauptfenster um (%3,%4) -- es muss mit")
                     .arg(delta.x()).arg(delta.y())
                     .arg(hauptDelta.x()).arg(hauptDelta.y())));
    }

    // ── Fall 2: eine Groessenaenderung haelt das Verhaeltnis ─────────────
    void groessenaenderungHaeltDasVerhaeltnis()
    {
        // Seit 2026-11-12 im Verhaeltnis der ARBEITSFLAECHE (ohne die
        // Leisten oben und unten, die nicht mitwachsen).
        const QMargins leisten = m_mw->leistenRaenderForTest();
        const QRect alt = m_mw->frameGeometry();
        const QRect altA = alt.marginsRemoved(leisten);
        const QRect f = m_abgeloest->frameGeometry();
        const double fx = double(f.x() - altA.x()) / altA.width();
        const double fy = double(f.y() - altA.y()) / altA.height();
        const QSize innen = m_abgeloest->size();

        m_mw->resize(alt.width() + 480, alt.height() + 160);
        QTest::qWait(kNachlaufMs);

        const QRect neu = m_mw->frameGeometry();
        const QRect neuA = neu.marginsRemoved(leisten);
        QVERIFY2(neu.size() != alt.size(), "das Hauptfenster muss gewachsen sein");
        const QRect g = m_abgeloest->frameGeometry();
        const int sollX = neuA.x() + qRound(fx * neuA.width());
        const int sollY = neuA.y() + qRound(fy * neuA.height());
        QVERIFY2(qAbs(g.x() - sollX) <= kToleranzPx
                     && qAbs(g.y() - sollY) <= kToleranzPx,
                 qPrintable(QStringLiteral(
                     "Lage (%1,%2), erwartet im Verhaeltnis (%3,%4)")
                     .arg(g.x()).arg(g.y()).arg(sollX).arg(sollY)));
        // Die Groesse waechst im selben Verhaeltnis (2026-11-11). Was
        // danach noch an ihr ruehrt, ist Longpaths Raster, das alle vier
        // Kanten des Rahmens einrasten laesst -- eine Rasterweite, wie
        // bei einem Zug von Hand auch.
        const QSize soll(qRound(innen.width() * double(neuA.width()) / altA.width()),
                         qRound(innen.height() * double(neuA.height()) / altA.height()));
        const QSize ist = m_abgeloest->size();
        QVERIFY2(qAbs(ist.width() - soll.width()) <= kSnapGridPx
                     && qAbs(ist.height() - soll.height()) <= kSnapGridPx,
                 qPrintable(QStringLiteral("Groesse %1x%2, erwartet %3x%4 (vorher %5x%6)")
                                .arg(ist.width()).arg(ist.height())
                                .arg(soll.width()).arg(soll.height())
                                .arg(innen.width()).arg(innen.height())));
    }

    // ── Fall 2b: verkleinert der Betreiber das Hauptfenster, werden die
    //    schwebenden Fenster im Verhaeltnis kleiner (2026-11-11) ───────────
    void verkleinernMachtSieKleiner()
    {
        const QMargins leisten = m_mw->leistenRaenderForTest();
        const QRect alt = m_mw->frameGeometry();
        const QRect altA = alt.marginsRemoved(leisten);
        const QSize innen = m_abgeloest->size();

        m_mw->resize(alt.width() - 360, alt.height() - 120);
        QTest::qWait(kNachlaufMs);

        const QRect neu = m_mw->frameGeometry();
        const QRect neuA = neu.marginsRemoved(leisten);
        QVERIFY2(neu.width() < alt.width(), "das Hauptfenster muss kleiner geworden sein");
        const QSize ist = m_abgeloest->size();
        const QSize soll = QSize(qRound(innen.width() * double(neuA.width()) / altA.width()),
                                 qRound(innen.height() * double(neuA.height()) / altA.height()))
                               .expandedTo(m_abgeloest->minimumSize());
        QVERIFY2(ist.width() < innen.width() || ist.width() == m_abgeloest->minimumWidth(),
                 qPrintable(QStringLiteral("Breite %1, vorher %2 -- nicht kleiner geworden")
                                .arg(ist.width()).arg(innen.width())));
        QVERIFY2(qAbs(ist.width() - soll.width()) <= kSnapGridPx
                     && qAbs(ist.height() - soll.height()) <= kSnapGridPx,
                 qPrintable(QStringLiteral("Groesse %1x%2, erwartet %3x%4 (vorher %5x%6)")
                                .arg(ist.width()).arg(ist.height())
                                .arg(soll.width()).arg(soll.height())
                                .arg(innen.width()).arg(innen.height())));
    }

    // ── Fall 3: eine Profilanwendung (eigene Bewegung) zieht NICHT ───────
    //
    // Mit einem Versatz NEBEN dem Raster (-93,-41): das Raster rueckt das
    // Hauptfenster danach um wenige Pixel nach, als eigene Bewegung im
    // laufenden Nachlauf -- genau der Weg, auf dem beim Aufbau am
    // 2026-11-04 fuenf Fenster gezogen wurden (Geraetelog 12:09:43,
    // "wieder irgendeine zusammensetzung der panels"). Ohne fremde
    // Bewegung davor darf das nichts ziehen.
    // 2026-11-12, Betreiber mit Bild: nach dem Verkleinern lag Combined
    // ueber den Profilknoepfen. Ein Fenster direkt unter der
    // Befehlsleiste bleibt beim Verkleinern unter ihr.
    void verkleinernSchiebtNichtsInDieLeiste()
    {
        const QMargins leisten = m_mw->leistenRaenderForTest();
        QVERIFY2(leisten.top() > 0, "ohne Leiste oben prueft der Fall nichts");
        const QRect alt = m_mw->frameGeometry();
        m_abgeloest->move(alt.x() + 300, alt.y() + leisten.top() + 4);
        QTest::qWait(kNachlaufMs);
        m_mw->schwebendeMitziehenForTest();   // Basis = jetzt

        m_mw->resize(alt.width() - 200, alt.height() - 160);
        QTest::qWait(kNachlaufMs);

        const QRect neu = m_mw->frameGeometry();
        QVERIFY2(neu.height() < alt.height(), "das Hauptfenster muss niedriger geworden sein");
        const int unterDerLeiste = neu.y() + leisten.top();
        const QRect g = m_abgeloest->frameGeometry();
        qInfo() << "Leisten" << leisten << "Hauptfenster" << neu << "Fenster" << g;
        QVERIFY2(g.y() >= unterDerLeiste - kToleranzPx,
                 qPrintable(QStringLiteral("Fenster oben bei %1, Leiste endet bei %2")
                                .arg(g.y()).arg(unterDerLeiste)));
    }

    // 2026-11-12, Betreiber: „nicht alle windows werden automatisch
    // angepasst". Jeder Zug rechnete vom LETZTEN Stand weiter: hing ein
    // Fenster beim Verkleinern an seiner Untergrenze, kam es beim
    // Vergroessern nicht mehr auf seine alte Groesse zurueck. Jetzt gilt
    // je Fenster eine feste Grundlage (Anteile der Arbeitsflaeche).
    void kleinUndWiederGrossGibtDieAlteLage()
    {
        const QRect alt = m_mw->frameGeometry();
        const QSize hauptInnen = m_mw->size();
        const QRect vorher = m_abgeloest->frameGeometry();
        const QSize innenVorher = m_abgeloest->size();
        const int minAlt = m_abgeloest->minimumWidth();
        // Die Untergrenze erzwingen: kleiner als jetzt darf es nicht.
        m_abgeloest->setMinimumWidth(innenVorher.width());
        m_mw->schwebendeMitziehenForTest();   // Basis = jetzt

        m_mw->resize(hauptInnen.width() - 400, hauptInnen.height() - 100);
        QTest::qWait(kNachlaufMs);
        QCOMPARE(m_abgeloest->width(), innenVorher.width());   // hing an der Grenze

        m_mw->resize(hauptInnen);
        QTest::qWait(kNachlaufMs);
        m_abgeloest->setMinimumWidth(minAlt);

        const QRect nachher = m_abgeloest->frameGeometry();
        qInfo() << "Hauptfenster" << alt << "->" << m_mw->frameGeometry()
                << "Fenster vorher" << vorher << "nachher" << nachher;
        QVERIFY2(qAbs(m_mw->width() - hauptInnen.width()) <= kSnapGridPx
                     && qAbs(m_mw->height() - hauptInnen.height()) <= kSnapGridPx,
                 "das Hauptfenster muss wieder seine alte Groesse haben");
        QVERIFY2(qAbs(nachher.width() - vorher.width()) <= kSnapGridPx
                     && qAbs(nachher.height() - vorher.height()) <= kSnapGridPx
                     && qAbs(nachher.x() - vorher.x()) <= kToleranzPx
                     && qAbs(nachher.y() - vorher.y()) <= kToleranzPx,
                 qPrintable(QStringLiteral("vorher %1,%2 %3x%4 -- nachher %5,%6 %7x%8")
                     .arg(vorher.x()).arg(vorher.y()).arg(vorher.width()).arg(vorher.height())
                     .arg(nachher.x()).arg(nachher.y()).arg(nachher.width()).arg(nachher.height())));
    }

    void eigeneBewegungZiehtNicht()
    {
        const QPoint vorher = m_abgeloest->frameGeometry().topLeft();
        {
            const EigeneBewegung wirSelbst;
            m_mw->move(m_mw->pos() + QPoint(-93, -41));
            QCoreApplication::processEvents();
        }
        QTest::qWait(kNachlaufMs);
        // Das Raster darf es um weniger als eine Rasterweite ruecken,
        // das Mitziehen haette es um rund (93,41) bewegt.
        const QPoint delta = m_abgeloest->frameGeometry().topLeft() - vorher;
        QVERIFY2(qAbs(delta.x()) < kSnapGridPx && qAbs(delta.y()) < kSnapGridPx,
                 qPrintable(QStringLiteral("eigene Bewegung zog mit: (%1,%2)")
                                .arg(delta.x()).arg(delta.y())));
    }
};

QTEST_MAIN(TestSchwebendeFolgenDemHauptfenster)
#include "tst_schwebende_folgen_dem_hauptfenster.moc"
