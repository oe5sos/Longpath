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
    {
        QFont f = m_caption->font();
        f.setPixelSize(8);
        f.setWeight(QFont::DemiBold);
        f.setCapitalization(QFont::AllUppercase);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 8.0 * 0.18);
        m_caption->setFont(f);
    }
    m_caption->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; background: transparent; }")
            .arg(Style::kTextScale)));
    lay->addWidget(m_caption, 0, Qt::AlignBaseline);

    m_middle = new QLabel(this);
    {
        QFont f = m_middle->font();
        f.setPixelSize(11);
        f.setFamily(QStringLiteral("Menlo"));   // Zahlen, die sich ändern
        m_middle->setFont(f);
    }
    m_middle->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; background: transparent; }")
            .arg(Instrument::measuredDim().name())));
    lay->addWidget(m_middle, 1, Qt::AlignBaseline);

    m_value = new QLabel(this);
    {
        QFont f = m_value->font();
        f.setPixelSize(22);
        f.setWeight(QFont::DemiBold);
        f.setFamily(QStringLiteral("Menlo"));
        m_value->setFont(f);
    }
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
