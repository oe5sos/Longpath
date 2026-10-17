#include "GeneralSetupPages.h"

#include "core/AppSettings.h"
#include "core/Maidenhead.h"
#include "gui/StyleConstants.h"

#include <QLineEdit>

namespace Longpath {

// ---------------------------------------------------------------------------
// StartupPrefsPage
// ---------------------------------------------------------------------------

StartupPrefsPage::StartupPrefsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Startup & Preferences"), model, parent)
{
    // Section: Startup
    addSection(QStringLiteral("Startup"));

    auto* restoreFreq = addLabeledToggle(QStringLiteral("Restore last frequency on connect"));
    markNyi(restoreFreq, QStringLiteral("Phase 3E"));

    // ── Rufzeichen und Locator (verdrahtet 2026-10-13) ───────────────────────
    //
    // Diese zwei Felder waren Attrappen ("future"), und das hatte Folgen,
    // die niemandem auffielen, weil nichts eine Fehlermeldung gab:
    //
    //   * Der FlexRadio-Broadcaster sendet das Rufzeichen ins Netz, wo
    //     SmartSDR und der PGXL es sehen. Ohne Eintrag sendete er
    //     "LONGPATH" (RadioModel.cpp:8952-8954, Vorgabewert).
    //   * Der KiwiSDR schickt es dem Betreiber des Empfaengers als
    //     Kennung -- bisher leer (MainWindow_KiwiSdr.cpp:616-620).
    //   * Die QSO-Karte zeigt die eigene Station nur mit Rufzeichen; ohne
    //     endet sie "beim Punkt" (QsoMapWindow.cpp:625-633).
    //   * Der Satellitendienst rechnet Ueberfluege vom Locator aus. Ohne
    //     gueltigen Locator gibt inView() IMMER eine leere Liste zurueck
    //     (SatelliteService.cpp:52-65) -- ein stiller Totalausfall.
    //
    // Die Schluessel sind die, die diese vier LESEN: `StationCallsign`
    // und `StationGridSquare`. Das Feld hier hiess vorher "Callsign" und
    // haette, waere es je verdrahtet worden, unter einem fuenften,
    // ungelesenen Namen gespeichert.
    auto* callsign = addLabeledEdit(QStringLiteral("Callsign"),
                                    QStringLiteral("z. B. OE5SOS"));
    callsign->setToolTip(QStringLiteral(
        "Das eigene Rufzeichen. Geht an die QSO-Karte, den KiwiSDR, den "
        "FlexRadio-Rundruf im Netz und ins Logbuch."));
    {
        auto& st = AppSettings::instance();
        callsign->setText(st.value(QStringLiteral("StationCallsign"), QString{})
                              .toString());
        // Beim Verlassen des Felds, nicht bei jedem Tastendruck: ein
        // halb getipptes Rufzeichen soll nicht in den Rundruf gehen.
        QObject::connect(callsign, &QLineEdit::editingFinished, callsign, [callsign]() {
            const QString wert = callsign->text().trimmed().toUpper();
            // Gross geschrieben, weil alle vier Leser es so erwarten --
            // QsoMapWindow ruft selbst toUpper(), die anderen drei nicht.
            if (callsign->text() != wert) { callsign->setText(wert); }
            AppSettings::instance().setValue(QStringLiteral("StationCallsign"), wert);
        });
    }

    auto* gridSquare = addLabeledEdit(QStringLiteral("Grid Square"),
                                      QStringLiteral("z. B. JN78"));
    gridSquare->setToolTip(QStringLiteral(
        "Der eigene Locator. Der Satellitendienst rechnet Ueberfluege "
        "daraus -- ohne gueltigen Locator findet er nichts."));
    {
        auto& st = AppSettings::instance();
        gridSquare->setText(st.value(QStringLiteral("StationGridSquare"), QString{})
                                .toString());
        QObject::connect(gridSquare, &QLineEdit::editingFinished, gridSquare, [gridSquare]() {
            const QString wert = gridSquare->text().trimmed().toUpper();
            if (gridSquare->text() != wert) { gridSquare->setText(wert); }
            // Ein UNGUELTIGER Locator wird NICHT gespeichert, und das Feld
            // sagt es. Sonst stuende Unsinn in den Einstellungen, der
            // Satellitendienst faende weiter nichts, und niemand wuesste,
            // warum. Leeren ist erlaubt (dann ist eben keiner gesetzt).
            if (wert.isEmpty()) {
                AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                                 QString{});
                gridSquare->setStyleSheet(QString{});
                return;
            }
            if (!isValidGridSquare(wert)) {
                gridSquare->setStyleSheet(
                    QStringLiteral("QLineEdit { border: 1px solid %1; }")
                        .arg(Style::kRedBorder));
                gridSquare->setToolTip(
                    tr("\"%1\" is not a valid grid square "
                      "(expected e.g. JN78 or JN78dc)").arg(wert));
                return;
            }
            gridSquare->setStyleSheet(QString{});
            gridSquare->setToolTip(QStringLiteral(
                "Der eigene Locator. Der Satellitendienst rechnet "
                "Ueberfluege daraus."));
            AppSettings::instance().setValue(QStringLiteral("StationGridSquare"), wert);
        });
    }

    // Section: Application
    addSection(QStringLiteral("Application"));

    auto* splash = addLabeledToggle(QStringLiteral("Show splash screen at startup"));
    markNyi(splash, QStringLiteral("Phase 3N"));

    auto* checkUpdates = addLabeledToggle(QStringLiteral("Check for updates on startup"));
    markNyi(checkUpdates, QStringLiteral("Phase 3N"));

    auto* regenWisdom = addLabeledButton(QStringLiteral("FFTW Wisdom"),
                                         QStringLiteral("Regenerate"));
    markNyi(regenWisdom, QStringLiteral("Phase 3C"));

    auto* priority = addLabeledCombo(QStringLiteral("Process Priority"),
        {QStringLiteral("Normal"), QStringLiteral("Above Normal"),
         QStringLiteral("High"), QStringLiteral("Realtime")});
    markNyi(priority, QStringLiteral("future"));
}

// ---------------------------------------------------------------------------
// UiScalePage
// ---------------------------------------------------------------------------

UiScalePage::UiScalePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("UI Scale & Theme"), model, parent)
{
    // Section: Scale
    addSection(QStringLiteral("Scale"));

    auto* scale = addLabeledCombo(QStringLiteral("UI Scale"),
        {QStringLiteral("100%"), QStringLiteral("125%"),
         QStringLiteral("150%"), QStringLiteral("175%"),
         QStringLiteral("200%")});
    markNyi(scale, QStringLiteral("Phase 3H"));

    // Section: Theme
    addSection(QStringLiteral("Theme"));

    auto* darkLight = addLabeledToggle(QStringLiteral("Dark mode"));
    markNyi(darkLight, QStringLiteral("Phase 3H"));

    auto* fontSize = addLabeledCombo(QStringLiteral("Font Size"),
        {QStringLiteral("Small"), QStringLiteral("Medium"),
         QStringLiteral("Large")});
    markNyi(fontSize, QStringLiteral("Phase 3H"));
}

// ---------------------------------------------------------------------------
// NavigationPage
// ---------------------------------------------------------------------------

NavigationPage::NavigationPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Navigation"), model, parent)
{
    // Section: Mouse
    addSection(QStringLiteral("Mouse"));

    auto* wheelTune = addLabeledToggle(QStringLiteral("Mouse wheel tunes VFO"));
    markNyi(wheelTune, QStringLiteral("Phase 3E"));

    auto* clickTune = addLabeledToggle(QStringLiteral("Click-to-tune on panadapter"));
    markNyi(clickTune, QStringLiteral("Phase 3E"));

    auto* scrollZoom = addLabeledToggle(QStringLiteral("Scroll zoom on panadapter"));
    markNyi(scrollZoom, QStringLiteral("Phase 3E"));

    auto* dblClick = addLabeledCombo(QStringLiteral("Double-click action"),
        {QStringLiteral("Tune"), QStringLiteral("Center"),
         QStringLiteral("None")});
    markNyi(dblClick, QStringLiteral("Phase 3E"));

    // Section: Tuning
    addSection(QStringLiteral("Tuning"));

    auto* snapTune = addLabeledToggle(QStringLiteral("Snap click-tune to step"));
    markNyi(snapTune, QStringLiteral("Phase 3E"));

    auto* wheelOutside = addLabeledToggle(QStringLiteral("Wheel tunes outside spectral display"));
    markNyi(wheelOutside, QStringLiteral("Phase 3E"));

    auto* wheelReverse = addLabeledToggle(QStringLiteral("Reverse wheel direction"));
    markNyi(wheelReverse, QStringLiteral("Phase 3E"));
}

} // namespace Longpath
