// =================================================================
// src/gui/applets/FrequencyApplet.cpp  (NereusSDR)
// =================================================================
// Siehe FrequencyApplet.h.
// =================================================================

#include "gui/applets/FrequencyApplet.h"

#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "core/AppSettings.h"
#include "gui/meters/MeterPoller.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QVBoxLayout>

namespace Longpath {

FrequencyApplet::FrequencyApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    m_instrument = new FrequencyInstrument(this);
    lay->addWidget(m_instrument);

    // ── Die beiden Zusatzzeilen ──────────────────────────────────────
    //
    // Sie werden IMMER gebaut, aber nur auf Wunsch gezeigt. Der Grund
    // ist Erfahrung aus dieser Woche: ein Widget, das seine Kinder
    // erst beim Einschalten anlegt, muss dabei das Layout umbauen —
    // und genau daran hat der Panadapter beim Abloesen gezuckt. Ein
    // setVisible ist ein Zustand, kein Umbau.
    //
    // Es sind gewoehnliche BarInstrument, also gelten fuer sie
    // dieselben Formen wie fuer die eigenstaendigen Anzeigen: Segmente
    // und die Roehre lassen sich hier genauso einstellen.
    m_powerBar = new BarInstrument(this);
    m_powerBar->setPrimary(MeterBinding::TxPower);
    lay->addWidget(m_powerBar);

    m_swrBar = new BarInstrument(this);
    m_swrBar->setPrimary(MeterBinding::TxSwr);
    lay->addWidget(m_swrBar);

    applyRowVisibility();

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



void FrequencyApplet::applyRowVisibility()
{
    if (m_powerBar) { m_powerBar->setVisible(m_showPower); }
    if (m_swrBar)   { m_swrBar->setVisible(m_showSwr); }
}

void FrequencyApplet::setShowPower(bool on)
{
    if (m_showPower == on) { return; }
    m_showPower = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::setShowSwr(bool on)
{
    if (m_showSwr == on) { return; }
    m_showSwr = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::onReading(int bindingId, double value)
{
    // Beide bekommen jeden Wert; BarInstrument sortiert selbst aus, ob
    // die Kennung seine ist. Auch die unsichtbare Zeile wird gefuettert
    // — sonst zeigte sie beim Einschalten erst einmal nichts, und der
    // Betreiber haette einen halben Meterzyklus lang ein leeres Feld.
    if (m_powerBar) { m_powerBar->onReading(bindingId, value); }
    if (m_swrBar)   { m_swrBar->onReading(bindingId, value); }
}

namespace {
QString powerKey() { return QStringLiteral("FrequencyApplet_ShowPower"); }
QString swrKey()   { return QStringLiteral("FrequencyApplet_ShowSwr"); }
} // namespace

void FrequencyApplet::saveState() const
{
    auto& st = AppSettings::instance();
    st.setValue(powerKey(), m_showPower ? QStringLiteral("True")
                                        : QStringLiteral("False"));
    st.setValue(swrKey(), m_showSwr ? QStringLiteral("True")
                                    : QStringLiteral("False"));
}

void FrequencyApplet::restoreState()
{
    auto& st = AppSettings::instance();
    m_showPower = st.value(powerKey(), QStringLiteral("False")).toString()
                  == QStringLiteral("True");
    m_showSwr = st.value(swrKey(), QStringLiteral("False")).toString()
                == QStringLiteral("True");
    applyRowVisibility();
}

void FrequencyApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu menu(this);

    // Haken, keine Punkte: die beiden Zeilen sind unabhaengig. Ein
    // Auswahlkranz aus "keine / Stehwelle / SWR / beide" waere
    // dieselbe Information in vier Zeilen statt zwei und liesse sich
    // schlechter erweitern.
    QAction* pwr = menu.addAction(tr("Stehwelle anzeigen"));
    pwr->setCheckable(true);
    pwr->setChecked(m_showPower);
    connect(pwr, &QAction::triggered, this,
            [this](bool on) { setShowPower(on); });

    QAction* swr = menu.addAction(tr("SWR anzeigen"));
    swr->setCheckable(true);
    swr->setChecked(m_showSwr);
    connect(swr, &QAction::triggered, this,
            [this](bool on) { setShowSwr(on); });

    // Die Form der Zusatzzeilen — nur anbieten, wenn ueberhaupt eine
    // steht. Ein Menuepunkt fuer etwas Unsichtbares ist eine Falle.
    if (m_showPower || m_showSwr) {
        menu.addSeparator();
        auto* form = menu.addMenu(tr("Zusatzzeilen"));
        QAction* seg = form->addAction(tr("Segmente"));
        seg->setCheckable(true);
        seg->setChecked(m_powerBar && m_powerBar->isSegmented());
        connect(seg, &QAction::triggered, this, [this](bool on) {
            if (m_powerBar) { m_powerBar->setSegmented(on); }
            if (m_swrBar)   { m_swrBar->setSegmented(on); }
        });
        QAction* tube = form->addAction(tr("Roehre (3D)"));
        tube->setCheckable(true);
        tube->setChecked(m_powerBar && m_powerBar->isTube());
        connect(tube, &QAction::triggered, this, [this](bool on) {
            if (m_powerBar) { m_powerBar->setTube(on); }
            if (m_swrBar)   { m_swrBar->setTube(on); }
        });
    }

    menu.exec(ev->globalPos());
    ev->accept();
}

} // namespace Longpath
