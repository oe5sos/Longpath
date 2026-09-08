// src/gui/HGauge.cpp
#include "gui/styles/ThemeQss.h"
#include "HGauge.h"
#include "StyleConstants.h"
#include <QPainter>
#include <QLocale>
#include <QPaintEvent>

namespace Longpath {

// Die gedaempfte Stufe des Messwert-Tons. Dieselbe Zahl wie beim
// Pegelbalken unter S9 (VfoStyles.h) -- eine Groesse, zwei
// Helligkeiten, kein zweiter Farbbegriff.
// Siehe Style::kAmberDim — dieser dateilokale Zwilling bleibt nur als
// Rueckfallwert fuer role() stehen.
static constexpr auto kMeasuredDim = Style::kAmberDim;

HGauge::HGauge(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HGauge::setRange(double min, double max) { m_min = min; m_max = max; update(); }
void HGauge::setYellowStart(double val) { m_yellowStart = val; update(); }
void HGauge::setRedStart(double val) { m_redStart = val; update(); }
void HGauge::setReversed(bool rev) { m_reversed = rev; update(); }
void HGauge::setTitle(const QString& t) { m_title = t; update(); }
void HGauge::setUnit(const QString& u) { m_unit = u; update(); }
void HGauge::setValue(double val) {
    // Value-change guard: skip the repaint when the new sample matches the
    // current one within FP noise.  HGauge is driven by many fast timers
    // (TxApplet fwd / SWR at 20 Hz, PhoneCwApplet mic at 20 Hz, VaxApplet
    // levels at 60 Hz aggregate, AudioTxInputPage VU at 100 Hz, etc.).  When
    // the signal is quiet or steady, suppressing the redundant update() cuts
    // a large chunk of the main-thread paint load that was driving the
    // QCALayerBackingStore::recreateBackBufferIfNeeded thrash on macOS.
    if (qFuzzyCompare(1.0 + m_value, 1.0 + val)) { return; }
    m_value = val;
    update();
}
void HGauge::setPeakValue(double val) {
    if (qFuzzyCompare(1.0 + m_peak, 1.0 + val)) { return; }
    m_peak = val;
    update();
}
void HGauge::setTickLabels(const QStringList& labels) { m_tickLabels = labels; update(); }

void HGauge::setReadout(bool on, int nachkommastellen, const QString& einheit)
{
    m_readout = on;
    m_readoutDecimals = nachkommastellen;
    m_readoutUnit = einheit;
    // Die ablesbare Bauform braucht weniger Hoehe als der Streifen mit
    // Teilung darunter: keine Skalenbeschriftung, dafuer die Zahl auf
    // derselben Zeile.
    setFixedHeight(on ? 24 : 30);
    update();
}

void HGauge::setLabelWidth(int px) { m_labelWidth = px; update(); }

// Die Zahl rechts. Am Skalenanfang steht ein Gedankenstrich: „0,0 W"
// und „1,0" behaupten eine Messung, die es ohne Sendung nicht gibt.
static QString readoutText(double v, double min, int decimals,
                           const QString& unit)
{
    if (v <= min) { return QStringLiteral("\u2014"); }
    QString s = QLocale::system().toString(v, 'f', decimals);
    if (!unit.isEmpty()) { s += QLatin1Char(' ') + unit; }
    return s;
}

void HGauge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_readout) { paintReadout(p); return; }

    const int w = width();
    const int h = height();
    const int barH = 12;
    const int barY = (h - barH) / 2;
    const int barX = 2;
    const int barW = w - 4;

    // ── Die Rille ist eine Mulde ────────────────────────────────────
    //
    // Wie ein Eingabefeld: Grund dunkler als das Panel, Rand sichtbar.
    // kBorderSubtle lag zu nah an kInsetBg -- zwei fast gleiche
    // Grautoene, und der leere Balken verschwand. Derselbe Befund wie auf
    // Startup & Preferences.
    p.setPen(QColor(Style::role("border", Style::kBorder)));
    p.setBrush(QColor(Style::role("inset-bg", Style::kInsetBg)));
    p.drawRoundedRect(barX, barY, barW, barH, 2, 2);

    if (m_max <= m_min) { return; }

    const double range = m_max - m_min;
    const double normalized = qBound(0.0, (m_value - m_min) / range, 1.0);

    if (m_reversed) {
        const int fillW = static_cast<int>(normalized * barW);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Style::role("danger", Style::kGaugeDanger)));
        p.drawRoundedRect(barX + barW - fillW, barY + 1, fillW, barH - 2, 1, 1);
    } else {
        const int fillW = static_cast<int>(normalized * barW);
        if (fillW > 0) {
            const double yellowNorm = (m_yellowStart - m_min) / range;
            const double redNorm = (m_redStart - m_min) / range;
            const int yellowX = static_cast<int>(yellowNorm * barW);
            const int redX = static_cast<int>(redNorm * barW);

            int normalEnd = qMin(fillW, yellowX);
            if (normalEnd > 0) {
                p.setPen(Qt::NoPen);
                // Was der Balken zeigt, ist gemessen -- also Bernstein,
                // nicht Akzentblau. kGaugeNormal war #4a7ba8, der Ton
                // fuer "anfassbar".
                p.setBrush(QColor(Style::role("measured-dim", kMeasuredDim)));
                p.drawRoundedRect(barX + 1, barY + 1, normalEnd, barH - 2, 1, 1);
            }
            if (fillW > yellowX) {
                int warnEnd = qMin(fillW, redX) - yellowX;
                if (warnEnd > 0) {
                    // Zweite Helligkeit DERSELBEN Groesse, kein zweiter
                    // Farbbegriff -- wie unter und ab S9 beim Pegelbalken.
                    p.setBrush(QColor(Style::role("measured", Style::kAmberText)));
                    p.drawRect(barX + 1 + yellowX, barY + 1, warnEnd, barH - 2);
                }
            }
            if (fillW > redX) {
                int dangerW = fillW - redX;
                // Ab hier ist es keine Messung mehr, sondern eine
                // Warnung. Die Schwelle setzt das Applet
                // (setRedStart): SWR ueber 2,5 und Vorlauf ueber 100 W
                // laut TxApplet.h, beim Verstaerker 1500 W. Bleibt
                // kraeftig.
                p.setBrush(QColor(Style::role("danger", Style::kGaugeDanger)));
                p.drawRect(barX + 1 + redX, barY + 1, dangerW, barH - 2);
            }
        }
    }

    // Peak hold marker
    if (m_peak > m_min) {
        const double peakNorm = qBound(0.0, (m_peak - m_min) / range, 1.0);
        const int peakX = barX + 1 + static_cast<int>(peakNorm * (barW - 2));
        p.setPen(QPen(QColor(Style::role("instrument-face", Style::kInstrumentFace)), 1));
        p.drawLine(peakX, barY + 1, peakX, barY + barH - 2);
    }

    // Center title
    if (!m_title.isEmpty()) {
        // Beschriftung und Teilung gehoeren zum Instrument.
        p.setPen(QColor(Style::role("instrument-face", Style::kInstrumentFace)));
        p.setFont(QFont(p.font().family(), 8, QFont::Bold));
        p.drawText(QRect(barX, barY, barW, barH), Qt::AlignCenter, m_title);
    }

    // Tick labels along bottom
    if (!m_tickLabels.isEmpty()) {
        p.setFont(QFont(p.font().family(), 7));
        const int n = m_tickLabels.size();
        for (int i = 0; i < n; ++i) {
            double frac = static_cast<double>(i) / (n - 1);
            int x = barX + static_cast<int>(frac * barW);
            // Die Teilung ist Instrument, nicht Messwert -- sie steht
            // still, waehrend der Balken wandert. Nur jenseits der
            // Schwelle bekommt sie die Warnfarbe, damit man sieht, WO
            // die Grenze liegt, bevor der Balken sie erreicht.
            QColor col(Style::role("instrument-face", Style::kInstrumentFace));
            double val = m_min + frac * range;
            if (val >= m_redStart) {
                col = QColor(Style::role("danger", Style::kGaugeDanger));
            }
            p.setPen(col);
            p.drawText(QRect(x - 20, barY + barH + 1, 40, 10),
                       Qt::AlignHCenter | Qt::AlignTop, m_tickLabels[i]);
        }
    }
}

// ── Beschriftung links, Mulde, Zahl rechts ──────────────────────────
//
// Dieselben drei Zonen wie oben (gemessen / Grenze / zu viel), nur
// ohne Beschriftung IN der Mulde — die stuende sonst unter der
// Fuellung und waere ab etwa zwei Dritteln nicht mehr lesbar. Genau
// das ist am Entwurfsblatt vom 2026-09-02 an der SWR-Anzeige
// aufgefallen.
void HGauge::paintReadout(QPainter& p)
{
    const int w = width();
    const int h = height();
    const bool ueber = (m_value >= m_redStart);

    QFont f = p.font();
    f.setPixelSize(Style::kFontCaption);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor(Style::role("text-scale", Style::kTextScale)));
    p.drawText(QRect(0, 0, m_labelWidth, h),
               Qt::AlignLeft | Qt::AlignVCenter, m_title);

    const int zahlW = 62;
    const int x = m_labelWidth + 4;
    const int barW = w - x - zahlW - 4;
    const int barH = 8;
    const int barY = (h - barH) / 2;

    if (barW > 8) {
        p.setPen(QColor(Style::role("border", Style::kBorder)));
        p.setBrush(QColor(Style::role("inset-bg", Style::kInsetBg)));
        p.drawRoundedRect(x, barY, barW, barH, 2, 2);

        const double range = m_max - m_min;
        if (range > 0.0) {
            const double norm = qBound(0.0, (m_value - m_min) / range, 1.0);
            const int fill = static_cast<int>(norm * (barW - 2));
            if (fill > 0) {
                const double yellowNorm = (m_yellowStart - m_min) / range;
                const double redNorm    = (m_redStart - m_min) / range;
                const int yellowX = static_cast<int>(yellowNorm * (barW - 2));
                const int redX    = static_cast<int>(redNorm * (barW - 2));
                p.setPen(Qt::NoPen);

                const int normalEnd = qMin(fill, yellowX);
                if (normalEnd > 0) {
                    p.setBrush(QColor(Style::role("measured-dim", Style::kAmberDim)));
                    p.drawRoundedRect(x + 1, barY + 1, normalEnd, barH - 2, 1, 1);
                }
                if (fill > yellowX) {
                    const int warnEnd = qMin(fill, redX) - yellowX;
                    if (warnEnd > 0) {
                        p.setBrush(QColor(Style::role("measured", Style::kAmberText)));
                        p.drawRect(x + 1 + yellowX, barY + 1, warnEnd, barH - 2);
                    }
                }
                if (fill > redX) {
                    p.setBrush(QColor(Style::role("danger", Style::kGaugeDanger)));
                    p.drawRect(x + 1 + redX, barY + 1, fill - redX, barH - 2);
                }
            }
        }
    }

    const QString zahl = readoutText(m_value, m_min, m_readoutDecimals,
                                     m_readoutUnit);
    const bool leer = (m_value <= m_min);
    f.setPixelSize(Style::kFontBody);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(ueber ? Style::role("danger", Style::kGaugeDanger)
                          : (leer ? Style::role("text-inactive", Style::kTextInactive)
                                  : Style::role("text-primary", Style::kTextPrimary))));
    p.drawText(QRect(w - zahlW, 0, zahlW, h),
               Qt::AlignRight | Qt::AlignVCenter, zahl);
}

} // namespace Longpath
