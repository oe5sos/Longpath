// =================================================================
// src/gui/applets/InstrumentApplet.cpp  (NereusSDR)
// =================================================================
// Siehe InstrumentApplet.h.
// =================================================================

#include "gui/applets/InstrumentApplet.h"

#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/NeedleInstrument.h"

#include <QStackedWidget>
#include <QVBoxLayout>

namespace NereusSDR {

InstrumentApplet::InstrumentApplet(const QString& id, const QString& title,
                                   RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
    , m_id(id)
    , m_title(title)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_needle = new NeedleInstrument(m_stack);
    m_bar    = new BarInstrument(m_stack);
    m_stack->addWidget(m_needle);
    m_stack->addWidget(m_bar);
    lay->addWidget(m_stack);

    setForm(Form::Needle);
}

bool InstrumentApplet::setPrimary(int bindingId)
{
    // Beide Formen bekommen dieselbe Grösse. Das && ist kein Tippfehler
    // und keine Kurzschluss-Falle: setPrimary hat keine Nebenwirkung,
    // die eine der beiden auslassen dürfte, deshalb erst beide rufen
    // und dann verknüpfen.
    const bool a = m_needle->setPrimary(bindingId);
    const bool b = m_bar->setPrimary(bindingId);
    return a && b;
}

bool InstrumentApplet::setSecondary(int bindingId)
{
    const bool a = m_needle->setSecondary(bindingId);
    const bool b = m_bar->setSecondary(bindingId);
    return a && b;
}

void InstrumentApplet::setForm(Form f)
{
    m_form = f;
    m_stack->setCurrentWidget(f == Form::Needle
                                  ? static_cast<QWidget*>(m_needle)
                                  : static_cast<QWidget*>(m_bar));
}

void InstrumentApplet::onReading(int bindingId, double value)
{
    // BEIDE bekommen den Wert, auch die gerade unsichtbare. Sonst
    // stünde die andere Form beim Umschalten auf ihrem letzten Wert,
    // und der Betreiber sähe für einen Augenblick eine alte Messung —
    // genau der Sprung, den die gemeinsame Fusszeile vermeiden soll.
    m_needle->onReading(bindingId, value);
    m_bar->onReading(bindingId, value);
}

} // namespace NereusSDR
