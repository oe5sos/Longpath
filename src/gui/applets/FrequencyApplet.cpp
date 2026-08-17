// =================================================================
// src/gui/applets/FrequencyApplet.cpp  (NereusSDR)
// =================================================================
// Siehe FrequencyApplet.h.
// =================================================================

#include "gui/applets/FrequencyApplet.h"

#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QVBoxLayout>

namespace NereusSDR {

FrequencyApplet::FrequencyApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    m_instrument = new FrequencyInstrument(this);
    lay->addWidget(m_instrument);

    // Das Widget schreibt NICHT selbst. Es meldet die gewuenschte
    // Frequenz, und der Weg dorthin ist derselbe wie bei jeder anderen
    // Abstimmung — SliceModel::setFrequency, mit allem, was daran
    // haengt. Stellenweises Drehen fuehrt so wenig ueber eine
    // Bandkante wie das Rad auf der Flagge.
    connect(m_instrument, &FrequencyInstrument::frequencyEdited,
            this, [this](double hz) {
        if (m_slice) { m_slice->setFrequency(hz); }
    });
}

void FrequencyApplet::bindSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    if (m_slice) { disconnect(m_slice, nullptr, this, nullptr); }
    m_slice = slice;
    if (!m_slice) { return; }

    connect(slice, &SliceModel::frequencyChanged, this, [this](double hz) {
        m_instrument->setFrequency(hz);
    });
    m_instrument->setFrequency(slice->frequency());
    syncFromModel();
}

void FrequencyApplet::syncFromModel()
{
    if (!m_model || !m_slice) { return; }
    // Die zweite Scheibe fuer die VFO-Zeile. Gibt es keine, steht dort
    // dieselbe Zahl zweimal — das ist ehrlicher als eine erfundene B.
    SliceModel* other = nullptr;
    for (SliceModel* s : m_model->slices()) {
        if (s && s != m_slice) { other = s; break; }
    }
    m_instrument->setOtherFrequency(other ? other->frequency()
                                          : m_slice->frequency());
    m_instrument->setActiveIsThis(true);
}

} // namespace NereusSDR
