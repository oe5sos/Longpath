// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: rendert zwei Entwuerfe fuer die aktive
// Bandmarkierung im Band-Flyout (SpectrumOverlayPanel::buildBandFlyout,
// Zeile ~700) in wirklicher Groesse -- je Entwurf ein eigenes Blatt, je
// Blatt zwei Betriebsfaelle (aktives Band in der Mitte des Rasters,
// aktives Band am Rand), damit sich die Lesbarkeit unabhaengig von der
// Knopf-Position beurteilen laesst.
//
// Anlass, 2026-09-06: AetherSDR-Sichtung ("highlight active band").
// Longpaths Band-Flyout hatte denselben Fehlstand, den AetherSDR vor
// ihrem Fix hatte: ein Raster aus Knoepfen, keiner davon setCheckable,
// keine Markierung, welches Band gerade gehoert wird. Betreiber
// 2026-09-05: "zeig mir erst einen Entwurf" -- dieses Blatt.
//
// Masse, Raster und Knopf-Stil sind wortgleich aus buildBandFlyout()
// uebernommen (kBandBtnW/H, kBands, bandBtnStyle), nur die
// :checked-Regel ist der Entwurf. Entwurf A ist wortgleich der
// :checked-Stil, den der WNB-Knopf im selben Panel schon nutzt
// (Zeile ~892) -- die Hausstil-Regel 2 in Reinform ("aktiv gefuellt").
// Entwurf B ist die zurueckhaltendere Alternative: nur ein Rahmen,
// keine Flaeche.

#include <QtTest>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "gui/StyleConstants.h"

using namespace Longpath;

namespace {

// Wortgleich aus SpectrumOverlayPanel.cpp uebernommen.
constexpr int kBandBtnW = 48;
constexpr int kBandBtnH = 26;
struct BandEntry { const char* label; const char* name; };
constexpr BandEntry kBands[] = {
    {"160", "160m"}, {"80", "80m"}, {"60", "60m"}, {"40", "40m"},
    {"30", "30m"},   {"20", "20m"}, {"17", "17m"}, {"15", "15m"},
    {"12", "12m"},   {"10", "10m"}, {"6", "6m"},   {"WWV", "WWV"},
};
constexpr int kBandCount = static_cast<int>(sizeof(kBands) / sizeof(kBands[0]));
constexpr int kCols = 4;

// Wortgleich aus buildBandFlyout() uebernommen (Zeile ~709-714).
const QString kBandBtnStyleBase =
    "QPushButton { background: rgba(30, 40, 55, 220); "
    "border: 1px solid #304050; border-radius: 6px; "
    "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: rgba(0, 112, 192, 180); "
    "border: 1px solid #4a7ba8; }";

enum class Entwurf { Gefuellt, NurRahmen };

QString checkedRuleFor(Entwurf e)
{
    switch (e) {
    case Entwurf::Gefuellt:
        // Wortgleich der WNB-Knopf-Regel (SpectrumOverlayPanel.cpp ~892).
        return QStringLiteral(
            "QPushButton:checked { background: #4a7ba8; color: #ffffff; "
            "border: 1px solid #4a7ba8; }");
    case Entwurf::NurRahmen:
        return QStringLiteral(
            "QPushButton:checked { background: rgba(30, 40, 55, 220); "
            "color: #4a7ba8; border: 2px solid #4a7ba8; }");
    }
    return {};
}

// Baut das Band-Flyout-Raster nach, mit einem aktiven Band.
QWidget* bauenRaster(Entwurf e, const QString& aktivesBand)
{
    auto* panel = new QWidget;
    panel->setStyleSheet(QStringLiteral(
        "background: #12141a; border: 1px solid #304050; border-radius: 4px;"));

    auto* grid = new QGridLayout(panel);
    grid->setContentsMargins(2, 2, 2, 2);
    grid->setSpacing(2);

    const QString style = kBandBtnStyleBase + checkedRuleFor(e);

    for (int i = 0; i < kBandCount; ++i) {
        const int row = i / kCols;
        const int col = i % kCols;
        auto* btn = new QPushButton(QString::fromLatin1(kBands[i].label), panel);
        btn->setFixedSize(kBandBtnW, kBandBtnH);
        btn->setCheckable(true);
        btn->setStyleSheet(style);
        if (QString::fromLatin1(kBands[i].name) == aktivesBand) {
            btn->setChecked(true);
        }
        grid->addWidget(btn, row, col);
    }
    panel->adjustSize();
    return panel;
}

// Ein Blatt: Titel, dann je Betriebsfall eine Beschriftungszeile plus
// das gerenderte Raster, wortgleiches Verfahren wie
// tst_tx_entwurf_sheet.cpp (WA_DontShowOnScreen + render()).
QImage blatt(Entwurf e, const QString& kopf)
{
    struct Fall { const char* aktiv; const char* beschriftung; };
    const Fall faelle[] = {
        {"40m", "Aktives Band in der Mitte des Rasters (40m)"},
        {"WWV", "Aktives Band am Rand (WWV, letzte Kachel)"},
    };

    const int panelW = kCols * kBandBtnW + (kCols - 1) * 2 + 4;   // Raster + Spacing + Margins
    const int panelH = 3 * kBandBtnH + 2 * 2 + 4;
    const int kopfH = 26;
    const int beschriftungH = 16;
    const int rand = 12;
    const int zeile = beschriftungH + panelH + 14;
    // Der Kopftext ist breiter als das Raster -- die Leinwand muss ihn
    // fassen, sonst wird er abgeschnitten (das Raster selbst bleibt bei
    // seiner wirklichen Groesse, nur der Rand rechts wird groesser).
    const int bildW = qMax(panelW + 2 * rand, 460);

    QImage img(qRound(bildW * 2.0),
               qRound((kopfH + 2 * zeile + rand) * 2.0),
               QImage::Format_ARGB32);
    img.setDevicePixelRatio(2.0);
    img.fill(QColor(Style::kAppBg));

    QPainter p(&img);
    QFont f = p.font();
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(Style::kTitleText));
    p.drawText(QRect(rand, 4, bildW - 2 * rand, kopfH), Qt::AlignLeft | Qt::AlignVCenter, kopf);

    for (int i = 0; i < 2; ++i) {
        const int y = kopfH + i * zeile;
        f.setPixelSize(10);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        p.drawText(QRect(rand, y, bildW - 2 * rand, beschriftungH), Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromUtf8(faelle[i].beschriftung));

        QWidget* w = bauenRaster(e, QString::fromUtf8(faelle[i].aktiv));
        w->setAttribute(Qt::WA_DontShowOnScreen);
        w->show();
        QCoreApplication::processEvents();
        w->render(&p, QPoint(rand, y + beschriftungH), QRegion(),
                  QWidget::DrawWindowBackground | QWidget::DrawChildren);
        delete w;
    }
    return img;
}

} // namespace

class TstBandFlyoutHighlightEntwurf : public QObject
{
    Q_OBJECT
private slots:
    void blaetter()
    {
        struct { Entwurf e; const char* kopf; const char* datei; } e[] = {
            {Entwurf::Gefuellt,
             "Entwurf A — Gefuellt (wie der WNB-Knopf im selben Panel)",
             "/tmp/band_flyout_A_gefuellt.png"},
            {Entwurf::NurRahmen,
             "Entwurf B — Nur Rahmen (zurueckhaltender)",
             "/tmp/band_flyout_B_rahmen.png"},
        };
        for (auto& x : e) {
            const QImage img = blatt(x.e, QString::fromUtf8(x.kopf));
            QVERIFY2(img.save(QString::fromUtf8(x.datei)), x.datei);
            qInfo().noquote() << "Blatt:" << x.datei;
        }
    }
};

QTEST_MAIN(TstBandFlyoutHighlightEntwurf)
#include "tst_band_flyout_highlight_entwurf.moc"
