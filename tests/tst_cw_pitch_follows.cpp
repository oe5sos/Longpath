// =================================================================
// tests/tst_cw_pitch_follows.cpp  (Longpath)
// =================================================================
//
// Die CW-Tonhoehe bekommt einen Setter, ein Signal und ein Setup-Feld
// (2026-09-27). Bis dahin lasen CW-Filter, APF, Decoder und Kiwi den
// Schluessel "CWPitch", aber nichts konnte ihn aendern.
//
// Festgenagelt wird, was der Thetis-Setter CWPitch (console.cs:18142-
// 18242, v2.10.3.15-5-g852bf0e) mit den Filtern macht:
//
//   1. Jeder CW-Durchlass wandert auf die neue Tonhoehe und BEHAELT
//      seine Breite (`bw / 2`, Ganzzahlteilung: eine ungerade Breite
//      verliert 1 Hz).
//   2. An der Spiegelgrenze (0 Hz) rutscht er, statt ueber den Traeger
//      zu reichen: CWU unten auf 0, CWL oben auf 0 — die Breite bleibt.
//   3. Das gilt fuer die Vorgabentabelle, fuer die vom Bedienenden
//      ueberschriebenen Plaetze, fuer VAR1/VAR2, fuer den laufenden
//      Durchlass jeder CW-Scheibe und fuer die gespeicherten
//      Banddurchlaesse.
//   4. Das Signal kommt genau dann, wenn sich der Wert aendert; geklemmt
//      wird auf die Spanne der Thetis-Felder, 200..2250.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-27 -- Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-09-28 -- thePresetTableSlidesAtALowPitch auf die Thetis-
//                 Tabelle umgestellt (Martin Fischer, OE5SOS,
//                 KI-gestuetzt ueber Anthropic Claude).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "gui/setup/DspSetupPages.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

using Edges = std::pair<int, int>;

Edges centred(int low, int high, DSPMode mode, int pitch)
{
    SliceModel::centreCwFilterOnPitch(low, high, mode, pitch);
    return {low, high};
}

Edges edgesOf(const SliceModel* s)
{
    return {s->filterLow(), s->filterHigh()};
}

} // namespace

class TstCwPitchFollows : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── Die Rechnung selbst ──────────────────────────────────────────

    void cwuMovesToThePitchAndKeepsItsWidth()
    {
        // 300 Hz breit um 600 -> 300 Hz breit um 800.
        QCOMPARE(centred(450, 750, DSPMode::CWU, 800), Edges(650, 950));
    }

    void cwlMovesToTheNegativePitch()
    {
        QCOMPARE(centred(-750, -450, DSPMode::CWL, 800), Edges(-950, -650));
    }

    void cwuSlidesUpAtTheImageLimit()
    {
        // 1500 Hz breit um 300: -450..1050 wuerde ueber den Traeger
        // reichen. Thetis: `high -= low; low = 0;`.
        QCOMPARE(centred(-150, 1350, DSPMode::CWU, 300), Edges(0, 1500));
    }

    void cwlSlidesDownAtTheImageLimit()
    {
        // Spiegelbild: -1050..450 -> `low -= high; high = 0;`.
        QCOMPARE(centred(-1350, 150, DSPMode::CWL, 300), Edges(-1500, 0));
    }

    void aPassbandExactlyAtZeroDoesNotSlide()
    {
        // `if (low < 0)` — nicht `<=`: an der Grenze selbst bleibt er.
        QCOMPARE(centred(0, 400, DSPMode::CWU, 200), Edges(0, 400));
        QCOMPARE(centred(-400, 0, DSPMode::CWL, 200), Edges(-400, 0));
    }

    void anOddWidthLosesOneHertzLikeThetis()
    {
        // bw 251, bw / 2 == 125 -> 575..825, 250 breit.
        QCOMPARE(centred(500, 751, DSPMode::CWU, 700), Edges(575, 825));
    }

    void nonCwModesAreLeftAlone()
    {
        QCOMPARE(centred(100, 2800, DSPMode::USB, 800), Edges(100, 2800));
        QCOMPARE(centred(-2800, -100, DSPMode::LSB, 800), Edges(-2800, -100));
        QCOMPARE(centred(-5000, 5000, DSPMode::AM, 800), Edges(-5000, 5000));
    }

    // ── Spanne ───────────────────────────────────────────────────────

    void theReadSideClampsToTheThetisFieldRange()
    {
        auto& s = AppSettings::instance();
        QCOMPARE(SliceModel::cwPitchHz(), 600);            // Vorgabe
        s.setValue(QStringLiteral("CWPitch"), QStringLiteral("150"));
        QCOMPARE(SliceModel::cwPitchHz(), 200);
        s.setValue(QStringLiteral("CWPitch"), QStringLiteral("2200"));
        QCOMPARE(SliceModel::cwPitchHz(), 2200);           // frueher 2000
        s.setValue(QStringLiteral("CWPitch"), QStringLiteral("9000"));
        QCOMPARE(SliceModel::cwPitchHz(), 2250);
    }

    // ── Die Vorgabentabelle ──────────────────────────────────────────

    void thePresetTableSlidesAtALowPitch()
    {
        AppSettings::instance().setValue(QStringLiteral("CWPitch"),
                                         QStringLiteral("300"));
        // Thetis-Tabelle (console.cs:5339-5379 [@852bf0e]) seit
        // 2026-09-28; vorher hing dieser Test an erfundenen Breiten
        // (±750 … ±6).
        const auto cwu = SliceModel::presetsForMode(DSPMode::CWU);
        QCOMPARE(cwu.size(), 10);
        QCOMPARE(cwu[0], Edges(0, 1000));    // ±500 -> gerutscht
        QCOMPARE(cwu[1], Edges(0, 800));     // ±400 -> gerutscht
        QCOMPARE(cwu[2], Edges(0, 600));     // ±300 -> liegt genau an
        QCOMPARE(cwu[3], Edges(50, 550));    // ±250 -> unberuehrt
        QCOMPARE(cwu[4], Edges(100, 500));   // ±200 -> unberuehrt
        QCOMPARE(cwu[9], Edges(287, 313));   // ±13, „25"

        const auto cwl = SliceModel::presetsForMode(DSPMode::CWL);
        QCOMPARE(cwl[0], Edges(-1000, 0));
        QCOMPARE(cwl[1], Edges(-800, 0));
        QCOMPARE(cwl[4], Edges(-500, -100));

        // Thetis behaelt beim Verschieben den Namen (console.cs:18165
        // [@852bf0e]) — die Namen haengen nicht an der Tonhoehe.
        QCOMPARE(SliceModel::presetNamesForMode(DSPMode::CWU).value(0),
                 QStringLiteral("1.0k"));
    }

    void noPresetCrossesTheCarrierAtAnyPitch()
    {
        for (int pitch = SliceModel::kCwPitchMinHz;
             pitch <= SliceModel::kCwPitchMaxHz;
             pitch += SliceModel::kCwPitchStepHz) {
            AppSettings::instance().setValue(QStringLiteral("CWPitch"),
                                             QString::number(pitch));
            for (DSPMode m : {DSPMode::CWL, DSPMode::CWU}) {
                for (const Edges& e : SliceModel::presetsForMode(m)) {
                    QVERIFY2(!SliceModel::filterCrossesCarrier(e.first, e.second, m),
                             qPrintable(QStringLiteral("%1 Hz, %2: %3..%4")
                                        .arg(pitch).arg(SliceModel::modeName(m))
                                        .arg(e.first).arg(e.second)));
                }
            }
        }
    }

    // ── Setter und Signal ────────────────────────────────────────────

    void setCwPitchStoresClampsAndSignalsOnlyOnChange()
    {
        RadioModel radio;
        QSignalSpy spy(&radio, &RadioModel::cwPitchChanged);

        radio.setCwPitch(700);
        QCOMPARE(radio.cwPitch(), 700);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("CWPitch")).toInt(), 700);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().at(0).toInt(), 700);

        radio.setCwPitch(700);                   // derselbe Wert: still
        QCOMPARE(spy.count(), 1);

        radio.setCwPitch(50);
        QCOMPARE(radio.cwPitch(), 200);
        QCOMPARE(spy.last().at(0).toInt(), 200);

        radio.setCwPitch(99999);
        QCOMPARE(radio.cwPitch(), 2250);
        QCOMPARE(spy.count(), 3);
    }

    // ── Der laufende Durchlass ───────────────────────────────────────

    void aCwuSliceFollowsLive()
    {
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::CWU);
        slice->setFilter(450, 750);

        QSignalSpy filterSpy(slice, &SliceModel::filterChanged);
        radio.setCwPitch(800);
        QCOMPARE(edgesOf(slice), Edges(650, 950));
        QCOMPARE(filterSpy.count(), 1);
    }

    void aCwlSliceFollowsLive()
    {
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::CWL);
        slice->setFilter(-750, -450);

        radio.setCwPitch(700);
        QCOMPARE(edgesOf(slice), Edges(-850, -550));
    }

    void aSliceAtTheImageLimitSlidesAndMayNotMoveAtAll()
    {
        RadioModel radio;
        radio.setCwPitch(300);
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::CWU);
        slice->setFilter(0, 1500);

        // 1500 breit um 350 waere -400..1100 -> rutscht zurueck auf 0..1500.
        // Der Filter bewegt sich nicht, die Tonhoehe schon: das Signal
        // muss trotzdem kommen (Kiwi und Decoder haengen daran).
        QSignalSpy filterSpy(slice, &SliceModel::filterChanged);
        QSignalSpy pitchSpy(&radio, &RadioModel::cwPitchChanged);
        radio.setCwPitch(350);
        QCOMPARE(edgesOf(slice), Edges(0, 1500));
        QCOMPARE(filterSpy.count(), 0);
        QCOMPARE(pitchSpy.count(), 1);

        // Weit genug hinauf, und er loest sich von der Grenze.
        radio.setCwPitch(800);
        QCOMPARE(edgesOf(slice), Edges(50, 1550));
    }

    void aNonCwSliceKeepsItsFilter()
    {
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFilter(100, 2800);

        QSignalSpy filterSpy(slice, &SliceModel::filterChanged);
        radio.setCwPitch(900);
        QCOMPARE(edgesOf(slice), Edges(100, 2800));
        QCOMPARE(filterSpy.count(), 0);
    }

    void theVarSlotsOfCwFollowToo()
    {
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::CWU);
        slice->setFilterByHand(500, 700);          // landet auf VAR1
        QCOMPARE(slice->varFilter(0), qMakePair(500, 700));

        radio.setCwPitch(800);
        QCOMPARE(slice->varFilter(0), qMakePair(700, 900));
    }

    // ── Ueberschriebene Vorgaben ─────────────────────────────────────

    void overriddenPresetsFollowAndKeepTheirName()
    {
        RadioModel radio;
        FilterPresetStore* store = radio.filterPresetStore();
        QVERIFY(store);
        store->setPreset(DSPMode::CWU, 2, FilterPreset{QStringLiteral("Mine"), 800, 1400});
        store->setPreset(DSPMode::CWL, 0, FilterPreset{QStringLiteral("Wide"), -2100, -100});

        QSignalSpy spy(store, &FilterPresetStore::presetsChanged);
        radio.setCwPitch(700);

        const auto cwu = store->presetsForMode(DSPMode::CWU);
        QCOMPARE(cwu[2].name, QStringLiteral("Mine"));
        QCOMPARE(Edges(cwu[2].low, cwu[2].high), Edges(400, 1000));
        // Ein Platz ohne Ueberschreibung kommt aus der Tabelle, um 700.
        QCOMPARE(Edges(cwu[4].low, cwu[4].high), Edges(500, 900));

        // 2000 breit um -700 reichte bis +300 -> rutscht auf -2000..0.
        const auto cwl = store->presetsForMode(DSPMode::CWL);
        QCOMPARE(cwl[0].name, QStringLiteral("Wide"));
        QCOMPARE(Edges(cwl[0].low, cwl[0].high), Edges(-2000, 0));

        // Beide CW-Tabellen melden sich, damit die Knoepfe neu beschriften.
        QList<DSPMode> modes;
        for (const QList<QVariant>& args : spy) {
            modes << args.at(0).value<DSPMode>();
        }
        QVERIFY(modes.contains(DSPMode::CWL));
        QVERIFY(modes.contains(DSPMode::CWU));
    }

    // ── Gespeicherte Banddurchlaesse ─────────────────────────────────

    void storedBandFiltersFollow()
    {
        auto& s = AppSettings::instance();
        // Neue Form je (Scheibe, Band, Betriebsart).
        s.setValue(QStringLiteral("Slice5/Band40m/ModeCWU/FilterLow"),  400);
        s.setValue(QStringLiteral("Slice5/Band40m/ModeCWU/FilterHigh"), 800);
        s.setValue(QStringLiteral("Slice5/Band40m/ModeUSB/FilterLow"),  100);
        s.setValue(QStringLiteral("Slice5/Band40m/ModeUSB/FilterHigh"), 2800);
        // Alte Form je (Scheibe, Band); die Betriebsart steht daneben.
        s.setValue(QStringLiteral("Slice5/Band20m/FilterLow"),  -800);
        s.setValue(QStringLiteral("Slice5/Band20m/FilterHigh"), -400);
        s.setValue(QStringLiteral("Slice5/Band20m/DspMode"),
                   static_cast<int>(DSPMode::CWL));
        s.setValue(QStringLiteral("Slice5/Band30m/FilterLow"),  100);
        s.setValue(QStringLiteral("Slice5/Band30m/FilterHigh"), 2800);
        s.setValue(QStringLiteral("Slice5/Band30m/DspMode"),
                   static_cast<int>(DSPMode::USB));

        RadioModel radio;
        radio.setCwPitch(700);

        auto v = [&s](const char* key) { return s.value(QLatin1String(key)).toInt(); };
        QCOMPARE(v("Slice5/Band40m/ModeCWU/FilterLow"),  500);
        QCOMPARE(v("Slice5/Band40m/ModeCWU/FilterHigh"), 900);
        QCOMPARE(v("Slice5/Band40m/ModeUSB/FilterLow"),  100);
        QCOMPARE(v("Slice5/Band40m/ModeUSB/FilterHigh"), 2800);
        QCOMPARE(v("Slice5/Band20m/FilterLow"),  -900);
        QCOMPARE(v("Slice5/Band20m/FilterHigh"), -500);
        QCOMPARE(v("Slice5/Band30m/FilterLow"),  100);
        QCOMPARE(v("Slice5/Band30m/FilterHigh"), 2800);
    }

    // ── Das Setup-Feld ───────────────────────────────────────────────

    void theSetupFieldHasTheThetisRangeAndTooltip()
    {
        RadioModel radio;
        CwSetupPage page(&radio);
        QSpinBox* spin = page.cwPitchSpinForTest();
        QVERIFY(spin);
        QCOMPARE(spin->minimum(), 200);
        QCOMPARE(spin->maximum(), 2250);
        QCOMPARE(spin->singleStep(), 10);
        QCOMPARE(spin->value(), 600);
        QCOMPARE(spin->toolTip(), QStringLiteral("Selects the preferred CW tone frequency."));
        QVERIFY(spin->isEnabled());
    }

    void theSetupFieldDrivesTheModelAndFollowsIt()
    {
        RadioModel radio;
        CwSetupPage page(&radio);
        QSpinBox* spin = page.cwPitchSpinForTest();
        QVERIFY(spin);

        QSignalSpy spy(&radio, &RadioModel::cwPitchChanged);
        spin->setValue(750);
        QCOMPARE(radio.cwPitch(), 750);
        QCOMPARE(spy.count(), 1);

        // Von anderswo gesetzt (ein zweites Setup-Fenster, spaeter CAT):
        // das Feld zieht nach, ohne ein zweites Mal zu setzen.
        radio.setCwPitch(900);
        QCOMPARE(spin->value(), 900);
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_MAIN(TstCwPitchFollows)
#include "tst_cw_pitch_follows.moc"
