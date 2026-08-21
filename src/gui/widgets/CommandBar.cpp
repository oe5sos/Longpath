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
#include "gui/widgets/DspQuickPopups.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QInputDialog>
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
    buildFilterGroup(row);
    buildStepGroup(row);
    buildNrGroup(row);
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
    m_links << connect(slice, &SliceModel::filterChanged,
                       this, [this](int, int) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::activeNrChanged,
                       this, [this](NrSlot) { pullFromModel(); });
    pullFromModel();
}

// ── Filter ───────────────────────────────────────────────────────────
//
// Drei Breiten vorne, der Rest im „…", und darin auch eine eigene
// Breite zum Eintippen. Auf Ansage vom 2026-08-21, mit Vorlage:
// FILTER 2.7k · 2.9k · 3.3k · …
//
// Die Breiten haengen am MODUS — CW arbeitet mit anderen als SSB.
// Deshalb werden die Pillen nicht fest beschriftet, sondern bei jedem
// Abgleich neu (relabelFilterPills), und ein Klick greift ueber seine
// STELLE in die Liste des aktuellen Modus. Feste Beschriftungen haetten
// in CW die Breiten von SSB angeboten.
void CommandBar::buildFilterGroup(QHBoxLayout* row)
{
    Group& g = addGroup(QStringLiteral("Filter"), row);
    for (int i = 0; i < kVisiblePerGroup; ++i) {
        QPushButton* b = addPill(g, QStringLiteral("—"));
        // ── Die Nutzlast haengt am KNOPF, nicht an seiner Stelle ─────
        //
        // Der erste Entwurf griff ueber den Index in die Preset-Liste
        // des Modus. Das faellt in dem Moment auseinander, in dem eine
        // Pille nachrueckt: laeuft eine Breite, die nicht unter den
        // ersten dreien ist, wird die letzte Pille auf sie
        // umbeschriftet — und der Index zeigte dann weiter auf das
        // dritte Preset. Der Knopf haette „1.8k" gesagt und 3.3k
        // geschaltet.
        //
        // Der bestehende Test exactlyOnePillIsLitPerGroup hat den
        // Entwurf auffliegen lassen, nicht ich.
        b->setProperty("loHz", 0);
        b->setProperty("hiHz", 0);
        connect(b, &QPushButton::clicked, this, [this, b]() {
            if (!m_slice || !b->isEnabled()) { return; }
            pushFilterToModel(b->property("loHz").toInt(),
                              b->property("hiHz").toInt());
        });
    }

    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(QStringLiteral(
        "Alle Breiten dieses Modus — und eine eigene zum Eintippen."));
    g.overflow = more;
    if (g.row) { g.row->addWidget(more); }

    connect(more, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        QMenu m(this);
        const auto all = SliceModel::presetsForMode(m_slice->dspMode());
        for (const auto& pr : all) {
            const int low = pr.first, high = pr.second;
            QAction* a = m.addAction(filterLabel(low, high));
            connect(a, &QAction::triggered, this,
                    [this, low, high]() { pushFilterToModel(low, high); });
        }
        m.addSeparator();
        QAction* own = m.addAction(QStringLiteral("Eigene Breite…"));
        connect(own, &QAction::triggered, this,
                [this]() { askForCustomFilter(); });
        m.exec(QCursor::pos());
    });
}

/// Wie eine Breite in der Leiste heisst: „2.9k" statt „-2900…-100".
/// Der Betrag zaehlt, nicht das Vorzeichen — LSB liegt unter Null und
/// ist trotzdem 2,9 kHz breit.
QString CommandBar::filterLabel(int low, int high)
{
    const int w = qAbs(high - low);
    if (w >= 1000) {
        return QStringLiteral("%1k").arg(w / 1000.0, 0, 'f',
                                         (w % 1000 == 0) ? 0 : 1);
    }
    return QStringLiteral("%1").arg(w);
}

void CommandBar::relabelFilterPills()
{
    Group* g = group(QStringLiteral("Filter"));
    if (!g || !m_slice) { return; }
    const auto all = SliceModel::presetsForMode(m_slice->dspMode());
    for (int i = 0; i < g->pills.size(); ++i) {
        QPushButton* b = g->pills.at(i);
        if (i < all.size()) {
            b->setText(filterLabel(all.at(i).first, all.at(i).second));
            b->setProperty("loHz", all.at(i).first);
            b->setProperty("hiHz", all.at(i).second);
            b->setEnabled(true);
            b->setToolTip(QStringLiteral("%1 Hz bis %2 Hz")
                              .arg(all.at(i).first).arg(all.at(i).second));
        } else {
            // Weniger als drei Breiten in diesem Modus: die Pille
            // bleibt stehen, aber leer und tot. Sie zu verstecken
            // liesse die Leiste bei jedem Moduswechsel springen.
            b->setText(QStringLiteral("—"));
            b->setEnabled(false);
            b->setToolTip(QString{});
        }
    }
}

void CommandBar::pushFilterToModel(int low, int high)
{
    if (m_slice) { m_slice->setFilter(low, high); }
    pullFromModel();
}

// Eine eigene Breite: gefragt wird nach der BREITE, nicht nach zwei
// Flanken. Die Mitte des laufenden Filters bleibt stehen — sonst
// spraenge ein LSB-Filter beim Eintippen auf die andere Seite des
// Traegers, und das will niemand.
void CommandBar::askForCustomFilter()
{
    if (!m_slice) { return; }
    const int low  = m_slice->filterLow();
    const int high = m_slice->filterHigh();
    const int cur  = qAbs(high - low);
    const double centre = (low + high) / 2.0;

    bool ok = false;
    const int w = QInputDialog::getInt(
        this, QStringLiteral("Filterbreite"),
        QStringLiteral("Breite in Hz:"), cur, 50, 20000, 50, &ok);
    if (!ok) { return; }

    pushFilterToModel(qRound(centre - w / 2.0), qRound(centre + w / 2.0));
}

// ── Rauschminderung ──────────────────────────────────────────────────
//
// NR1 bis NR3 vorne, der Rest im „…". Sie schliessen einander aus; ein
// zweiter Klick auf die laufende schaltet sie ab. Das ist derselbe
// Vertrag wie im RX-Feld — dort steht er seit dem 2026-08-18 so, und
// zwei verschiedene Vertraege fuer denselben Schalter waeren die Art
// Unstimmigkeit, die man erst im Betrieb merkt.
void CommandBar::buildNrGroup(QHBoxLayout* row)
{
    m_allNr = {
        {QStringLiteral("NR1"),  NrSlot::NR1},
        {QStringLiteral("NR2"),  NrSlot::NR2},
        {QStringLiteral("NR3"),  NrSlot::NR3},
        {QStringLiteral("NR4"),  NrSlot::NR4},
        {QStringLiteral("DFNR"), NrSlot::DFNR},
        {QStringLiteral("BNR"),  NrSlot::BNR},
        {QStringLiteral("MNR"),  NrSlot::MNR},
    };

    Group& g = addGroup(QStringLiteral("NR"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allNr.size(); ++i) {
        const auto entry = m_allNr.at(i);
        QPushButton* b = addPill(g, entry.first);
        // Nutzlast am Knopf — siehe die Notiz bei den Filterpillen.
        b->setProperty("nrSlot", static_cast<int>(entry.second));
        connect(b, &QPushButton::clicked, this, [this, b]() {
            pushNrToModel(static_cast<NrSlot>(b->property("nrSlot").toInt()));
        });

        // ── Rechtsklick: die Schnellregler DIESER Rauschminderung ────
        //
        // Die Faehigkeit sass bisher auf den NR-Knoepfen im RX-Feld.
        // Weil die auf Ansage vom 2026-08-21 verschwinden ('dies
        // sollte nichts rechts in den widgets stehen'), zieht sie hier
        // mit — sonst waere sie danach unerreichbar, und das ist genau
        // die Art Verlust, die niemandem auffaellt, bis er das erste
        // Mal an einem Regler drehen will.
        //
        // Nicht die Einstellungsseite: auf der Flagge oeffnete der
        // Rechtsklick die drei bis fuenf Regler dieser Minderung, und
        // die Setup-Seite ist darin nur der Verweis ganz unten.
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QWidget::customContextMenuRequested, this,
                [this, b](const QPoint& pos) {
            if (!m_slice) { return; }
            const NrSlot slot =
                static_cast<NrSlot>(b->property("nrSlot").toInt());
            DspQuickPopup::showFor(this, m_slice, slot,
                                   b->mapToGlobal(pos),
                                   [this, slot]() {
                                       emit openNrSetupRequested(slot);
                                   });
        });
    }

    addOverflow(g, m_allNr, [this](NrSlot s) { pushNrToModel(s); });
}

void CommandBar::pushNrToModel(NrSlot slot)
{
    if (!m_slice) { return; }
    // Ein zweiter Klick auf die laufende schaltet ab — der einzige Weg
    // zu „keine", ohne einen achten Knopf „AUS".
    m_slice->setActiveNr(m_slice->activeNr() == slot ? NrSlot::Off : slot);
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

    // ── Filter ───────────────────────────────────────────────────────
    //
    // Erst neu beschriften (der Modus kann sich geaendert haben), dann
    // die passende Pille markieren. Umgekehrt waere die Markierung
    // einen Durchlauf alt.
    relabelFilterPills();
    if (Group* g = group(QStringLiteral("Filter"))) {
        const int lo = m_slice->filterLow();
        const int hi = m_slice->filterHigh();
        const QString label = filterLabel(lo, hi);

        // Dieselbe Regel wie bei Modus und Schrittweite: was laeuft,
        // rueckt an die letzte sichtbare Stelle, wenn es nicht schon
        // vorne steht. Sonst zeigt die Leiste drei Pillen, von denen
        // keine an ist, und die eingestellte Breite steht nirgends.
        bool visible = false;
        for (QPushButton* b : g->pills) {
            if (b->text() == label) { visible = true; break; }
        }
        if (!visible && !g->pills.isEmpty()) {
            QPushButton* last = g->pills.last();
            last->setText(label);
            last->setProperty("loHz", lo);
            last->setProperty("hiHz", hi);
            last->setEnabled(true);
            last->setToolTip(QStringLiteral("%1 Hz bis %2 Hz").arg(lo).arg(hi));
        }
        setActive(*g, label);
    }

    // ── Rauschminderung ──────────────────────────────────────────────
    //
    // Wieder dieselbe Regel. Der erste Entwurf hatte hier eine eigene
    // erfunden — die Marke am „…" statt Nachruecken — und der
    // bestehende Test exactlyOnePillIsLitPerGroup hat sie sofort
    // verworfen. Zu Recht: zwei Regeln fuer dieselbe Leiste sind eine
    // zu viel.
    if (Group* g = group(QStringLiteral("NR"))) {
        const NrSlot cur = m_slice->activeNr();
        QString label;
        for (const auto& e : m_allNr) {
            if (e.second == cur) { label = e.first; break; }
        }
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            if (!visible && !g->pills.isEmpty()) {
                QPushButton* last = g->pills.last();
                last->setText(label);
                last->setProperty("nrSlot", static_cast<int>(cur));
            }
        }
        // Leeres Etikett heisst „keine laeuft" — dann darf auch keine
        // leuchten.
        setActive(*g, label);
    }
}

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
