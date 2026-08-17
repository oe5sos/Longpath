// =================================================================
// src/gui/applets/InstrumentApplet.cpp  (NereusSDR)
// =================================================================
// Siehe InstrumentApplet.h.
// =================================================================

#include "gui/applets/InstrumentApplet.h"

#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
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

} // namespace NereusSDR
