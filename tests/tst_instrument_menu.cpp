// no-port-check: nennt console.cs nur als Herkunft einer Begruendung —
// dass Max Bin denselben Kalibrieroffset bekommt wie Signal Peak und
// Signal Avg. Der Port dieser Zeile lebt in MeterPoller.cpp und ist
// dort mit Zeilenangabe und Version zitiert. Hier steht kein
// uebernommener Code, nur die Erklaerung, warum die drei auf einer
// Skala stehen.

// =================================================================
// tests/tst_instrument_menu.cpp  (NereusSDR)
// =================================================================
//
// Das Rechtsklickmenü der Instrumente — die eine Stelle, an der ein
// Instrument eingestellt wird.
//
// Warum das einen Test verdient: bis 2026-08-18 gab es ZWEI Wege, eine
// Empfangsquelle zu waehlen. Die Kennung (jedes Messwerkzeug, aus
// ReadingSource) und der Rechtsklick der analogen S-Meter-Anzeige mit
// ihrer eigenen vierzeiligen RxMode-Liste. Zwei Listen fuer dieselbe
// Entscheidung laufen auseinander, und genau das war passiert: „Max
// Bin" stand nur in der zweiten.
//
// Der Test haelt fest, dass das Menue aus der EINEN Liste kommt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>

#include <QAction>
#include <QMenu>

#include "gui/applets/InstrumentApplet.h"
#include "gui/instruments/PeakHold.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

namespace {

/// Das Untermenue mit diesem Titel, oder nullptr.
QMenu* subMenu(QMenu* menu, const QString& title)
{
    for (QAction* a : menu->actions()) {
        if (a->menu() && a->text() == title) { return a->menu(); }
    }
    return nullptr;
}

QStringList entryLabels(QMenu* menu)
{
    QStringList out;
    if (!menu) { return out; }
    for (QAction* a : menu->actions()) {
        if (a->isSeparator() || a->menu()) { continue; }
        out << a->text();
    }
    return out;
}

QAction* entry(QMenu* menu, const QString& text)
{
    if (!menu) { return nullptr; }
    for (QAction* a : menu->actions()) {
        if (a->text() == text) { return a; }
    }
    return nullptr;
}

} // namespace

class TestInstrumentMenu : public QObject
{
    Q_OBJECT

private slots:

    // ── Eine Liste ───────────────────────────────────────────────────

    void theSourceMenuIsTheReadingListItself()
    {
        InstrumentApplet applet(QStringLiteral("Test"),
                                QStringLiteral("Test"), nullptr);
        QMenu* menu = applet.buildContextMenu(nullptr);
        const QStringList shown = entryLabels(subMenu(menu, "Quelle"));

        QStringList expected;
        for (const ReadingDescriptor* d : readingsWithScale()) {
            expected << d->label;
        }
        QVERIFY2(!expected.isEmpty(), "die Liste ist leer, der Test sagt nichts");
        QCOMPARE(shown, expected);
        delete menu;
    }

    // Der Befund, der den Umbau ausgeloest hat: Max Bin war nur ueber
    // die zweite Liste erreichbar, weil es in ReadingSource ohne Skala
    // stand. Es steht jetzt auf derselben S-Skala wie Signal Peak und
    // Signal Avg — Thetis rechnet ihm denselben Offset an
    // (console.cs:46881 gegen :46824 und :46828).
    void maxBinIsOfferedLikeTheOtherTwoSignalSources()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::SignalMaxBin);
        QVERIFY(d);
        QVERIFY2(d->hasScale, "Max Bin traegt keine Skala und ist unsichtbar");

        const ReadingDescriptor* peak = readingFor(MeterBinding::SignalPeak);
        QVERIFY(peak);
        QCOMPARE(d->min, peak->min);
        QCOMPARE(d->max, peak->max);
        QCOMPARE(d->ticks.size(), peak->ticks.size());
    }

    void theSecondDisplayCanBeTurnedOff()
    {
        InstrumentApplet applet(QStringLiteral("Test"),
                                QStringLiteral("Test"), nullptr);
        QMenu* menu = applet.buildContextMenu(nullptr);
        QMenu* second = subMenu(menu, "Zweite Anzeige");
        QVERIFY(second);
        QVERIFY2(entry(second, "keine"),
                 "ohne Aus-Eintrag ist die zweite Anzeige eine Einbahn");
        delete menu;
    }

    // ── Beide Ansichten, eine Einstellung ────────────────────────────
    //
    // Eine Einstellung, die nur die sichtbare Form erreicht, springt
    // beim Umschalten zurueck — der Betreiber stellt sie am Zeiger ein
    // und findet sie am Balken nicht wieder.

    void theFormSwitchLivesInTheMenu()
    {
        InstrumentApplet applet(QStringLiteral("Test"),
                                QStringLiteral("Test"), nullptr);
        QMenu* menu = applet.buildContextMenu(nullptr);
        QMenu* form = subMenu(menu, "Form");
        QVERIFY(form);
        QCOMPARE(entryLabels(form), QStringList({"Zeiger", "Balken"}));

        QCOMPARE(applet.form(), InstrumentApplet::Form::Needle);
        entry(form, "Balken")->trigger();
        QCOMPARE(applet.form(), InstrumentApplet::Form::Bar);
        delete menu;
    }

    void peakSettingsReachBothForms()
    {
        InstrumentApplet applet(QStringLiteral("Test"),
                                QStringLiteral("Test"), nullptr);
        QMenu* menu = applet.buildContextMenu(nullptr);
        QMenu* peak = subMenu(menu, "Spitzenhaltung");
        QVERIFY(peak);

        QAction* show = entry(peak, "Anzeigen");
        QVERIFY(show);
        QVERIFY(show->isChecked());
        show->trigger();   // aus

        // Beide Ansichten, nicht nur die sichtbare.
        QVERIFY(!applet.peakHold(InstrumentApplet::Form::Needle).enabled());
        QVERIFY(!applet.peakHold(InstrumentApplet::Form::Bar).enabled());

        // Und die Haltezeit ebenso.
        QMenu* hold = subMenu(peak, "Halten");
        QVERIFY(hold);
        hold->actions().first()->trigger();          // kurz
        QCOMPARE(applet.peakHold(InstrumentApplet::Form::Needle).holdMs(),
                 PeakHold::kShortMs);
        QCOMPARE(applet.peakHold(InstrumentApplet::Form::Bar).holdMs(),
                 PeakHold::kShortMs);

        delete menu;
    }

    void theHoldTimeIsOfferedInThreeSteps()
    {
        InstrumentApplet applet(QStringLiteral("Test"),
                                QStringLiteral("Test"), nullptr);
        QMenu* menu = applet.buildContextMenu(nullptr);
        QMenu* hold = subMenu(subMenu(menu, "Spitzenhaltung"), "Halten");
        QVERIFY(hold);
        QCOMPARE(hold->actions().size(), 3);

        // Die Vorgabe ist die mittlere — dieselbe Zahl, die vorher als
        // Konstante in beiden Instrumenten stand.
        QCOMPARE(PeakHold::kMediumMs, 3000);
        delete menu;
    }

    // ── Die Spitze selbst ────────────────────────────────────────────

    void thePeakFallsBackAfterItsHoldTime()
    {
        PeakHold p;
        p.setHoldMs(1);
        p.seed(10.0);
        QCOMPARE(p.value(), 10.0);
        // Echte Zeit, keine gesetzte Uhr: die Haltezeit ist das
        // Einzige, was PeakHold ueberhaupt tut, und ein Test, der sie
        // umgeht, prueft nur noch das Groesser-Zeichen.
        QTest::qWait(20);
        p.note(3.0);
        QCOMPARE(p.value(), 3.0);
    }

    void thePeakKeepsTheHigherValueWhileItHolds()
    {
        PeakHold p;
        p.setHoldMs(60000);
        p.seed(10.0);
        p.note(3.0);
        QCOMPARE(p.value(), 10.0);
        p.note(12.0);
        QCOMPARE(p.value(), 12.0);
    }

    // Der erste Messwert IST die erste Spitze. Sonst stuende als Spitze
    // der Anfang der Skala — und bei der Stehwelle waere das 1,00,
    // also ein hervorragendes Ergebnis, das nie gemessen wurde.
    void theFirstReadingIsTheFirstPeak()
    {
        PeakHold p;
        p.note(7.5);
        QCOMPARE(p.value(), 7.5);
    }
};

QTEST_MAIN(TestInstrumentMenu)
#include "tst_instrument_menu.moc"
