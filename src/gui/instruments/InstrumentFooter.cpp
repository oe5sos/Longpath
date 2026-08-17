// =================================================================
// src/gui/instruments/InstrumentFooter.cpp  (NereusSDR)
// =================================================================
// Siehe InstrumentFooter.h — eine Fusszeile für beide Instrumente.
// =================================================================

#include "gui/instruments/InstrumentFooter.h"

#include "gui/StyleConstants.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/styles/ThemeQss.h"

#include <QHBoxLayout>
#include <QLabel>

namespace NereusSDR {

InstrumentFooter::InstrumentFooter(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 7, 0, 0);
    lay->setSpacing(14);

    // Versalzeile, Laufweite .18em — HAUSSTIL.md §Die acht Regeln,
    // Regel 1. Qt-Stylesheets kennen kein letter-spacing und verwerfen
    // es kommentarlos; die Laufweite muss über QFont gesetzt werden
    // (Vorlage: makeGroupHead() in SpectrumOverlayPanel.cpp).
    m_caption = new QLabel(this);
    // Versalzeile ueber Style::capsFont — Groesse, Halbfett,
    // Grossbuchstaben und Laufweite an einer Stelle. Sie stand hier als
    // 8 px mit von Hand gerechneter Laufweite; 9 ist die benannte
    // Stufe und liegt weiterhin im Bereich, den HAUSSTIL.md nennt.
    m_caption->setFont(Style::capsFont(m_caption->font()));
    m_caption->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; background: transparent; }")
            .arg(Style::kTextScale)));
    lay->addWidget(m_caption, 0, Qt::AlignBaseline);

    m_middle = new QLabel(this);
    // Zahlen, die sich aendern -> Monospace (HAUSSTIL Regel 2).
    m_middle->setFont(Style::monoFont(m_middle->font(), Style::kFontSmall));
    m_middle->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; background: transparent; }")
            .arg(Instrument::measuredDim().name())));
    lay->addWidget(m_middle, 1, Qt::AlignBaseline);

    m_value = new QLabel(this);
    m_value->setFont(Style::monoFont(m_value->font(), Style::kFontReading,
                                     QFont::DemiBold));
    setValueColour(QColor(Style::role("measured", Style::kAmberText)));
    lay->addWidget(m_value, 0, Qt::AlignBaseline);
}

void InstrumentFooter::setCaption(const QString& text)
{
    m_caption->setText(text);
}

void InstrumentFooter::setValueText(const QString& text)
{
    m_value->setText(text);
}

void InstrumentFooter::setValueColour(const QColor& c)
{
    m_value->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: transparent; }")
            .arg(c.name()));
}

void InstrumentFooter::setPeakAndLimit(const QString& peakText,
                                       const QString& limitText)
{
    QStringList parts;
    if (!peakText.isEmpty())  { parts << QStringLiteral("Spitze %1").arg(peakText); }
    if (!limitText.isEmpty()) { parts << QStringLiteral("Grenze %1").arg(limitText); }
    m_middle->setText(parts.join(QStringLiteral(" · ")));
}

} // namespace NereusSDR
