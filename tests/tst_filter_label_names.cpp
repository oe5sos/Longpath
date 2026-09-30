// =================================================================
// tests/tst_filter_label_names.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original test file. The Thetis cite for the
// behaviour (panelFilter.Text = "Filter - " + GetName(f)) sits in
// RxApplet::updateFilterLabel.
//
// Der laufende Filter heisst wie seine Vorgabe — im RX-Applet und an der
// Pille der Befehlsleiste —, sonst steht die Breite da. Bis 2026-09-28
// stand ueberall die gerechnete Breite: CW F10 (±13 Hz) hiess „26", in
// Thetis und im „…"-Menue „25".
//
// =================================================================
// Modification history (Longpath):
//   2026-09-28 -- Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QLabel>

#include "core/AppSettings.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/CommandBar.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFilterLabelNames : public QObject {
    Q_OBJECT

    static QLabel* filterLabelOf(const RxApplet& applet)
    {
        for (QLabel* l : applet.findChildren<QLabel*>()) {
            if (l->toolTip().startsWith(QStringLiteral("Current filter"))) {
                return l;
            }
        }
        return nullptr;
    }

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── Der Speicher findet den Namen zu den Kanten ─────────────────────
    void theStoreNamesAPresetByItsEdges()
    {
        // Thetis-Vorgaben, Tonhoehe 600.
        QCOMPARE(FilterPresetStore::defaultNameForEdges(DSPMode::USB, 100, 2800),
                 QStringLiteral("2.7k"));
        QCOMPARE(FilterPresetStore::defaultNameForEdges(DSPMode::CWL, -613, -587),
                 QStringLiteral("25"));
        QCOMPARE(FilterPresetStore::defaultNameForEdges(DSPMode::DIGU, 1000, 2000),
                 QStringLiteral("1.0k"));
        // Keine Vorgabe: leer, die Anzeige nimmt dann die Breite.
        QVERIFY(FilterPresetStore::defaultNameForEdges(DSPMode::USB, 100, 2700).isEmpty());

        // Eine eigene Vorgabe gewinnt im Speicher, die Thetis-Vorgabe
        // bleibt davon unberuehrt.
        FilterPresetStore store;
        store.setPreset(DSPMode::USB, 5, FilterPreset{QStringLiteral("Mine"), 100, 2800});
        QCOMPARE(store.nameForEdges(DSPMode::USB, 100, 2800), QStringLiteral("Mine"));
        QCOMPARE(FilterPresetStore::defaultNameForEdges(DSPMode::USB, 100, 2800),
                 QStringLiteral("2.7k"));
    }

    // ── RX-Applet ───────────────────────────────────────────────────────
    void theRxAppletShowsThePresetNameElseTheWidth()
    {
        RadioModel model;
        if (model.slices().isEmpty()) { model.addSlice(); }
        SliceModel* s = model.slices().first();
        RxApplet applet(s, &model);
        QLabel* label = filterLabelOf(applet);
        QVERIFY(label);

        s->setDspMode(DSPMode::CWL);
        s->setFilter(-613, -587);                 // CW F10, ±13 Hz
        QCOMPARE(label->text(), QStringLiteral("25"));

        s->setFilter(-640, -560);                 // keine Vorgabe
        QCOMPARE(label->text(), QStringLiteral("80"));

        s->setDspMode(DSPMode::USB);
        s->setFilter(100, 2800);                  // USB F6
        QCOMPARE(label->text(), QStringLiteral("2.7k"));

        s->setFilter(100, 2600);                  // keine Vorgabe, kleines k
        QCOMPARE(label->text(), QStringLiteral("2.5k"));

        // Eine eigene Vorgabe mit genau diesen Kanten: der Name zieht nach,
        // ohne dass sich der Filter bewegt.
        s->setFilter(100, 2800);
        model.filterPresetStore()->setPreset(
            DSPMode::USB, 5, FilterPreset{QStringLiteral("Mine"), 100, 2800});
        QCOMPARE(label->text(), QStringLiteral("Mine"));
    }

    // ── Befehlsleiste ───────────────────────────────────────────────────
    void theRunningPillSaysThePresetName()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::CWL);
        CommandBar bar;
        bar.attach(&slice);

        slice.setFilter(-613, -587);
        QCOMPARE(bar.activePill(QStringLiteral("Filter")), QStringLiteral("25"));

        slice.setFilter(-640, -560);
        QCOMPARE(bar.activePill(QStringLiteral("Filter")), QStringLiteral("80"));

        // Mit Speicher: die eigene Vorgabe heisst, wie der Betreiber sie nennt.
        FilterPresetStore store;
        store.setPreset(DSPMode::CWL, 9, FilterPreset{QStringLiteral("Eng"), -613, -587});
        bar.setFilterPresetStore(&store);
        slice.setFilter(-613, -587);
        QCOMPARE(bar.activePill(QStringLiteral("Filter")), QStringLiteral("Eng"));
    }
};

QTEST_MAIN(TstFilterLabelNames)
#include "tst_filter_label_names.moc"
