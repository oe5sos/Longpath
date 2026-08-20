// =================================================================
// src/gui/widgets/CommandBar.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See CommandBar.h for what this is and why the
// band and filter groups are deliberately absent.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/CommandBar.h"

#include "gui/StyleConstants.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {
namespace {

/// Die Versalzeile über einer Gruppe. 8 px, weite Laufweite,
/// Skalenfarbe — sie soll benennen, nicht mitreden.
QLabel* captionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f = l->font();
    f.setPixelSize(9);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(QString::fromLatin1(Style::kTextScale)));
    return l;
}

} // namespace

/// Das Aussehen einer Pille. Über Style::-Konstanten, damit die
/// Theme-Datei sie erreicht. Oeffentlich, weil die untere Leiste
/// dieselbe Optik traegt.
QString CommandBar::pillStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %2; border-radius: %6px;"
        "  color: %3; font-size: 11px; font-weight: 600;"
        "  padding: 0 11px; min-height: %5px; max-height: %5px;"
        "}"
        "QPushButton:hover { background: %4; }"
        // Genau einer je Gruppe leuchtet. Der Rest ist fast unsichtbar —
        // das ist der Unterschied zwischen einer Leiste und einer Wand.
        "QPushButton:checked {"
        "  background: %7; border: 1px solid %8; color: %9;"
        "}")
        .arg(QString::fromLatin1(Style::kButtonBg),
             QString::fromLatin1(Style::kBorder),
             QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kButtonHover))
        .arg(CommandBar::kPillHeight)
        .arg(CommandBar::kPillRadius)
        .arg(QString::fromLatin1(Style::kBlueBg),
             QString::fromLatin1(Style::kBlueBorder),
             QString::fromLatin1(Style::kBlueText));
}

CommandBar::CommandBar(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "CommandBar { background: %1; border: 1px solid %2;"
        "             border-radius: 6px; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorderSubtle)));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(13, 9, 13, 10);
    // Der Abstand ZWISCHEN den Gruppen ist das, was sie zu Gruppen
    // macht. Fünf Pixel innerhalb, zweiundzwanzig dazwischen.
    row->setSpacing(22);

    m_row = row;
    buildModeGroup(row);
    buildStepGroup(row);
    row->addStretch(1);
}

void CommandBar::addTrailing(QWidget* w)
{
    if (!w || !m_row) { return; }
    // Rechtsbündig: die Dehnung steht schon davor, angehängt wird
    // dahinter. Die Ausrichtung unten hält das Plus auf einer Linie mit
    // den Pillen und nicht mit den Versalzeilen darüber.
    m_row->addWidget(w, 0, Qt::AlignBottom);
}

// ── Gruppen bauen ────────────────────────────────────────────────────

CommandBar::Group& CommandBar::addGroup(const QString& caption,
                                        QHBoxLayout* row)
{
    auto* box = new QVBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(5);
    box->addWidget(captionLabel(caption, this), 0, Qt::AlignLeft);

    auto* pills = new QHBoxLayout;
    pills->setContentsMargins(0, 0, 0, 0);
    pills->setSpacing(5);
    box->addLayout(pills);
    row->addLayout(box);

    auto* g = new Group{caption, pills, {}, nullptr};
    m_groups.append(g);
    return *g;
}

QPushButton* CommandBar::addPill(Group& g, const QString& label)
{
    auto* b = new QPushButton(label, this);
    b->setCheckable(true);
    b->setStyleSheet(pillStyle());
    b->setCursor(Qt::PointingHandCursor);
    g.pills.append(b);
    if (g.row) { g.row->addWidget(b); }
    return b;
}

void CommandBar::setActive(Group& g, const QString& label)
{
    for (QPushButton* b : g.pills) {
        b->setChecked(b->text() == label);
    }
}

void CommandBar::buildModeGroup(QHBoxLayout* row)
{
    // Der vollständige Satz für das „…". SPEC und DRM bleiben draußen:
    // das eine ist kein Betriebsmodus, das andere hat in diesem
    // Programm keinen Decoder.
    m_allModes = {
        {QStringLiteral("LSB"),  DSPMode::LSB},
        {QStringLiteral("USB"),  DSPMode::USB},
        {QStringLiteral("CWL"),  DSPMode::CWL},
        {QStringLiteral("CWU"),  DSPMode::CWU},
        {QStringLiteral("AM"),   DSPMode::AM},
        {QStringLiteral("SAM"),  DSPMode::SAM},
        {QStringLiteral("FM"),   DSPMode::FM},
        {QStringLiteral("DIGL"), DSPMode::DIGL},
        {QStringLiteral("DIGU"), DSPMode::DIGU},
        {QStringLiteral("DSB"),  DSPMode::DSB},
    };

    Group& g = addGroup(QStringLiteral("Mode"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allModes.size(); ++i) {
        const auto entry = m_allModes.at(i);
        QPushButton* b = addPill(g, entry.first);
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushModeToModel(entry.second);
        });
    }

    // ── Das „…" ──────────────────────────────────────────────────────
    //
    // Drei vorne, der Rest im Menü. Wählt jemand dort etwas aus, tritt
    // es an die dritte Stelle — sonst wäre der eingeschaltete Modus
    // unsichtbar, sobald er nicht zu den ersten dreien gehört, und die
    // Leiste würde lügen.
    addOverflow(g, m_allModes,
                [this](DSPMode m) { pushModeToModel(m); });
}

void CommandBar::buildStepGroup(QHBoxLayout* row)
{
    m_allSteps = {
        {QStringLiteral("10 Hz"),   10},
        {QStringLiteral("100 Hz"),  100},
        {QStringLiteral("1 kHz"),   1000},
        {QStringLiteral("50 Hz"),   50},
        {QStringLiteral("500 Hz"),  500},
        {QStringLiteral("5 kHz"),   5000},
        {QStringLiteral("9 kHz"),   9000},
        {QStringLiteral("10 kHz"),  10000},
    };

    Group& g = addGroup(QStringLiteral("Step"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allSteps.size(); ++i) {
        const auto entry = m_allSteps.at(i);
        QPushButton* b = addPill(g, entry.first);
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushStepToModel(entry.second);
        });
    }

    addOverflow(g, m_allSteps, [this](int hz) { pushStepToModel(hz); });
}

// Ein Helfer statt zweimal derselbe Block: der Ueberlauf ist fuer jede
// Gruppe dasselbe, und zwei Kopien waeren zwei Stellen, an denen der
// naechste Gruppentyp vergessen wird.
template <typename Entry, typename Apply>
void CommandBar::addOverflow(Group& g, const QVector<Entry>& all, Apply apply)
{
    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(QStringLiteral("Alle %1 anzeigen").arg(all.size()));
    g.overflow = more;
    if (g.row) { g.row->addWidget(more); }

    connect(more, &QPushButton::clicked, this, [this, all, apply]() {
        QMenu m(this);
        for (const auto& e : all) {
            QAction* a = m.addAction(e.first);
            connect(a, &QAction::triggered, this,
                    [apply, e]() { apply(e.second); });
        }
        m.exec(QCursor::pos());
    });
}

// ── Modellanbindung ──────────────────────────────────────────────────

void CommandBar::attach(SliceModel* slice)
{
    // Die alten Verbindungen zuerst lösen. Ohne das hängt die Leiste
    // nach einem Pan-Wechsel an beiden Ketten und meldet abwechselnd
    // deren Modus — ein Fehler, der erst beim zweiten Pan auffällt und
    // dann wie ein Wackelkontakt aussieht.
    for (const auto& c : m_links) { disconnect(c); }
    m_links.clear();

    m_slice = slice;
    if (!slice) { return; }

    m_links << connect(slice, &SliceModel::dspModeChanged,
                       this, [this](DSPMode) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::stepHzChanged,
                       this, [this](int) { pullFromModel(); });
    pullFromModel();
}

void CommandBar::pushModeToModel(DSPMode m)
{
    if (m_slice) { m_slice->setDspMode(m); }
    // Nicht selbst umschalten: das Modell meldet die Änderung zurück,
    // und wenn es sie ablehnt, soll die Leiste den alten Zustand
    // zeigen und nicht den gewünschten. Ein Knopf, der immer angeht,
    // egal was darunter passiert, ist eine Lüge mit Rückmeldung.
    pullFromModel();
}

void CommandBar::pushStepToModel(int hz)
{
    if (m_slice) { m_slice->setStepHz(hz); }
    pullFromModel();
}

void CommandBar::pullFromModel()
{
    if (!m_slice) { return; }

    if (Group* g = group(QStringLiteral("Mode"))) {
        const DSPMode cur = m_slice->dspMode();
        QString label;
        for (const auto& e : m_allModes) {
            if (e.second == cur) { label = e.first; break; }
        }
        // Steht der aktuelle Modus nicht vorne, rückt er an die letzte
        // sichtbare Stelle. Sonst zeigt die Leiste drei Pillen, von
        // denen keine an ist, und der eingestellte Modus steht nirgends.
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            if (!visible && !g->pills.isEmpty()) {
                g->pills.last()->setText(label);
            }
            setActive(*g, label);
        }
    }

    if (Group* g = group(QStringLiteral("Step"))) {
        const int cur = m_slice->stepHz();
        QString label;
        for (const auto& e : m_allSteps) {
            if (e.second == cur) { label = e.first; break; }
        }
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            if (!visible && !g->pills.isEmpty()) {
                g->pills.last()->setText(label);
            }
            setActive(*g, label);
        }
    }
}

// ── Zugriff für Tests ────────────────────────────────────────────────

CommandBar::~CommandBar() { qDeleteAll(m_groups); }

CommandBar::Group* CommandBar::group(const QString& name)
{
    for (Group* g : m_groups) {
        if (g->name.compare(name, Qt::CaseInsensitive) == 0) { return g; }
    }
    return nullptr;
}

const CommandBar::Group* CommandBar::group(const QString& name) const
{
    for (const Group* g : m_groups) {
        if (g->name.compare(name, Qt::CaseInsensitive) == 0) { return g; }
    }
    return nullptr;
}

QStringList CommandBar::groups() const
{
    QStringList out;
    for (const Group* g : m_groups) { out << g->name; }
    return out;
}

QStringList CommandBar::pillsIn(const QString& groupName) const
{
    QStringList out;
    if (const Group* g = group(groupName)) {
        for (QPushButton* b : g->pills) { out << b->text(); }
    }
    return out;
}

QString CommandBar::activePill(const QString& groupName) const
{
    if (const Group* g = group(groupName)) {
        for (QPushButton* b : g->pills) {
            if (b->isChecked()) { return b->text(); }
        }
    }
    return {};
}

bool CommandBar::clickPill(const QString& groupName, const QString& label)
{
    if (Group* g = group(groupName)) {
        for (QPushButton* b : g->pills) {
            if (b->text() == label) { b->click(); return true; }
        }
    }
    return false;
}

} // namespace Longpath
