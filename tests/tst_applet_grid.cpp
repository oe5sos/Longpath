// =================================================================
// tests/tst_applet_grid.cpp  (NereusSDR)
// =================================================================
//
// Schritt 1 des freien Rasters. Entwurf:
// docs/architecture/2026-08-17-freies-raster-vorschlag.md.
//
// Der Schritt ist mit Absicht der, an dem sich SICHTBAR NICHTS aendert:
// aus dem Stapel aus Huellen in einem QVBoxLayout wird ein Raster mit
// einer Spalte. Solange es eine Spalte gibt, IST die Zeile der Index.
// Der Umbau von „Reihenfolge" auf „Ort" findet damit allein statt,
// bevor Spalten, Spannweiten und Density darauf aufbauen.
//
// Was ein Test dazu leisten muss, ist deshalb nicht „das Raster
// funktioniert", sondern: dieselbe Reihenfolge wie vorher, dasselbe
// Bild, und das Datenmodell traegt bereits, was die spaeteren Schritte
// brauchen — mehrere Inhalte je Feld.
//
// ── Warum die Liste schon jetzt geprueft wird ────────────────────────
//
// Festlegung des Betreibers, 2026-08-18: „Ein Feld im Raster ist ein
// Behälter, kein Widget … Baust du das Feld erst als Einzel-Widget und
// rüstest die Liste später nach, muss die gespeicherte Anordnung
// zweimal wandern — und der Kennungs-Fehler von heute Abend hat
// gezeigt, was eine Anordnungswanderung still verlieren kann."
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QCoreApplication>
#include <QResizeEvent>
#include <QSet>
#include <QGridLayout>

#include <QLabel>

#include "gui/applets/AppletGrid.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletWidget.h"
#include "gui/applets/GridCell.h"
#include "gui/applets/GridCellWidget.h"

using namespace NereusSDR;

namespace {

class StubApplet : public AppletWidget {
public:
    explicit StubApplet(QString id)
        : AppletWidget(nullptr), m_id(std::move(id)) {}
    QString appletId() const override    { return m_id; }
    QString appletTitle() const override { return m_id.toUpper(); }
    void syncFromModel() override {}
private:
    QString m_id;
};

QStringList idsOf(const QList<AppletWidget*>& list)
{
    QStringList out;
    for (AppletWidget* a : list) { if (a) { out << a->appletId(); } }
    return out;
}

} // namespace


namespace {
/// Groesse setzen UND das Ereignis zustellen.
///
/// `resize()` allein genuegt nicht: bei einem nicht gezeigten
/// Fenster haelt Qt6 das QResizeEvent bis zum Anzeigen zurueck. Der
/// Test lief dadurch gegen einen Handler, der nie gerufen wurde —
/// und meldete „1 statt 2", was wie ein Fehler in der
/// Schwellenrechnung aussah und keiner war.
void resizeAndDeliver(QWidget& w, int width, int height)
{
    const QSize before = w.size();
    w.resize(width, height);
    QResizeEvent ev(w.size(), before);
    QCoreApplication::sendEvent(&w, &ev);
}
} // namespace

class TestAppletGrid : public QObject
{
    Q_OBJECT

private slots:

    // ── Ein Feld ist ein Behaelter ───────────────────────────────────

    void aCellHoldsAListNotASingleWidget()
    {
        GridCellWidget cell(QStringLiteral("cell1"));
        auto* a = new StubApplet(QStringLiteral("Rx"));
        auto* b = new StubApplet(QStringLiteral("Swr"));

        cell.addWidget(a);
        cell.addWidget(b);

        QCOMPARE(cell.widgets().size(), 2);
        QCOMPARE(idsOf(cell.applets()),
                 QStringList({"Rx", "Swr"}));
        QVERIFY(cell.contains(a));
        QVERIFY(cell.contains(b));
    }

    // Der Titel kommt vom einen Inhalt, solange es einer ist. Bei
    // mehreren waere jede Wahl willkuerlich.
    void oneContentLendsItsTitleToTheCell()
    {
        GridCellWidget cell(QStringLiteral("cell1"));
        auto* a = new StubApplet(QStringLiteral("Rx"));
        cell.addWidget(a);

        const QList<QLabel*> labels = cell.findChildren<QLabel*>();
        bool found = false;
        for (QLabel* l : labels) {
            if (l->text() == QStringLiteral("RX")) { found = true; }
        }
        QVERIFY2(found, "der Titel des einen Inhalts steht nicht in der "
                        "Kopfleiste");

        cell.setTitle(QStringLiteral("EIGENER NAME"));
        found = false;
        for (QLabel* l : cell.findChildren<QLabel*>()) {
            if (l->text() == QStringLiteral("EIGENER NAME")) { found = true; }
        }
        QVERIFY2(found, "ein gesetzter Titel muss den geliehenen schlagen");
    }

    // Aushaengen heisst NICHT loeschen. Darauf beruht das Abloesen in
    // ein eigenes Fenster und das Umhaengen zwischen Feldern.
    void removingAWidgetLeavesItAlive()
    {
        auto* a = new StubApplet(QStringLiteral("Rx"));
        QPointer<AppletWidget> guard(a);
        {
            GridCellWidget cell(QStringLiteral("cell1"));
            cell.addWidget(a);
            cell.removeWidget(a);
            QVERIFY(cell.isEmpty());
            QVERIFY(a->parentWidget() == nullptr);
        }
        QVERIFY2(!guard.isNull(),
                 "das Widget ist mit dem Feld gestorben");
        delete a;
    }

    // ── Das Raster ───────────────────────────────────────────────────

    void oneContentPerCellInStepOne()
    {
        AppletGrid grid;
        auto* a = new StubApplet(QStringLiteral("Rx"));
        auto* b = new StubApplet(QStringLiteral("Tx"));
        grid.appendInOwnCell(a);
        grid.appendInOwnCell(b);

        QCOMPARE(grid.cells().size(), 2);
        QCOMPARE(grid.columns(), 1);
        QCOMPARE(idsOf(grid.applets()), QStringList({"Rx", "Tx"}));
    }

    // Mit einer Spalte IST die Zeile der Index — das ist der Grund,
    // warum sich sichtbar nichts aendert.
    void withOneColumnTheRowIsTheIndex()
    {
        AppletGrid grid;
        for (const QString& id : {QStringLiteral("Rx"),
                                  QStringLiteral("Tx"),
                                  QStringLiteral("Vax")}) {
            grid.appendInOwnCell(new StubApplet(id));
        }
        const QList<GridCell> arr = grid.arrangement();
        QCOMPARE(arr.size(), 3);
        for (int i = 0; i < arr.size(); ++i) {
            QCOMPARE(arr.at(i).row, i);
            QCOMPARE(arr.at(i).col, 0);
            QCOMPARE(arr.at(i).colSpan, 1);
        }
    }

    void anEmptyCellDoesNotLingerAsAHole()
    {
        AppletGrid grid;
        auto* a = new StubApplet(QStringLiteral("Rx"));
        grid.appendInOwnCell(a);
        QCOMPARE(grid.cells().size(), 1);

        grid.takeWidget(a);
        QCOMPARE(grid.cells().size(), 0);
        QVERIFY(a->parentWidget() == nullptr);
        delete a;
    }

    void movingACellMovesItsContentWithIt()
    {
        AppletGrid grid;
        auto* a = new StubApplet(QStringLiteral("Rx"));
        auto* b = new StubApplet(QStringLiteral("Tx"));
        auto* c = new StubApplet(QStringLiteral("Vax"));
        grid.appendInOwnCell(a);
        grid.appendInOwnCell(b);
        grid.appendInOwnCell(c);

        GridCellWidget* cellOfA = grid.cellFor(a);
        QVERIFY(cellOfA);
        grid.moveCell(cellOfA->cellId(), 2);

        QCOMPARE(idsOf(grid.applets()), QStringList({"Tx", "Vax", "Rx"}));
    }

    // ── Die Anordnung als Datensatz ──────────────────────────────────

    void theArrangementRoundTripsThroughAVariant()
    {
        GridCell c;
        c.id = QStringLiteral("cell7");
        c.row = 2; c.col = 1;
        c.rowSpan = 1; c.colSpan = 3;
        c.applets = QStringList({"Swr", "SignalInstrument"});
        c.title = QStringLiteral("MESSWERKE");
        c.locked = true;

        const GridCell back =
            GridCell::fromVariant(c.id, c.toVariant());
        QCOMPARE(back.id, c.id);
        QCOMPARE(back.row, c.row);
        QCOMPARE(back.col, c.col);
        QCOMPARE(back.colSpan, c.colSpan);
        QCOMPARE(back.applets, c.applets);
        QCOMPARE(back.title, c.title);
        QCOMPARE(back.locked, c.locked);
    }

    // Eine Aufnahme ohne Spannweite meint ein einfaches Feld. Null
    // waere unsichtbar, und der Bediener suchte danach.
    void aMissingSpanMeansOneNotZero()
    {
        const GridCell c =
            GridCell::fromVariant(QStringLiteral("cell1"), QVariantMap{});
        QCOMPARE(c.rowSpan, 1);
        QCOMPARE(c.colSpan, 1);
    }

    // ── Und das Panel darueber ───────────────────────────────────────
    //
    // Die eigentliche Zusicherung von Schritt 1: dieselbe Reihenfolge
    // wie vorher. Was hier durchfaellt, hat der Bediener am naechsten
    // Start als umsortierte Spalte vor sich.

    void thePanelKeepsTheOrderItIsGiven()
    {
        AppletPanelWidget panel;
        auto* rx  = new StubApplet(QStringLiteral("Rx"));
        auto* tx  = new StubApplet(QStringLiteral("Tx"));
        auto* vax = new StubApplet(QStringLiteral("Vax"));
        panel.addApplet(rx);
        panel.addApplet(tx);
        panel.addApplet(vax);

        QCOMPARE(idsOf(panel.applets()), QStringList({"Rx", "Tx", "Vax"}));
        QCOMPARE(panel.appletPosition(tx), 1);

        panel.setAppletOrder({vax, rx, tx});
        QCOMPARE(idsOf(panel.applets()), QStringList({"Vax", "Rx", "Tx"}));
        QCOMPARE(panel.appletPosition(rx), 1);
    }

    void movingAnAppletInThePanelMovesItInTheModelToo()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("Rx"));
        auto* tx = new StubApplet(QStringLiteral("Tx"));
        auto* vx = new StubApplet(QStringLiteral("Vax"));
        panel.addApplet(rx);
        panel.addApplet(tx);
        panel.addApplet(vx);

        QVERIFY(panel.moveApplet(rx, 2));
        // applets() ist das, was gespeichert wird. Liefe es gegen das
        // Bild, kaeme nach dem Neustart eine andere Anordnung heraus
        // als die, die man hinterlassen hat.
        QCOMPARE(idsOf(panel.applets()), QStringList({"Tx", "Vax", "Rx"}));
        QCOMPARE(panel.appletPosition(rx), 2);
    }

    void removingAnAppletFromThePanelLeavesItAlive()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("Rx"));
        QPointer<AppletWidget> guard(rx);
        panel.addApplet(rx);
        panel.removeApplet(rx);

        QVERIFY(panel.applets().isEmpty());
        QVERIFY2(!guard.isNull(),
                 "removeApplet muss aushaengen, nicht loeschen — darauf "
                 "beruht das Abloesen in ein eigenes Fenster");
        delete rx;
    }

    // ── Schritt 2: Spalten nach Breite ───────────────────────────────
    //
    // Die Schwellen sind eine Festlegung des Betreibers (1100 / 1600),
    // keine Herleitung. Der Test NENNT sie darum als Zahlen und rechnet
    // sie nicht nach — sonst prueft er nur, dass zwei Formeln
    // uebereinstimmen, und nicht, dass die Festlegung eingehalten ist.
    void columnsFollowWidthAtTheAgreedThresholds()
    {
        QCOMPARE(AppletGrid::columnsForWidth(0),    1);
        QCOMPARE(AppletGrid::columnsForWidth(1099), 1);
        QCOMPARE(AppletGrid::columnsForWidth(1100), 2);   // Schwelle
        QCOMPARE(AppletGrid::columnsForWidth(1599), 2);
        QCOMPARE(AppletGrid::columnsForWidth(1600), 3);   // Schwelle
        QCOMPARE(AppletGrid::columnsForWidth(4000), 3);
    }

    void resizingChangesTheColumnCount()
    {
        AppletGrid g;
        resizeAndDeliver(g, 900, 600);
        QCOMPARE(g.columns(), 1);
        resizeAndDeliver(g, 1200, 600);
        QCOMPARE(g.columns(), 2);
        resizeAndDeliver(g, 1700, 600);
        QCOMPARE(g.columns(), 3);
        resizeAndDeliver(g, 1000, 600);
        QCOMPARE(g.columns(), 1);
    }

    // Die Antwort des Betreibers auf Entwurf §4 Frage 3: nein, die
    // Automatik ueberschreibt keine ausdrueckliche Wahl. Ohne diese
    // Sperre waere eine Wahl bis zur naechsten
    // Fenstergroessenaenderung gueltig, also praktisch nie.
    void anExplicitChoiceSurvivesResizing()
    {
        AppletGrid g;
        resizeAndDeliver(g, 1700, 600);
        QCOMPARE(g.columns(), 3);
        g.setColumns(1);
        QVERIFY(g.columnsAreExplicit());
        resizeAndDeliver(g, 1200, 600);
        QCOMPARE(g.columns(), 1);      // NICHT 2
        resizeAndDeliver(g, 1800, 600);
        QCOMPARE(g.columns(), 1);      // auch nicht 3
        g.clearExplicitColumns();      // zurueck an die Automatik
        QVERIFY(!g.columnsAreExplicit());
        QCOMPARE(g.columns(), 3);
    }

    // Nichts faellt heraus und nichts ueberlappt: bei fuenf Feldern und
    // drei Spalten muessen genau fuenf Plaetze belegt sein, jeder
    // einmal. Das ist die Pruefbarkeit, die der Entwurf fuer Schritt 2
    // verlangt.
    void nothingOverlapsAndNothingFallsOut()
    {
        AppletGrid g;
        for (int i = 0; i < 5; ++i) { g.addCell(); }
        resizeAndDeliver(g, 1700, 600);
        QCOMPARE(g.columns(), 3);

        auto* lay = qobject_cast<QGridLayout*>(g.layout());
        QVERIFY(lay);
        QSet<QPair<int,int>> seen;
        int found = 0;
        for (int i = 0; i < lay->count(); ++i) {
            int r = 0, c = 0, rs = 0, cs = 0;
            lay->getItemPosition(i, &r, &c, &rs, &cs);
            QVERIFY2(!seen.contains(qMakePair(r, c)),
                     "zwei Felder auf demselben Platz");
            seen.insert(qMakePair(r, c));
            ++found;
        }
        QCOMPARE(found, 5);
    }
};

QTEST_MAIN(TestAppletGrid)
#include "tst_applet_grid.moc"
