// =================================================================
// src/gui/applets/InstrumentApplet.cpp  (NereusSDR)
// =================================================================
// Siehe InstrumentApplet.h.
// =================================================================

#include "gui/applets/InstrumentApplet.h"

#include "core/AppSettings.h"

#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "models/RadioModel.h"

namespace Longpath {

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

    // Ruhelage ohne Radio (Betreiber-Entscheidung 2026-08-18). Das
    // Instrument selbst kennt kein Radio — es kennt Messwerte. Die
    // Verbindung ist Wissen der Applet, also wird sie hier
    // durchgereicht und nicht im Instrument abgefragt.
    if (m_model) {
        auto push = [this] {
            const bool offline =
                m_model->connectionState() != ConnectionState::Connected;
            m_needle->setOffline(offline);
        };
        connect(m_model, &RadioModel::connectionStateChanged,
                this, [push](Longpath::ConnectionState) { push(); });
        push();
    }
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

const PeakHold& InstrumentApplet::peakHold(Form f) const
{
    return f == Form::Needle ? m_needle->peakHold() : m_bar->peakHold();
}

void InstrumentApplet::setForm(Form f)
{
    m_form = f;
    m_stack->setCurrentWidget(f == Form::Needle
                                  ? static_cast<QWidget*>(m_needle)
                                  : static_cast<QWidget*>(m_bar));
    saveState();
}

QImage InstrumentApplet::renderTransparent(int sidePx, qreal dpr)
{
    if (sidePx < 24) { return {}; }

    // Das Seitenverhaeltnis der Ansicht behalten. Ein Zeigerbogen ist
    // breiter als hoch; in ein Quadrat gepresst waere er verzogen.
    QWidget* view = (m_form == Form::Needle)
                        ? static_cast<QWidget*>(m_needle)
                        : static_cast<QWidget*>(m_bar);
    if (!view) { return {}; }

    const QSize live = view->size();
    const double ar = (live.height() > 0 && live.width() > 0)
                          ? double(live.width()) / double(live.height())
                          : 1.5;
    const QSize want(sidePx, qMax(24, int(std::lround(sidePx / ar))));

    QImage img(want * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);

    // Kein resize() mehr auf das lebende Instrument. Der Kommentar hier
    // behauptete frueher, die Spalte merke davon nichts — sie merkt es
    // sehr wohl: ein resize() auf ein Widget in einem Layout ist ein
    // Eingriff, und die Einblendung malt alle 500 ms neu. Beim Rotor
    // hat genau das die Rose plattgedrueckt.
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_form == Form::Needle && m_needle) {
        m_needle->paintInto(p, want, true);
    } else if (m_bar) {
        m_bar->paintInto(p, want, true);
    }
    p.end();
    return img;
}

void InstrumentApplet::setSegmented(bool on)
{
    if (m_bar) { m_bar->setSegmented(on); }
    saveState();
}

bool InstrumentApplet::isSegmented() const
{
    return m_bar && m_bar->isSegmented();
}

// ── Merken ──────────────────────────────────────────────────────────
//
// Ein Schluessel je Instrument: „SwrInstrument" und „SignalInstrument"
// sollen verschiedene Formen haben duerfen.
namespace {
QString formKey(const QString& id) {
    return QStringLiteral("Instrument_%1_Form").arg(id);
}
QString segKey(const QString& id) {
    return QStringLiteral("Instrument_%1_BarSegments").arg(id);
}
} // namespace

void InstrumentApplet::saveState() const
{
    auto& st = AppSettings::instance();
    st.setValue(formKey(m_id), m_form == Form::Bar
                                   ? QStringLiteral("Bar")
                                   : QStringLiteral("Needle"));
    st.setValue(segKey(m_id), isSegmented() ? QStringLiteral("True")
                                            : QStringLiteral("False"));
}

void InstrumentApplet::restoreState()
{
    auto& st = AppSettings::instance();
    if (m_bar) {
        m_bar->setSegmented(
            st.value(segKey(m_id), QStringLiteral("False")).toString()
            == QStringLiteral("True"));
    }
    // setForm zuletzt: es ruft saveState, und das soll den eben
    // gelesenen Segmentzustand mitschreiben, nicht den vorigen.
    const QString f = st.value(formKey(m_id), QStringLiteral("Needle")).toString();
    setForm(f == QStringLiteral("Bar") ? Form::Bar : Form::Needle);
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

// ── Das Rechtsklickmenü ──────────────────────────────────────────────

void InstrumentApplet::forEachInstrument(
    const std::function<void(PeakHold&)>& fn)
{
    fn(m_needle->peakHold());
    fn(m_bar->peakHold());
}

void InstrumentApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = buildContextMenu(this);
    menu->exec(ev->globalPos());
    menu->deleteLater();
}

QMenu* InstrumentApplet::buildContextMenu(QWidget* parent)
{
    auto* menu = new QMenu(parent);

    // ── Quelle ───────────────────────────────────────────────────────
    //
    // readingsWithScale(), nicht eine eigene Liste. Wer eine Grösse
    // hinzufügt, fügt sie in ReadingSource hinzu und findet sie hier
    // ohne weiteres Zutun — und ein Instrument bietet nie etwas an,
    // wofür es keinen verantworteten Bereich gibt.
    auto* sourceMenu = menu->addMenu(tr("Quelle"));
    auto* sourceGroup = new QActionGroup(menu);
    sourceGroup->setExclusive(true);
    for (const ReadingDescriptor* d : readingsWithScale()) {
        QAction* a = sourceMenu->addAction(d->label);
        a->setCheckable(true);
        a->setChecked(d->bindingId == m_needle->primary());
        sourceGroup->addAction(a);
        const int id = d->bindingId;
        connect(a, &QAction::triggered, this, [this, id]() {
            setPrimary(id);
        });
    }

    // ── Zweite Anzeige ───────────────────────────────────────────────
    auto* secondMenu = menu->addMenu(tr("Zweite Anzeige"));
    auto* secondGroup = new QActionGroup(menu);
    secondGroup->setExclusive(true);
    {
        QAction* none = secondMenu->addAction(tr("keine"));
        none->setCheckable(true);
        none->setChecked(m_needle->secondary() < 0);
        secondGroup->addAction(none);
        connect(none, &QAction::triggered, this, [this]() {
            setSecondary(-1);
        });
        secondMenu->addSeparator();
    }
    for (const ReadingDescriptor* d : readingsWithScale()) {
        QAction* a = secondMenu->addAction(d->label);
        a->setCheckable(true);
        a->setChecked(d->bindingId == m_needle->secondary());
        secondGroup->addAction(a);
        const int id = d->bindingId;
        connect(a, &QAction::triggered, this, [this, id]() {
            setSecondary(id);
        });
    }

    // ── Form ─────────────────────────────────────────────────────────
    auto* formMenu = menu->addMenu(tr("Form"));
    auto* formGroup = new QActionGroup(menu);
    formGroup->setExclusive(true);
    const struct { const char* label; Form form; } kForms[] = {
        {QT_TR_NOOP("Zeiger"), Form::Needle},
        {QT_TR_NOOP("Balken"), Form::Bar},
    };
    for (const auto& f : kForms) {
        QAction* a = formMenu->addAction(tr(f.label));
        a->setCheckable(true);
        a->setChecked(m_form == f.form);
        formGroup->addAction(a);
        const Form target = f.form;
        connect(a, &QAction::triggered, this, [this, target]() {
            setForm(target);
        });
    }

    // ── Balkenform ───────────────────────────────────────────────────
    //
    // Nur wenn der Balken auch gezeigt wird: ein Untermenue, das den
    // Zeiger nicht betrifft, gehoert nicht in dessen Menue. Der
    // Betreiber hat die Segmentform am 2026-08-20 mit einem
    // Bildschirmfoto aus Zeus verlangt.
    if (m_form == Form::Bar && m_bar) {
        auto* styleMenu = menu->addMenu(tr("Balken"));
        auto* styleGroup = new QActionGroup(menu);
        styleGroup->setExclusive(true);
        const struct { const char* label; bool seg; } kStyles[] = {
            {QT_TR_NOOP("Durchgehend"), false},
            {QT_TR_NOOP("Segmente"),    true},
        };
        for (const auto& st : kStyles) {
            QAction* a = styleMenu->addAction(tr(st.label));
            a->setCheckable(true);
            a->setChecked(m_bar->isSegmented() == st.seg);
            styleGroup->addAction(a);
            const bool seg = st.seg;
            connect(a, &QAction::triggered, this, [this, seg]() {
                setSegmented(seg);
            });
        }
    }

    // ── Spitzenhaltung ───────────────────────────────────────────────
    auto* peakMenu = menu->addMenu(tr("Spitzenhaltung"));
    {
        QAction* on = peakMenu->addAction(tr("Anzeigen"));
        on->setCheckable(true);
        on->setChecked(m_needle->peakHold().enabled());
        connect(on, &QAction::triggered, this, [this](bool checked) {
            forEachInstrument([checked](PeakHold& p) { p.setEnabled(checked); });
            update();
        });
    }
    auto* holdMenu = peakMenu->addMenu(tr("Halten"));
    auto* holdGroup = new QActionGroup(menu);
    holdGroup->setExclusive(true);
    const struct { const char* label; int ms; } kHolds[] = {
        {QT_TR_NOOP("kurz"),   PeakHold::kShortMs},
        {QT_TR_NOOP("mittel"), PeakHold::kMediumMs},
        {QT_TR_NOOP("lang"),   PeakHold::kLongMs},
    };
    for (const auto& h : kHolds) {
        QAction* a = holdMenu->addAction(
            QStringLiteral("%1 (%2 s)").arg(tr(h.label)).arg(h.ms / 1000));
        a->setCheckable(true);
        a->setChecked(m_needle->peakHold().holdMs() == h.ms);
        holdGroup->addAction(a);
        const int ms = h.ms;
        connect(a, &QAction::triggered, this, [this, ms]() {
            forEachInstrument([ms](PeakHold& p) { p.setHoldMs(ms); });
        });
    }
    peakMenu->addSeparator();
    connect(peakMenu->addAction(tr("Zurücksetzen")), &QAction::triggered,
            this, [this]() {
        m_needle->resetPeak();
        m_bar->resetPeak();
    });

    return menu;
}

} // namespace Longpath
