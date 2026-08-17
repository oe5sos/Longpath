// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - tst_vfo_widget_snr: VfoWidget SNR row contract (Phase 3R L1).
//
// NEW NereusSDR-native UI extension to VfoWidget. No upstream
// equivalent. Pins the contract that VfoWidget surfaces the active
// slice's snrDb Q_PROPERTY (set by RadeChannel via I5 routing) as a
// single combined-status RichText label.  Row visibility tracks the
// slice's current DSPMode: only shown when mode is either RADE
// sideband (DSPMode::RADE_U or DSPMode::RADE_L).
//
// 2026-05-12 (PR #238 review follow-up): rewritten against the
// 2026-05-11 bench-design RADE status-row refactor that merged the
// separate name + value labels into a single m_snrLabel rendering
// "<prefix> ● <snr>dB" RichText.  The old separate m_snrValue label
// is retained only as a hidden no-op placeholder for API stability;
// the live status text now lives on m_snrLabel.  Color thresholds
// also moved to match AetherSDR VfoWidget.cpp:3424-3432 [@0cd4559]:
//   < 5 dB  -> #e0e040 (yellow)
//   >= 5 dB -> #00ff88 (green)
//   hollow  -> #505050 (grey)
//
// Cases:
//   nanShowsHollowInitialState  - NaN snrDb paints "<prefix> ○ ---" in grey
//   lowSnrShowsYellow           - snrDb=3.0 paints "RADE ● 3dB" in yellow
//   goodSnrShowsGreen           - snrDb=12.0 paints "RADE ● 12dB" in green
//   negativeSnrFormats          - snrDb=-1.0 paints "RADE ● -1dB" in yellow
//   snrHiddenInNonRadeMode      - mode=USB hides row; mode=RADE_U shows it
//   snrVisibleInRadeLower       - mode=RADE_L also shows the row

#include <QtTest>
#include <QSignalSpy>
#include <QLabel>

#include <cmath>
#include <limits>

#include "gui/StyleConstants.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestVfoWidgetSnr : public QObject {
    Q_OBJECT
private slots:
    void nanShowsHollowInitialState();
    void lowSnrShowsYellow();
    void goodSnrShowsGreen();
    void negativeSnrFormats();
    void snrHiddenInNonRadeMode();
    void snrVisibleInRadeLower();
};

namespace {

// Helper: build a VfoWidget bound to a SliceModel and put it in RADE_U
// mode.  The widget is constructed parentless so QTest can show() it
// without dragging in MainWindow/SpectrumWidget.  setMode(RADE_U)
// drives updateSnrVisibility() -> setRadeActive(true) so the SNR
// label becomes paintable.
void seedRadeMode(VfoWidget* vfo, SliceModel* slice) {
    slice->setDspMode(DSPMode::RADE_U);
    vfo->setSlice(slice);
    vfo->setMode(DSPMode::RADE_U);
}

} // namespace

void TestVfoWidgetSnr::nanShowsHollowInitialState() {
    SliceModel slice;
    VfoWidget vfo;
    seedRadeMode(&vfo, &slice);

    // Initial slice snrDb is NaN.  setRadeSnrLabel(NaN) early-returns
    // so the label stays at the setRadeActive(true) initial paint: the
    // placeholder in the label colour.
    slice.setSnrDb(std::numeric_limits<double>::quiet_NaN());

    QLabel* label = vfo.snrLabelForTest();
    QVERIFY(label != nullptr);
    // Der Platzhalter ist seit dem Hausstil ein Geviertstrich, nicht
    // drei Bindestriche. Geprueft wird, DASS ein Platzhalter steht --
    // welches Zeichen es ist, ist Gestaltung.
    QVERIFY2(!label->text().contains(QStringLiteral("dB")),
             qPrintable(QStringLiteral("ohne Messung darf keine Zahl "
                                       "dastehen, steht aber: %1")
                            .arg(label->text())));
    // Die Rolle, nicht der Wert: der Platzhalter traegt die
    // Beschriftungsfarbe. Frueher stand hier das nackte #505050 und
    // fiel um, sobald die Palette sich bewegte.
    QVERIFY2(label->text().contains(QString::fromLatin1(Style::kTextSecondary)),
             qPrintable(QStringLiteral("erwartet Beschriftungsfarbe %1, "
                                       "bekommen: %2")
                            .arg(QString::fromLatin1(Style::kTextSecondary),
                                 label->text())));
}

void TestVfoWidgetSnr::lowSnrShowsYellow() {
    SliceModel slice;
    VfoWidget vfo;
    seedRadeMode(&vfo, &slice);

    slice.setSnrDb(3.0);

    QLabel* label = vfo.snrLabelForTest();
    QVERIFY(label != nullptr);
    QVERIFY2(label->text().contains(QStringLiteral("3dB")),
             qPrintable(QStringLiteral("expected '3dB' in label, got: %1")
                            .arg(label->text())));
    // Unter 5 dB -> gedaempfte Messwertstufe. Frueher stand hier ein
    // gelbes #e0e040 aus der Zeit vor dem Hausstil; Gelb ist keine
    // Rolle dieser Palette. Jetzt die Konstante statt des Wertes.
    QVERIFY2(label->text().contains(QString::fromLatin1(Style::kAmberDim)),
             qPrintable(QStringLiteral("erwartet gedaempften Messwert %1, "
                                       "bekommen: %2")
                            .arg(QString::fromLatin1(Style::kAmberDim),
                                 label->text())));
}

void TestVfoWidgetSnr::goodSnrShowsGreen() {
    SliceModel slice;
    VfoWidget vfo;
    seedRadeMode(&vfo, &slice);

    slice.setSnrDb(12.0);

    QLabel* label = vfo.snrLabelForTest();
    QVERIFY(label != nullptr);
    QVERIFY2(label->text().contains(QStringLiteral("12dB")),
             qPrintable(QStringLiteral("expected '12dB' in label, got: %1")
                            .arg(label->text())));
    // Ab 5 dB -> volle Messwertstufe. Frueher gruen (#00ff88): der SNR
    // ist ein GEMESSENER Wert und kein Zustand, also Bernstein.
    QVERIFY2(label->text().contains(QString::fromLatin1(Style::kAmberText)),
             qPrintable(QStringLiteral("erwartet vollen Messwert %1, "
                                       "bekommen: %2")
                            .arg(QString::fromLatin1(Style::kAmberText),
                                 label->text())));
}

void TestVfoWidgetSnr::negativeSnrFormats() {
    SliceModel slice;
    VfoWidget vfo;
    seedRadeMode(&vfo, &slice);

    slice.setSnrDb(-1.0);

    QLabel* label = vfo.snrLabelForTest();
    QVERIFY(label != nullptr);
    // Negative SNR formats with a single leading "-"; integer-truncate
    // (static_cast<int>) per VfoWidget::setRadeSnrLabel:814.
    QVERIFY2(label->text().contains(QStringLiteral("-1dB")),
             qPrintable(QStringLiteral("expected '-1dB' in label, got: %1")
                            .arg(label->text())));
    // Negativ ist unter der Schwelle -> gedaempft.
    QVERIFY2(label->text().contains(QString::fromLatin1(Style::kAmberDim)),
             qPrintable(QStringLiteral("erwartet gedaempften Messwert %1, "
                                       "bekommen: %2")
                            .arg(QString::fromLatin1(Style::kAmberDim),
                                 label->text())));
}

void TestVfoWidgetSnr::snrHiddenInNonRadeMode() {
    SliceModel slice;
    VfoWidget vfo;

    // Start in USB.  Row label must be hidden because SNR is only
    // meaningful for the RADE neural codec.  updateSnrVisibility ->
    // setRadeActive(false) -> m_snrLabel->hide().
    slice.setDspMode(DSPMode::USB);
    vfo.setSlice(&slice);
    vfo.setMode(DSPMode::USB);

    QLabel* label = vfo.snrLabelForTest();
    QVERIFY(label != nullptr);
    QVERIFY(!label->isVisibleTo(&vfo));

    // Switch the slice to RADE_U: row becomes visible.
    slice.setDspMode(DSPMode::RADE_U);
    vfo.setMode(DSPMode::RADE_U);
    QVERIFY(label->isVisibleTo(&vfo));
}

void TestVfoWidgetSnr::snrVisibleInRadeLower() {
    SliceModel slice;
    VfoWidget vfo;

    // RADE_L (lower sideband) also reveals the SNR row.  Both RADE
    // sidebands share the same SNR semantics; the neural codec
    // produces the same SNR estimate regardless of which sideband
    // it serves.
    slice.setDspMode(DSPMode::RADE_L);
    vfo.setSlice(&slice);
    vfo.setMode(DSPMode::RADE_L);

    QLabel* label = vfo.snrLabelForTest();
    QVERIFY(label != nullptr);
    QVERIFY(label->isVisibleTo(&vfo));
}

QTEST_MAIN(TestVfoWidgetSnr)
#include "tst_vfo_widget_snr.moc"
