// =================================================================
// src/gui/widgets/CommandBar.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See CommandBar.h for what this is and why the
// band and filter groups are deliberately absent.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-28 -- the filter "…" reads FilterPresetStore (Thetis names +
//                 the operator's own presets), Martin Fischer, OE5SOS,
//                 AI-assisted via Anthropic Claude.
//   2026-09-28 -- the running-filter pill says the preset name when the
//                 filter is a preset (same rule as the RX applet).
// =================================================================

#include "gui/widgets/CommandBar.h"

#include "gui/ScopedChildWidget.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/DspQuickPopups.h"
#include "gui/widgets/ProfileRail.h"
#include "core/AppSettings.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QPainter>
#include <QStyleOption>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstdlib>

namespace Longpath {

/// Das Aussehen einer Pille. Über Style::-Konstanten, damit die
/// Theme-Datei sie erreicht. Oeffentlich, weil die untere Leiste
/// dieselbe Optik traegt.
namespace {
// Ein Abstand für alle: zwischen Pillen, zwischen Zeilen und zwischen Bereichen.
constexpr int kAbstand = 5;
// Abstand zwischen zwei Bereichen; der Trennstrich liegt in der Mitte.
constexpr int kAbstandZwischenBereichen = 14;
// Pillen je Zeile in der Bandgruppe (zwei Zeilen zu vier).
/// Die Stufe der Leiste. Statisch, weil die Stilblätter (auch die der
/// Profilschiene) sie ohne Leiste erreichen müssen.
bool s_gross = false;
} // namespace

namespace {
/// Die Versalzeile über einer Gruppe (nur in der großen Stufe sichtbar).
QLabel* captionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("groupCaption"));
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

bool CommandBar::grossAktiv() { return s_gross; }
int CommandBar::pillHoehe() { return s_gross ? kPillHeightGross : kPillHeight; }
int CommandBar::pillSchrift() { return s_gross ? kPillFontGross : kPillFontPx; }
int CommandBar::pillBreite() { return kPillBreite; }
int CommandBar::pillPolster() { return s_gross ? kPillPadGross : kPillPadX; }

QString CommandBar::pillStyle()
{
    // Glas & Tiefe (2026-09-17, Stilblatt 3): die Pille ist ERHABEN —
    // Verlauf, Lichtkante oben, dunkle Unterkante — und die aktive
    // traegt den gedeckten Auswahlverlauf statt des flachen kBlueBg.
    // Das nimmt der Kopfleiste die Blau-Summe (fuenf leuchtende Knoepfe
    // in einer Reihe), ohne dass die Auswahl leiser wuerde.
    // Gelb wie das Plus oben rechts (Betreiber 2026-11-21): die aktive
    // Pille traegt die Plus-Farben und dunkle Schrift aus der Familie der
    // Sendetaste. Die Mulde der Auswahl (kGlassSel*) bleibt an ihren
    // anderen Stellen unberuehrt.
    return QStringLiteral(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %1, stop:1 %2);"
        "  border: 1px solid %3; border-radius: %4px;"
        "  border-top-color: %5; border-bottom-color: %6;"
        "  color: %7; font-size: %8px; font-weight: 600;"
        "  padding: 0 %9px; min-height: %10px; max-height: %10px;"
        "}"
        "QPushButton:hover { background: %11; }"
        // Genau einer je Gruppe leuchtet. Der Rest ist fast unsichtbar —
        // das ist der Unterschied zwischen einer Leiste und einer Wand.
        "QPushButton:checked {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %12, stop:1 %13);"
        "  border: 1px solid %14; border-top-color: %15; color: %16;"
        "}")
        .arg(QString::fromLatin1(Style::kGlassBtnTop),        // %1
             QString::fromLatin1(Style::kGlassBtnBot),        // %2
             QString::fromLatin1(Style::kBorder),             // %3
             QString::number(kPillRadius),                    // %4
             Style::shiftL(QColor(Style::kBorder), 12),       // %5
             Style::shiftL(QColor(Style::kBorder), -14),      // %6
             QString::fromLatin1(Style::kTextSecondary),      // %7
             QString::number(pillSchrift()),                    // %8
             QString::number(pillPolster()))                      // %9
        .arg(QString::number(pillHoehe()))                    // %10
        .arg(QString::fromLatin1(Style::kButtonHover),        // %11
             QString::fromLatin1(Style::kPlusTop),            // %12
             QString::fromLatin1(Style::kPlusBottom),         // %13
             QString::fromLatin1(Style::kPlusBorder),         // %14
             Style::shiftL(QColor(Style::kPlusBorder), 16),   // %15
             QString::fromLatin1(Style::kTxKeyText));         // %16
}

CommandBar::CommandBar(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // Die Leiste wächst nicht in die Höhe: sonst verteilt das Layout den
    // Platz auf die Zeilen, und der Abstand zwischen ihnen wird groß.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    // Leichtes Rendering (2026-09-26): dieselbe Platte wie die Panels
    // (Verlauf) und oben die Lichtkante aus HAUSSTIL.md "Weiche
    // Uebergaenge" -- vorher flach kPanelBg.
    setStyleSheet(QStringLiteral(
        "CommandBar { background: %1; border: 1px solid %2;"
        "             border-top-color: %3; border-radius: 6px; }")
        .arg(Style::glassPanelFill(),
             QString::fromLatin1(Style::kBorderSubtle),
             QLatin1String(Style::kGlassLight)));

    // Zwei Zeilen übereinander. Die obere ist die einzige, solange die
    // Leiste breit genug ist; die untere füllt sich nur bei schmaler
    // Breite, nach dem festen Umbruch vor „Filter" (siehe ordneZeilen).
    auto* zeilen = new QVBoxLayout(this);
    zeilen->setContentsMargins(13, 6, 13, 6);
    // Zwischen den Zeilen 5 px (2026-11-26, Betreiber: „da reichen 5 px", bei allen Pillen).
    zeilen->setSpacing(5);
    // Kein automatischer Mindestwert des Layouts: der wäre immer die
    // Einzelzeile und ließe die Leiste nie in zwei Zeilen fallen. Die
    // Mindestgröße setzt ordneZeilen() je nach Aufteilung selbst.
    zeilen->setSizeConstraint(QLayout::SetNoConstraint);

    auto* row = new QHBoxLayout;
    // Der Abstand ZWISCHEN den Gruppen ist das, was sie zu Gruppen
    // macht. Fünf Pixel innerhalb, zweiundzwanzig dazwischen.
    // Zwischen den Bereichen mehr Abstand, damit der Strich frei steht.
    row->setSpacing(kAbstandZwischenBereichen);
    zeilen->addLayout(row);
    m_row = row;

    m_rowZwei = new QHBoxLayout;
    m_rowZwei->setSpacing(kAbstandZwischenBereichen);
    zeilen->addLayout(m_rowZwei);

    // ── BAND zuerst ─────────────────────────────────────────────────
    //
    // Der Betreiber am 2026-08-22: "bandwechsel sollte auch mit
    // buttons möglich sein, am besten in der leiste oben."
    //
    // Ganz links, weil das Band die groebste Wahl ist: erst das Band,
    // dann Betriebsart, Filter, Schrittweite. Dieselbe Reihenfolge wie
    // in der Vorlage und bei Thetis.
    buildBandGroup(row);
    buildModeGroup(row);
    buildDigiGroup(row);     // hängt seine Pillen als zweite Zeile an Mode
    buildAntennaGroup(row);
    buildFilterGroup(row);
    buildStepGroup(row);
    buildNrGroup(row);
    buildRateGroup(row);
    verteileGruppen();
    zieheLeereGruppenZurueck();
}

// ── Die leise Pille (2026-10-07) ─────────────────────────────────────
//
// Gleiche Masse wie pillStyle(), damit beim Verbinden nichts springt:
// derselbe Innenabstand, dieselbe min-/max-Hoehe. Nur Fuellung, Rand
// und Schrift treten zurueck. Kein Verlauf -- "Glas & Tiefe" heisst
// erhaben, und etwas, das gerade nichts kann, soll nicht erhaben sein.
QString CommandBar::pillStyleGedaempft()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  color: %3; font-size: %4px; font-weight: 600;"
        "  padding: 0 %5px; min-height: %6px; max-height: %6px;"
        "  border-radius: %7px;"
        "}"
        "QPushButton:hover { color: %8; border: 1px solid %9; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorderSubtle),
             QString::fromLatin1(Style::kTextScale),
             QString::number(CommandBar::pillSchrift()),
             QString::number(CommandBar::pillPolster()),
             QString::number(CommandBar::pillHoehe()),
             QString::number(CommandBar::kPillRadius),
             QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kBorder));
}

void CommandBar::addGroupWidget(const QString& caption, QWidget* w)
{
    if (!w || !m_row) { return; }
    // Wie addGroup(): Versalzeile oben, Inhalt darunter -- damit das
    // Bauteil auf derselben Linie sitzt wie die Pillen daneben und die
    // Ueberschrift mit BAND/MODE/FILTER/RATE fluchtet.
    auto* box = new QVBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(3);
    QLabel* beschriftung = captionLabel(caption, this);
    // Keine Überschriften in der Leiste (Betreiber 2026-11-26): alle Pillen
    // beginnen auf derselben Linie.
    beschriftung->setVisible(false);
    box->addWidget(beschriftung, 0, Qt::AlignLeft);
    // Oben ausgerichtet: die Profile stehen auf der ersten Linie der Leiste
    // (Betreiber 2026-11-27: "die Profile gehoeren rauf").
    box->addWidget(w, 0, Qt::AlignLeft | Qt::AlignTop);
    m_gruppenWidgets.insert(w, box);
    // Ans Ende der Gruppen, also hinter die Rate -- wie bisher.
    m_boxen.append(box);
    verteileGruppen();
}

// ── Zwei Zeilen bei schmaler Breite (2026-11-21) ─────────────────────
//
// Betreiber am 2026-11-21: die Pillen niedriger, und bei schmaler Breite
// ZWEI Zeilen übereinander, damit Baender und Profile Platz haben. Die
// Umbruchstelle ist FEST: die Gruppe „Filter" beginnt die untere Zeile.
// Oben stehen Band · Mode · Digi; unten Filter · Step · NR · Rate ·
// Profil und das Plus. Die Stelle wechselt nie mit der Breite, nur ob
// umgebrochen wird.
//
// Die Gruppen bleiben, wie sie sind -- es werden nur ihre Kaesten
// umgehaengt. m_boxen ist die Reihenfolge; die Zeilen sind nur die Ablage.
void CommandBar::ordneZeilen(bool zwei)
{
    if (!m_row || !m_rowZwei) { return; }

    // Alles aus beiden Zeilen loesen. Kaesten werden nur losgemacht (ihr
    // Besitzer bleibt die Leiste bzw. removeGroupWidget), Dehnungen und
    // Eintraege des Plus werden abgebaut; die Bauteile selbst bleiben.
    for (QHBoxLayout* zeile : {m_row, m_rowZwei}) {
        while (zeile->count() > 0) {
            QLayoutItem* it = zeile->takeAt(0);
            if (it->layout()) {
                it->layout()->setParent(nullptr);
            } else {
                delete it;
            }
        }
    }

    // Umbruch vor „Filter", aber nur bei zwei Zeilen. Fehlt die Gruppe,
    // bleibt alles oben.
    const int umbruch = zwei ? umbruchStelle() : m_boxen.size();

    m_zweiZeilen = zwei;
    for (int i = 0; i < m_boxen.size(); ++i) {
        QHBoxLayout* zeile = (i < umbruch) ? m_row : m_rowZwei;
        zeile->addLayout(m_boxen.at(i));
    }

    // Überschriften nur in der ersten Zeile. In der zweiten fehlen sie, damit
    // alle Pillen auf demselben 5-px-Raster liegen (Betreiber 2026-11-26:
    // „überschriften ausblenden").
    for (int i = 0; i < m_boxen.size(); ++i) {
        QLayoutItem* kopf = m_boxen.at(i)->itemAt(0);
        if (kopf && kopf->widget()) {
            kopf->widget()->setVisible(false);
        }
    }

    // Das Plus hängt ganz rechts an der letzten Zeile.
    QHBoxLayout* letzte = zwei ? m_rowZwei : m_row;
    for (QWidget* w : m_nachStellen) {
        letzte->addWidget(w, 0, Qt::AlignBottom);
    }

    // Keine Verteilung des Restplatzes auf die Lücken: alle Abstände gleich.
    verteileZeile(m_row, false);
    verteileZeile(m_rowZwei, false);

    // Die Mindestbreite ist IMMER die der zweizeiligen Aufteilung: nur so
    // kann das Fenster unter die Einzelzeile schrumpfen, und resizeEvent
    // schaltet dann um. Die Höhe folgt der tatsächlichen Zeilenzahl.
    const QMargins r = contentsMargins();
    const int u = umbruchStelle();
    const int zweizeiligBreite =
        qMax(breiteEinerZeile(0, u, false),
             breiteEinerZeile(u, m_boxen.size(), true));
    setMinimumSize(zweizeiligBreite + r.left() + r.right(),
                   layout()->totalMinimumSize().height());
    updateGeometry();
    update();
}

int CommandBar::umbruchStelle() const
{
    for (int i = 0; i < m_boxen.size(); ++i) {
        if (gruppenName(m_boxen.at(i)) == QStringLiteral("Filter")) { return i; }
    }
    return m_boxen.size();
}

int CommandBar::breiteEinerZeile(int von, int bis, bool mitNachStellen) const
{
    int summe = 0;
    int anzahl = 0;
    for (int i = qMax(0, von); i < bis && i < m_boxen.size(); ++i) {
        summe += m_boxen.at(i)->totalMinimumSize().width();
        ++anzahl;
    }
    if (mitNachStellen) {
        for (QWidget* w : m_nachStellen) {
            summe += w->minimumSizeHint().width();
            ++anzahl;
        }
    }
    // Die Abstände zwischen den Elementen einer Zeile, wie sie Qt setzt.
    if (anzahl > 1 && m_row) { summe += (anzahl - 1) * m_row->spacing(); }
    return summe;
}

int CommandBar::einzeiligMindestbreite() const
{
    const QMargins r = contentsMargins();
    return breiteEinerZeile(0, m_boxen.size(), true) + r.left() + r.right();
}

// ── Die Gruppen ueber die Breite verteilen ───────────────────────────
//
// Vorher: eine einzige Dehnung ganz hinten. Folge: alle Gruppen klebten
// links zusammen, und rechts standen 168 Pixel tot, in denen allein das
// Plus schwebte.
//
// Jetzt eine Dehnung hinter jeder Gruppe, die nicht das letzte Element
// ihrer Zeile ist. Damit verteilt Qt den uebrigen Platz gleichmaessig --
// und wenn das Fenster schmal wird, schrumpfen alle Dehnungen auf null,
// und es bleibt beim Grundabstand von 22. Eine gerechnete Weite haette
// bei schmalem Fenster ueberlaufen; eine Dehnung kann das nicht.
//
// Die letzte Gruppe bekommt bewusst eine Dehnung, SOLANGE hinter ihr
// noch das Plus haengt -- das schiebt das Plus an den rechten Rand, wo
// es hingehoert. Haengt nichts dahinter, bekommt sie keine, sonst
// ruecke alles nach links und die Verteilung waere dahin.
void CommandBar::verteileZeile(QHBoxLayout* zeile, bool verteilen)
{
    for (int i = zeile->count() - 2; i >= 0; --i) {
        QLayoutItem* it = zeile->itemAt(i);
        if (!it || !it->layout()) { continue; }
        // Vor einem angehängten Bauteil (dem Plus) steht die Dehnung auch
        // in der zweizeiligen Anordnung: sie schiebt das Plus nach rechts.
        QLayoutItem* danach = zeile->itemAt(i + 1);
        const bool vorBauteil = danach && danach->widget();
        if (verteilen || vorBauteil) { zeile->insertStretch(i + 1, 1); }
    }
}

void CommandBar::verteileGruppen()
{
    ordneZeilen(m_zweiZeilen);
}

QString CommandBar::gruppenName(const QLayout* box) const
{
    for (const Group* g : m_groups) {
        if (g && g->box == box) { return g->name; }
    }
    return QString();
}

QVector<QStringList> CommandBar::zeilenGruppen() const
{
    QVector<QStringList> ergebnis(2);
    const QHBoxLayout* zeilen[2] = {m_row, m_rowZwei};
    for (int z = 0; z < 2; ++z) {
        if (!zeilen[z]) { continue; }
        for (int i = 0; i < zeilen[z]->count(); ++i) {
            QLayoutItem* it = zeilen[z]->itemAt(i);
            if (it && it->layout()) {
                ergebnis[z].append(gruppenName(it->layout()));
            }
        }
    }
    return ergebnis;
}

void CommandBar::removeGroupWidget(QWidget* w)
{
    // Das Gegenstueck zu addGroupWidget. Ohne das blieb die Versalzeile
    // ("PROFIL") samt leerem Teil-Layout in der Leiste stehen, wenn das
    // Bauteil wegging -- und beim naechsten Hinzufuegen kam eine zweite
    // dazu. Nach drei Wechseln standen drei Ueberschriften nebeneinander
    // (2026-10-07 offscreen nachgemessen).
    if (!w || !m_row) { return; }
    const auto it = m_gruppenWidgets.constFind(w);
    if (it == m_gruppenWidgets.constEnd()) { return; }
    QLayout* box = it.value();
    m_gruppenWidgets.erase(it);
    if (!box) { return; }
    // Das Bauteil selbst gehoert dem Aufrufer -- nur aus der Anordnung
    // nehmen, nicht loeschen. Alles andere darin (die Versalzeile) ist
    // unser und geht mit.
    box->removeWidget(w);
    while (QLayoutItem* item = box->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    // Erst aus der Reihenfolge, dann die Zeilen neu ordnen -- dabei loest
    // sich der Kasten aus seiner Zeile, und erst danach darf er weg.
    m_boxen.removeAll(box);
    verteileGruppen();
    delete box;
}

// ── Die Trennstriche ─────────────────────────────────────────────────
//
// Jedes andere Bauteil im Fenster ist eingefasst: BANDWIDTH FILTER,
// PANADAPTER, S-METER, TX, FREQUENZ, ROTOR/LOG -- alle mit Rahmen und
// Versalzeile. Die Kommandoleiste war die einzige Stelle, an der
// Gruppen ueberhaupt keine Grenze hatten; vierundzwanzig fast gleiche
// Pillen in einer Reihe, und das Auge hat keinen Halt.
//
// Der Haarstrich ist die leiseste Form derselben Aussage. kGlassEdgeLight
// ist dafuer die Hausfarbe (Lichtkante auf kBorder) -- kein neuer Ton.
//
// Gezeichnet, nicht gebaut: ein Strich-Widget waere ein weiteres Element
// in der Reihe, an dem verteileZeile() und addGroupWidget() vorbeizielen
// koennten. Zwischen Gruppen zweier Zeilen wird KEIN Strich gezogen.
void CommandBar::paintEvent(QPaintEvent* e)
{
    // Erst das Stilblatt zeichnen lassen -- die Leiste traegt
    // WA_StyledBackground, und ohne diesen Aufruf bliebe ihr Grund leer.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(e);

    // Die Bereiche in SICHTBARER Reihenfolge (Zeile fuer Zeile), nicht in der
    // Reihenfolge ihres Anlegens: nur so liegt der Strich zwischen Nachbarn.
    QVector<QRect> reihe;
    for (const QHBoxLayout* zeile : {m_row, m_rowZwei}) {
        if (!zeile) { continue; }
        for (int i = 0; i < zeile->count(); ++i) {
            QLayoutItem* it = zeile->itemAt(i);
            // Ausgeblendete (leere) Bereiche bleiben draußen, damit ihre Nachbarn
            // einander finden.
            if (it && it->layout() && it->layout()->geometry().width() > 0) {
                reihe.append(it->layout()->geometry());
            }
        }
    }
    if (reihe.size() < 2) { return; }

    // Trennstriche gelb (Betreiber 2026-11-26).
    p.setPen(QPen(QColor(QString::fromLatin1(Style::kEqBand2)), 1));
    for (int i = 0; i + 1 < reihe.size(); ++i) {
        const QRect a = reihe.at(i);
        const QRect b = reihe.at(i + 1);
        // Genau in die Mitte der Luecke, zwischen den Pillenkanten.
        const int x = (a.right() + b.left()) / 2;
        const int oben  = qMin(a.top(), b.top());
        const int unten = qMax(a.bottom(), b.bottom());
        p.drawLine(x, oben, x, unten);
    }
}

void CommandBar::setzeGrossModus(bool gross)
{
    if (s_gross == gross) { return; }
    // Die alten Stilblätter merken, damit nur die Pillen der Leiste (und
    // nichts Fremdes mit eigener Optik) umgestellt werden.
    const QString altPille = pillStyle();
    const QString altLeise = pillStyleGedaempft();
    s_gross = gross;
    const QString neuPille = pillStyle();
    const QString neuLeise = pillStyleGedaempft();
    for (QPushButton* b : findChildren<QPushButton*>()) {
        if (b->styleSheet() == altPille) { b->setStyleSheet(neuPille); }
        else if (b->styleSheet() == altLeise) { b->setStyleSheet(neuLeise); }
    }
    for (QLabel* l : findChildren<QLabel*>(QStringLiteral("groupCaption"))) {
        l->setVisible(false);
    }
    // Angehängtes Plus und Bauteile in den Gruppen folgen der Pillenhöhe.
    const int aussen = pillHoehe() + 2;
    for (QWidget* w : m_nachStellen) { w->setFixedSize(aussen, aussen); }
    // Die Profilschiene hat zwei Zeilen und behält ihre eigene Höhe.
    for (QWidget* w : m_gruppenWidgets.keys()) {
        if (qobject_cast<ProfileRail*>(w)) { continue; }
        w->setFixedHeight(aussen);
    }
    applyRateStyle(m_rateWarn);
    // Das Plus der Profilschiene trägt seine Höhe im eigenen Stilblatt.
    for (ProfileRail* r : findChildren<ProfileRail*>()) { r->stufeGewechselt(); }
    verteileGruppen();
}

void CommandBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    // Ab kGrossAbBreite wachsen die Pillen (große Stufe, mit Beschriftung).
    setzeGrossModus(width() >= kGrossAbBreite);
    // Unterhalb der Schwelle zwei Zeilen, ab ihr eine. Die Schwelle ist die
    // gemessene Konstante. Nur bei einem Wechsel umordnen. (2026-11-26:
    // nicht mehr die dynamische Mindestbreite -- sonst bricht die Leiste bei
    // der Breite des Betreibers in vier Zeilen um.)
    const int schwelle = kEinzeiligUnterBreite;
    const bool zwei = width() < schwelle;
    if (zwei != m_zweiZeilen) { ordneZeilen(zwei); }
}

// ── Keine alten Striche (2026-11-21) ─────────────────────────────────
//
// Betreiber: „wieder sind die trennlinien oben falsch - schaue oben bei
// ft8 und bei 192". Auf seinem Bild standen neben den richtigen Strichen
// lange an alten Stellen, halb hinter den Pillen. Die Striche malt die
// Leiste selbst (paintEvent); ruecken die Gruppen aber, ohne dass die
// Leiste ihre Groesse aendert -- andere Pillen-Texte nach dem Wechsel
// Flex -> Anvelina, eine Pille mehr oder weniger --, zeichnet Qt nur die
// verschobenen Pillen neu. Die alten Striche bleiben dann als Bildreste
// stehen. tst_command_bar hat 508 solche Punkte gezaehlt.
//
// Darum: jede neue Anordnung (LayoutRequest, nachdem Qt sie ausgefuehrt
// hat) zeichnet die ganze Leiste neu. Das kommt selten und kostet eine
// Leiste aus Pillen.
bool CommandBar::event(QEvent* e)
{
    const bool erledigt = QWidget::event(e);
    if (e->type() == QEvent::LayoutRequest) {
        update();
    }
    return erledigt;
}

void CommandBar::addTrailing(QWidget* w)
{
    if (!w || !m_row) { return; }
    // Rechtsbündig: das Plus kommt ans Ende der letzten Zeile. Die
    // Ausrichtung unten hält es auf einer Linie mit den Pillen und nicht
    // mit den Versalzeilen darüber. Ordnen übernimmt ordneZeilen().
    m_nachStellen.append(w);
    verteileGruppen();
}

// ── Gruppen bauen ────────────────────────────────────────────────────

CommandBar::Group& CommandBar::addGroup(const QString& caption,
                                        QHBoxLayout* row)
{
    auto* box = new QVBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(kAbstand);
    QLabel* versal = captionLabel(caption, this);
    versal->setVisible(false);
    box->addWidget(versal, 0, Qt::AlignLeft);

    auto* pills = new QHBoxLayout;
    pills->setContentsMargins(0, 0, 0, 0);
    pills->setSpacing(5);
    // Oben ausgerichtet: eine Gruppe mit einer Pillenreihe beginnt auf der
    // ersten Linie der Leiste, nicht mittig (Betreiber 2026-11-26).
    pills->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    box->addLayout(pills);
    // Die zweite Zeile entsteht erst, wenn eine Pille sie braucht (addPill).
    // Eine leere Zeile hier hat trotzdem einen Abstand gekostet: die Lücke
    // zwischen den Reihen (Betreiber 2026-11-27: "diese gehört weg").
    QHBoxLayout* zweite = nullptr;
    // Die Zeile setzt ordneZeilen() endgültig; hier nur der Besitz.
    row->addLayout(box);
    m_boxen.append(box);

    auto* g = new Group{caption, pills, {}, nullptr, versal, box};
    g->row2 = zweite;
    m_groups.append(g);
    return *g;
}

namespace {

// ── Die Pille darf nie schmaler sein als ihre Schrift ────────────────
//
// „advance + 20" stand hier seit dem 2026-08-23 und war damals
// richtig. Seit „Glas & Tiefe" (2026-09-17) tragen die Pillen aber
// Rand und Polsterung aus dem Stylesheet, und die zwanzig Punkte
// decken das nicht mehr. Gemessen am laufenden Programm (2026-10-15,
// Fenster 796 px): JEDE Pille stand genau auf ihrer gesetzten
// Mindestbreite, und die lag durchweg VIER Punkte unter dem, was Qt
// selbst als minimumSizeHint ausrechnet. Darum las sich „40m" als
// „40", „LSB" als „LSI" und „NR1" als „NR" — derselbe Fehler wie
// damals, nur vier Punkte statt vieler.
//
// Also nicht mehr selbst rechnen, sondern das Groessere von beidem
// nehmen: die eigene Schaetzung UND Qts Bedarf, der Rand und
// Polsterung des Stylesheets kennt. ensurePolished() davor, sonst
// antwortet der Hinweis noch ohne Stylesheet.
void setzeMindestbreite(QPushButton* b, const QString& label)
{
    // Jede Pille gleich breit, egal wie lang ihre Beschriftung ist
    // (Betreiber 2026-11-26: „jede pille gleiche groesse, egal wie breit der text ist").
    // Die Breite steht fest; ein zu langer Text wird abgeschnitten, nicht die Pille breiter.
    Q_UNUSED(label);
    if (!b) { return; }
    b->setFixedWidth(CommandBar::pillBreite());
}

} // namespace

QPushButton* CommandBar::addPill(Group& g, const QString& label)
{
    auto* b = new QPushButton(label, this);
    b->setCheckable(true);
    b->setStyleSheet(pillStyle());
    b->setCursor(Qt::PointingHandCursor);

    // ── Keine Pille schmaler als ihre Schrift ───────────────────────
    //
// Am 2026-08-23 live gesehen, nachdem die BAND-Gruppe dazukam:
    // aus "10 Hz" wurde "I0 Hz", aus "100 Hz" wurde "00 H". Die
    // Anordnung hatte keinen Platz mehr und hat die Pillen unter ihre
    // eigene Schrift gequetscht — eine halb gelesene Schrittweite ist
    // schlimmer als keine.
    //
    // Mindestbreite aus der Schriftmetrik plus Innenabstand: darunter
    // darf die Anordnung nicht. Wird es zu eng, muss sich etwas
    // anderes ruehren (die Leiste rollt), nicht die Beschriftung.
    setzeMindestbreite(b, label);
    // Je Gruppe `spalten` Pillen in der ersten Zeile, der Rest in der zweiten
    // (Betreiber 2026-11-26). Die zweite Zeile entsteht bei Bedarf.
    QHBoxLayout* ziel = g.row;
    if (static_cast<int>(g.pills.size()) >= g.spalten) {
        if (!g.row2 && g.box) {
            auto* zweite = new QHBoxLayout;
            zweite->setContentsMargins(0, 0, 0, 0);
            zweite->setSpacing(kAbstand);
            // Linksbündig, wie in addGroup(): FT8 steht unter LSB.
            zweite->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            static_cast<QBoxLayout*>(g.box)->addLayout(zweite);
            g.row2 = zweite;
        }
        if (g.row2) { ziel = g.row2; }
    }
    g.pills.append(b);
    if (ziel) { ziel->addWidget(b); }
    return b;
}

// ── Ohne Funkgeraet tritt eine leere Gruppe zurueck ──────────────────
//
// Die Regel steht schon im Fenster: POWER, SWR und SIGNAL AVERAGE
// zeigen ihre "—" gedaempft. FILTER und RATE taten es nicht -- drei
// helle Pillen mit einem Strich darin sehen nicht nach "noch nichts"
// aus, sondern nach kaputt.
//
// Die Masse bleiben gleich (siehe pillStyleGedaempft), es springt also
// nichts, wenn das Geraet kommt.
bool CommandBar::gruppeIstLeer(const Group& g) const
{
    // RATE traegt seinen Wert in einem eigenen Schild, nicht in Pillen
    // -- ohne diese Abzweigung waere die Gruppe immer "leer", auch mit
    // "48 kHz" darin.
    if (g.pills.isEmpty()) {
        return !m_rateLabel || m_rateLabel->text() == QStringLiteral("—");
    }
    for (QPushButton* b : g.pills) {
        if (!b) { continue; }
        const QString t = b->text();
        // Das "…" ist kein Inhalt, es ist der Weg zu mehr davon.
        if (t == QStringLiteral("…") || t == QStringLiteral("—")) { continue; }
        return false;
    }
    return true;
}

void CommandBar::zieheLeereGruppenZurueck()
{
    for (Group* g : m_groups) {
        if (!g) { continue; }
        const bool leer = gruppeIstLeer(*g);
        for (QPushButton* b : g->pills) {
            if (b) { b->setStyleSheet(leer ? pillStyleGedaempft() : pillStyle()); }
        }
        // Leere Gruppe ganz weg, nicht nur gedämpft (Betreiber 2026-11-26):
        // ausgeblendet nimmt sie keinen Platz und keinen Abstand mehr ein.
        for (QPushButton* b : g->pills) {
            if (b) { b->setVisible(!leer); }
        }
        if (g->overflow) { g->overflow->setVisible(!leer); }
        if (g->pills.isEmpty() && m_rateLabel) { m_rateLabel->setVisible(!leer); }
        if (g->caption) { g->caption->setVisible(false); }
        if (g->caption) {
            // Zurueck auf kTextScale, nicht auf einen leeren Stil:
            // captionLabel() setzt die Farbe selbst, und ein leeres
            // Stilblatt haette die Versalzeile auf die Vorgabefarbe
            // gestellt -- heller als vorher statt gleich.
            g->caption->setStyleSheet(
                QStringLiteral("QLabel { color: %1; }")
                    .arg(QString::fromLatin1(leer ? Style::kBorderMuted
                                                  : Style::kTextScale)));
        }
    }
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
    g.spalten = 4;   // LSB · USB · CWL · „…“ in einer Zeile (Betreiber 2026-11-26)
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

// ── Digital: eine eigene Gruppe, nicht zwei Pillen mehr bei Mode ─────
//
// Betreiber am 2026-10-21, vor der Antenne stehend: „ich will, sobald
// ich auf ft8 klicke, dass alles automatisch funktioniert", „ich will
// nicht die freq suchen", „quasi wie jtdx".
//
// Zuerst standen FT8 und FT4 als zwei weitere Pillen bei Mode. Das
// fiel in der CI sofort auf, und zu Recht: `tst_command_bar` haelt
// fest, dass JEDE Gruppe genau kVisiblePerGroup Pillen zeigt -- „drei
// vorne, der Rest im Menue" ist der ganze Grund, warum diese Leiste
// schmaler ist als die Filterwand im RX-Panel.
//
// Eine eigene Gruppe ist ohnehin das Richtigere: es sind keine
// Betriebsarten des Geraets. Das Geraet steht dabei auf DIGU; was FT8
// von FT4 unterscheidet, ist die Anrufstelle und der Dekoder, nicht
// die Demodulation. Zwei Pillen, kein „…" -- mehr gibt es nicht.
// Nur diese eine Gruppe nachziehen.
//
// Eine eigene Funktion und nicht `pullFromModel()`, weil sie an
// `frequencyChanged` haengt: beim Drehen am Knopf kommt das Signal
// dutzendfach je Sekunde, und die ganze Leiste neu zu stellen waere
// dafuer zu teuer. Hier sind es zwei Knoepfe.
void CommandBar::aktualisiereDigiLampe()
{
    Group* g = group(QStringLiteral("Digi"));
    if (!g) { return; }
    if (!m_slice) { setActive(*g, QString()); return; }

    // ── Die Lampe zeigt den DEKODER, nicht die Frequenz ──────────
    //
    // Bis zum 2026-10-30 las sie `stehtSchonDort(frequenz)`. Der
    // Betreiber stand an dem Abend auf 7.1275 MHz -- keine
    // Anrufstelle, also beide Pillen grau -- waehrend der Dekoder
    // durchgehend FT4 fuhr. Er sagte dazu: „war aber auf ft8", und
    // konnte es nicht besser wissen: die einzige Stelle, an der FT4
    // stand, war der Knopf IM FT8-Fenster.
    //
    // Zwei Anzeigen, die dasselbe meinen und Verschiedenes sagen.
    // Jetzt sagt die Lampe, was der Dekoder tut -- danach richtet
    // sich, was ankommt. Wo die Scheibe steht, sagt die Skala.
    const DSPMode m = m_slice->dspMode();
    const bool datenmodus = (m == DSPMode::DIGU || m == DSPMode::DIGL);
    QString label;
    if (datenmodus) {
        using DigiFrequenzen::Digimodus;
        if (m_digiDekoderLaeuft) {
            // Der Dekoder hat Vorrang: was er faehrt, bestimmt, was
            // ankommt. Die Frequenz sagt nur, WO man steht, und das
            // sagt die Skala schon.
            label = QLatin1String(DigiFrequenzen::name(m_digiBetriebsart));
        } else {
            // Ohne Dekoder bleibt die alte Auskunft: stehe ich auf
            // einer Anrufstelle? Sie darf NICHT schon bei DIGU
            // angehen -- in DIGU sitzt man auch auf PSK oder Olivia,
            // und ein Knopf, der dann leuchtet, luegt.
            const auto f = qint64(m_slice->frequency());
            if (DigiFrequenzen::stehtSchonDort(f, Digimodus::Ft8)) {
                label = QStringLiteral("FT8");
            } else if (DigiFrequenzen::stehtSchonDort(f, Digimodus::Ft4)) {
                label = QStringLiteral("FT4");
            } else if (DigiFrequenzen::stehtSchonDort(f, Digimodus::Ft2)) {
                label = QStringLiteral("FT2");
            }
        }
    }
    setActive(*g, label);
}

void CommandBar::setzeDigiDekoder(bool laeuft,
                                  DigiFrequenzen::Digimodus modus)
{
    if (m_digiDekoderLaeuft == laeuft && m_digiBetriebsart == modus) {
        return;
    }
    m_digiDekoderLaeuft = laeuft;
    m_digiBetriebsart   = modus;
    aktualisiereDigiLampe();
}

void CommandBar::buildDigiGroup(QHBoxLayout* row)
{
    // Betreiber am 2026-11-24: FT8, FT4 und FT2 stehen UNTER LSB/USB/CWL,
    // nicht als eigene Gruppe daneben. Die Gruppe „Digi" bleibt als
    // Buchführung (Lampe, Prüfstand) bestehen; ihr Kasten und ihre
    // Versalzeile entfallen, nur die Pillenzeile wandert in den
    // Mode-Kasten.
    Group* mode = group(QStringLiteral("Mode"));
    Group& g = addGroup(QStringLiteral("Digi"), row);
    for (const auto& eintrag : {
             std::pair{QStringLiteral("FT8"), DigiFrequenzen::Digimodus::Ft8},
             std::pair{QStringLiteral("FT4"), DigiFrequenzen::Digimodus::Ft4},
             std::pair{QStringLiteral("FT2"), DigiFrequenzen::Digimodus::Ft2}}) {
        QPushButton* b = addPill(g, eintrag.first);
        connect(b, &QPushButton::clicked, this,
                [this, modus = eintrag.second]() { emit digiRequested(modus); });
    }

    if (!mode || !mode->box) { return; }
    auto* modeBox = qobject_cast<QVBoxLayout*>(mode->box);
    if (!modeBox) { return; }
    // Zweite Zeile: die Pillen unter die Modusreihe, linksbündig.
    QLayout* eigenerKasten = g.box;
    eigenerKasten->removeItem(g.row);
    row->removeItem(eigenerKasten);
    m_boxen.removeAll(eigenerKasten);
    delete g.caption;
    g.caption = nullptr;
    g.box = nullptr;
    delete eigenerKasten;
    g.row->setParent(nullptr);
    // Die Digi-Pillen kommen in die ZWEITE Zeile der Betriebsart, nicht in
    // eine dritte (Betreiber 2026-11-26: zwei Zeilen; die Reihenfolge ist egal).
    QHBoxLayout* zeile = mode->row2;
    if (!zeile) { return; }
    while (QLayoutItem* it = g.row->takeAt(0)) { delete it; }
    for (QPushButton* b : g.pills) { zeile->addWidget(b); }
    delete g.row;
    g.row = zeile;
}

void CommandBar::setAntennenVerfuegbar(const QStringList& antennen)
{
    Group* g = group(QStringLiteral("Ant"));
    if (!g) { return; }
    for (QPushButton* b : g->pills) {
        if (!b) { continue; }
        const QString name = b->text();
        if (name != QStringLiteral("ANT3") && name != QStringLiteral("ANT4")) { continue; }
        const bool da = antennen.isEmpty() || antennen.contains(name);
        b->setEnabled(da);
        b->setToolTip(da ? QString() : tr("Dieses Gerät führt %1 nicht").arg(name));
    }
}

void CommandBar::buildAntennaGroup(QHBoxLayout* row)
{
    // Betreiber am 2026-11-24: ANT1 · ANT2 · ANT3 · ANT4 an der Stelle,
    // die FT8/FT4/FT2 freigemacht haben. Ein Druck schaltet RX und TX der
    // aktiven Scheibe auf diese Antenne (wie das Aufklappfeld im RX-Panel).
    Group& g = addGroup(QStringLiteral("Ant"), row);
    for (int i = 1; i <= 4; ++i) {
        const QString name = QStringLiteral("ANT%1").arg(i);
        QPushButton* b = addPill(g, name);
        connect(b, &QPushButton::clicked, this, [this, name]() {
            if (!m_slice) { return; }
            m_slice->setRxAntenna(name);
            m_slice->setTxAntenna(name);
        });
    }
}

void CommandBar::buildBandGroup(QHBoxLayout* row)
{
    // Vorne 40, 20, 15 — auf Ansage des Betreibers (2026-08-22):
    // "bänder sollten 40,20,15 ersichtlich sein, rest dann mit einem
    // fenster mit punkten ... 160, 80, 60, 30, 10, 6".
    //
    // Die Reihenfolge dahinter ist seine: erst die sechs, die er
    // genannt hat, dann der Rest. Was vorne steht, ist ohne Umweg
    // erreichbar; alles Weitere liegt hinter dem "…".
    m_allBands = {
        // Vorne 160 · 80 · 60 · 40 (Betreiber 2026-11-21), dann der Rest.
        {QStringLiteral("160m"), Band::Band160m},
        {QStringLiteral("80m"),  Band::Band80m},
        {QStringLiteral("60m"),  Band::Band60m},
        {QStringLiteral("40m"),  Band::Band40m},
        {QStringLiteral("20m"),  Band::Band20m},
        {QStringLiteral("15m"),  Band::Band15m},
        {QStringLiteral("30m"),  Band::Band30m},
        {QStringLiteral("10m"),  Band::Band10m},
        {QStringLiteral("6m"),   Band::Band6m},
        {QStringLiteral("17m"),  Band::Band17m},
        {QStringLiteral("12m"),  Band::Band12m},
        {QStringLiteral("GEN"),  Band::GEN},
        {QStringLiteral("WWV"),  Band::WWV},
    };

    Group& g = addGroup(QStringLiteral("Band"), row);
    g.spalten = 3;   // oben drei Bänder, darunter der Rest (Betreiber 2026-11-26)
    // Zweite Pillenzeile unter den ersten vier Baendern (2026-11-21).
    QHBoxLayout* zweite = g.row2;
    g.row2 = zweite;
    // Vier Bänder vorne (160 · 80 · 60 · 40, Betreiber 2026-11-21), die
    // übrigen im „…".
    for (int i = 0; i < kVisibleBand && i < m_allBands.size(); ++i) {
        const auto entry = m_allBands.at(i);
        QPushButton* b = addPill(g, entry.first);
        b->setProperty("bandValue", static_cast<int>(entry.second));
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushBandToModel(entry.second);
        });
    }
    addOverflow(g, m_allBands,
                [this](Band b) { pushBandToModel(b); });
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
    // Zweizeilig (Betreiber 2026-11-24: die Schrittweite braucht man
    // selten): 10 Hz · 100 Hz oben, 1 kHz · „…" darunter -- halb so breit.
    QHBoxLayout* zweite = g.row2;
    for (int i = 0; i < kVisiblePerGroup && i < m_allSteps.size(); ++i) {
        const auto entry = m_allSteps.at(i);
        QPushButton* b = addPill(g, entry.first);
        if (i >= 2) {
            g.row->removeWidget(b);
            zweite->addWidget(b);
        }
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushStepToModel(entry.second);
        });
    }
    g.row2 = zweite;   // das „…" kommt in die zweite Zeile

    addOverflow(g, m_allSteps, [this](int hz) { pushStepToModel(hz); });
}

// Ein Helfer statt zweimal derselbe Block: der Ueberlauf ist fuer jede
// Gruppe dasselbe, und zwei Kopien waeren zwei Stellen, an denen der
// naechste Gruppentyp vergessen wird.
template <typename Entry, typename Apply>
void CommandBar::addOverflow(Group& g, const QVector<Entry>& all, Apply apply)
{
    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setFixedWidth(CommandBar::pillBreite());   // gleiche Breite wie jede Pille (2026-11-26)
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(tr("Show all %1").arg(all.size()));
    g.overflow = more;
    // „…“ in die erste Zeile, solange dort Platz ist; sonst in die zweite.
    if (g.row) { ((static_cast<int>(g.pills.size()) < g.spalten || !g.row2) ? g.row : g.row2)->addWidget(more); }

    connect(more, &QPushButton::clicked, this, [this, all, apply]() {
        // Am Fenster, nicht an der Leiste: die Leiste (oder ihr Elternteil)
        // hat ein eigenes natives Fenster, und ein Menue daran bekam von Qt
        // keinen Fensterbezug -- „QWidgetClassWindow must be a top level
        // window." im Protokoll vom 2026-09-29, jedesmal kurz vor einem
        // Bandwechsel. Wie ProfileRail (2552eadc).
        ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
        QMenu& m = *menuOwner.get();
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
    m_links << connect(slice, &SliceModel::rxAntennaChanged,
                       this, [this](const QString&) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::bandChanged,
                       this, [this](Longpath::Band) { pullFromModel(); });
    // Die Digi-Lampe haengt an der genauen Frequenz, nicht nur am
    // Band: innerhalb von 20 m entscheidet sie zwischen FT8 (14.074),
    // FT4 (14.080) und „keins von beiden". Darum hier das feine
    // Signal -- aber nur fuer diese Gruppe, siehe
    // aktualisiereDigiLampe().
    m_links << connect(slice, &SliceModel::frequencyChanged,
                       this, [this](double) { aktualisiereDigiLampe(); });
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
    more->setFixedWidth(CommandBar::pillBreite());   // gleiche Breite wie jede Pille (2026-11-26)
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(tr("All widths for this mode -- and one of "
                                    "your own to type in."));
    g.overflow = more;
    // „…“ in die erste Zeile, solange dort Platz ist; sonst in die zweite.
    if (g.row) { ((static_cast<int>(g.pills.size()) < g.spalten || !g.row2) ? g.row : g.row2)->addWidget(more); }

    connect(more, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        ScopedChildWidget<QMenu> menuOwner(window());   // am Fenster, siehe addOverflow()
        QMenu& m = *menuOwner.get();
        // Beschriftet mit dem NAMEN der Vorgabe, wie Thetis' Knoepfe —
        // nicht mit der aus den Kanten gerechneten Breite: CW F10 ist
        // ±13 Hz, also 26 Hz breit, und heisst in Thetis „25".
        for (const FilterMenuEntry& e : filterMenuEntries()) {
            const int low = e.low, high = e.high;
            QAction* a = m.addAction(e.name);
            connect(a, &QAction::triggered, this,
                    [this, low, high]() { pushFilterToModel(low, high); });
        }
        m.addSeparator();
        QAction* own = m.addAction(tr("Own width…"));
        connect(own, &QAction::triggered, this,
                [this]() { askForCustomFilter(); });
        m.exec(QCursor::pos());
    });
}

void CommandBar::setMaschinenNr(const std::function<QString(NrSlot)>& namen)
{
    m_maschinenNr = namen;
}

void CommandBar::setFilterPresetStore(FilterPresetStore* store)
{
    m_presetStore = store;
}

QVector<CommandBar::FilterMenuEntry> CommandBar::filterMenuEntries() const
{
    QVector<FilterMenuEntry> out;
    if (!m_slice) { return out; }
    const DSPMode mode = m_slice->dspMode();
    if (m_presetStore) {
        for (const FilterPreset& p : m_presetStore->presetsForMode(mode)) {
            out.append({p.name, p.low, p.high});
        }
        return out;
    }
    const auto edges = SliceModel::presetsForMode(mode);
    const QStringList names = SliceModel::presetNamesForMode(mode);
    for (int i = 0; i < edges.size(); ++i) {
        out.append({names.value(i, filterLabel(edges[i].first, edges[i].second)),
                    edges[i].first, edges[i].second});
    }
    return out;
}

/// Was die Pille des LAUFENDEN Filters sagt: der Name der Vorgabe, wenn er
/// eine ist, sonst die Breite. Dieselbe Regel wie im RX-Applet
/// (RxApplet::updateFilterLabel, dort mit dem Thetis-Bezug) — sonst
/// stuende oben „26" und rechts „25" fuer denselben Filter.
QString CommandBar::runningFilterLabel(int low, int high) const
{
    if (!m_slice) { return filterLabel(low, high); }
    const DSPMode mode = m_slice->dspMode();
    const QString name = m_presetStore
        ? m_presetStore->nameForEdges(mode, low, high)
        : FilterPresetStore::defaultNameForEdges(mode, low, high);
    return name.isEmpty() ? filterLabel(low, high) : name;
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

// ── Welche drei vorne stehen ─────────────────────────────────────────
//
// Der Betreiber, 2026-08-21, zuerst „2,7; 2,9; 3,5", Minuten spaeter
// „2,9; 3,2; 3,8 bitte".
//
// Genau deshalb steht die Auswahl in den EINSTELLUNGEN und nicht im
// Quelltext: die erste Bitte enthielt zwei Breiten, die unsere
// Preset-Liste damals nicht kannte (sie fuehrte 5,0 / 4,4 / 3,8 / 3,2 /
// 2,9 / 2,6 / 2,3 / 1,7 / 1,1 / 0,5), die zweite nur bekannte. Seit
// 2026-09-28 fuehrt sie die Thetis-Liste 5,0 / 4,4 / 3,8 / 3,3 / 2,9 /
// 2,7 / 2,4 / 2,1 / 1,8 / 1,0 — die alte war erfunden (siehe
// SliceModel::presetsForMode); jetzt fehlt 3,5 aus der ersten Bitte
// und 3,2 aus der zweiten. Waere die Reihe fest verdrahtet, waere jede
// Meinungsaenderung eine Uebersetzung; so ist sie eine Zahl in einer
// Datei.
//
// Die drei vorderen sind darum keine Presets, sondern BREITEN. Das ist
// auch naeher an der Sache: wer „3,2" sagt, meint eine Breite und
// nicht den soundsovielten Eintrag einer Liste. Das „…" fuehrt
// weiterhin die echten Presets des Modus.
QVector<int> CommandBar::frontWidthsHz()
{
    const QString raw = AppSettings::instance()
        .value(QStringLiteral("CommandBarFilterFront"),
               QStringLiteral("2900,3200,3800")).toString();
    QVector<int> out;
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const int v = p.trimmed().toInt(&ok);
        if (ok && v >= 50 && v <= 20000) { out.append(v); }
    }
    if (out.isEmpty()) { out = {2900, 3200, 3800}; }
    return out;
}

/// Eine Breite auf die Flanken des laufenden Filters umrechnen: die
/// Mitte bleibt stehen. Sonst spraenge ein LSB-Filter auf die andere
/// Seite des Traegers, sobald man eine Breite waehlt.
QPair<int, int> CommandBar::edgesForWidth(int widthHz) const
{
    if (!m_slice) { return {-widthHz, 0}; }
    const double centre =
        (m_slice->filterLow() + m_slice->filterHigh()) / 2.0;
    return {qRound(centre - widthHz / 2.0), qRound(centre + widthHz / 2.0)};
}

void CommandBar::relabelFilterPills()
{
    Group* g = group(QStringLiteral("Filter"));
    if (!g || !m_slice) { return; }
    const QVector<int> front = frontWidthsHz();
    for (int i = 0; i < g->pills.size(); ++i) {
        QPushButton* b = g->pills.at(i);
        if (i < front.size()) {
            const QPair<int, int> e = edgesForWidth(front.at(i));
            const QString label = filterLabel(e.first, e.second);
            b->setText(label);
            // Dieselbe Mindestbreite wie addPill() beim Anlegen -- nur
            // dort neu berechnet, weil diese Pille mit dem einzelnen
            // Platzhalterzeichen "—" angelegt wird (buildFilterGroup)
            // und addPill()'s Mindestbreite sich an GENAU diesem
            // schmalen Text festgelegt hat. Ohne diese Zeile blieb die
            // Pille auf "—"-Breite gequetscht, sobald hier "2.9k"
            // hineingeschrieben wurde -- derselbe Fehler, den der
            // 2026-08-23-Kommentar in addPill() fuer BAND schon einmal
            // beschreibt ("aus '10 Hz' wurde 'I0 Hz'"), nur an der
            // Stelle, wo die Beschriftung sich NACH dem Anlegen aendert.
            setzeMindestbreite(b, label);
            b->setProperty("loHz", e.first);
            b->setProperty("hiHz", e.second);
            b->setEnabled(true);
            b->setToolTip(QStringLiteral("%1 Hz breit").arg(front.at(i)));
        } else {
            // Weniger als drei Breiten in diesem Modus: die Pille
            // bleibt stehen, aber leer und tot. Sie zu verstecken
            // liesse die Leiste bei jedem Moduswechsel springen.
            b->setText(QStringLiteral("—"));
            b->setEnabled(false);
            b->setToolTip(QString{});
        }
    }
    zieheLeereGruppenZurueck();
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
        tr("Width in Hz:"), cur, 50, 20000, 50, &ok);
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
    // ── Nur, was in DIESEM Bau auch etwas tut ────────────────────────
    //
    // Der Betreiber am 2026-09-17: "bitte kontrollieren nicht nur nr1,
    // nr2 und nr3 sondern auch die anderen optionen im offenen menü."
    // Befund: DFNR (DeepFilterNet3) ist nur enthalten, wenn die
    // Bibliothek beim Bauen gefunden wurde (HAVE_DFNR, siehe
    // ./setup-deepfilter.sh) — in seinem Bau war sie es nicht. BNR
    // (NVIDIA Broadcast) gibt es nur unter Windows mit NVIDIA-Karte
    // (HAVE_BNR, derzeit nirgends gesetzt). MNR (Apple Accelerate) nur
    // auf dem Mac (HAVE_MNR). Trotzdem standen alle acht im „…"-Menue:
    // ein Klick auf DFNR oder BNR leuchtete den Knopf an, schaltete die
    // laufende Minderung AB (die Fahnen sind exklusiv) und tat sonst
    // nichts. Ein Knopf, der angeht und nichts tut, ist eine Luege mit
    // Rueckmeldung — dieselbe Regel, nach der das DSP-Menue in
    // MainWindow MNR/BNR schon lange versteckt. Was fehlt, steht auf
    // der Setup-Seite („DFNR is not enabled in this build"), nicht in
    // der Leiste.
    m_allNr = {
        {QStringLiteral("NR1"),  NrSlot::NR1},
        {QStringLiteral("NR2"),  NrSlot::NR2},
        {QStringLiteral("NR3"),  NrSlot::NR3},
        {QStringLiteral("NR4"),  NrSlot::NR4},
    };
#ifdef HAVE_DFNR
    m_allNr.append({QStringLiteral("DFNR"), NrSlot::DFNR});
#endif
#ifdef HAVE_BNR
    m_allNr.append({QStringLiteral("BNR"),  NrSlot::BNR});
#endif
#ifdef HAVE_MNR
    m_allNr.append({QStringLiteral("MNR"),  NrSlot::MNR});
#endif
    m_allNr.append({QStringLiteral("NNR"),  NrSlot::NNR});

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
                                   },
                                   m_maschinenNr ? m_maschinenNr(slot)
                                                 : QString());
        });
    }

    // Das „…" liest die Liste beim KLICK, nicht beim Bau: die Flex-
    // Fassung ändert sie zur Laufzeit (setNrFlexModus). Sonst gleicher
    // Aufbau wie addOverflow().
    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setFixedWidth(CommandBar::pillBreite());   // gleiche Breite wie jede Pille (2026-11-26)
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(tr("Show all %1").arg(m_allNr.size()));
    g.overflow = more;
    // „…“ in die erste Zeile, solange dort Platz ist; sonst in die zweite.
    if (g.row) { ((static_cast<int>(g.pills.size()) < g.spalten || !g.row2) ? g.row : g.row2)->addWidget(more); }
    connect(more, &QPushButton::clicked, this, [this]() {
        ScopedChildWidget<QMenu> menuOwner(window());   // am Fenster, siehe addOverflow()
        QMenu& m = *menuOwner.get();
        for (const auto& e : nrListe()) {
            QAction* a = m.addAction(e.first);
            connect(a, &QAction::triggered, this,
                    [this, slot = e.second]() { pushNrToModel(slot); });
        }
        m.exec(QCursor::pos());
    });
}

// ── Die Flex-Fassung der Rauschminderung (2026-11-20) ────────────────
//
// Betreiber am 2026-11-20: „bei flexnutzung sollte natürlich auch nur
// das angezeigt werden, was wir können. hier heissen diese glaube ich
// auch anders." Die Pillen stammen aus der Anvelina-Fassung; an der
// Flex stehen Verfahren darin, die wir dort nicht ausführen (DFNR und
// BNR rechnen auf I/Q und sind am Flex-Ton nicht hörbar, NNR ebenso),
// und die Flex nennt ihre Verfahren anders. Die Regeln selbst stehen
// bei RadioModel::flexNrAngeboten / flexNrAnzeige; hier nur die Leiste.
void CommandBar::setNrFlexRegeln(const std::function<bool(NrSlot)>& angeboten,
                                 const std::function<QString(NrSlot)>& anzeige)
{
    m_nrFlexAngeboten = angeboten;
    m_nrFlexAnzeige   = anzeige;
    if (m_nrFlex) { aktualisiereNrPillen(); }
}

void CommandBar::setNrFlexModus(bool flex)
{
    if (m_nrFlex == flex) { return; }
    m_nrFlex = flex;
    aktualisiereNrPillen();
}

QVector<QPair<QString, NrSlot>> CommandBar::nrListe() const
{
    if (!m_nrFlex) { return m_allNr; }
    QVector<QPair<QString, NrSlot>> out;
    for (const auto& e : m_allNr) {
        if (m_nrFlexAngeboten && !m_nrFlexAngeboten(e.second)) { continue; }
        const QString flexName = m_nrFlexAnzeige ? m_nrFlexAnzeige(e.second)
                                                 : QString();
        out.append({flexName.isEmpty() ? e.first : flexName, e.second});
    }
    return out;
}

// Die drei Pillen vorne bekommen die ersten drei der Liste. Die Liste
// hat auch an der Flex immer mindestens drei Einträge (NR1, NR2, NR3
// bleiben dort), daher braucht es kein Ausblenden.
void CommandBar::aktualisiereNrPillen()
{
    Group* g = group(QStringLiteral("NR"));
    if (!g) { return; }
    const auto liste = nrListe();
    for (int i = 0; i < g->pills.size() && i < liste.size(); ++i) {
        g->pills.at(i)->setText(liste.at(i).first);
        g->pills.at(i)->setProperty("nrSlot", static_cast<int>(liste.at(i).second));
    }
    if (g->overflow) {
        g->overflow->setToolTip(tr("Show all %1").arg(liste.size()));
    }
    // Ein Verfahren, das gerade läuft und hier nicht mehr angeboten
    // wird, bleibt in der Maschine; die Leiste zeigt es nicht. Der
    // Stand wird unten aus dem Modell nachgezogen.
    pullFromModel();
}

// ── RATE: was der Empfangskanal wirklich faehrt ──────────────────────
//
// Betreiber am 2026-09-24, nachdem der QRP-Kanal drei Tage lang auf
// 192 kHz lief, waehrend 48 kHz ankamen: "waere es nicht sinnvoll, diese
// rate oben in der taskleiste einzubauen? ... neben NR usw." Angezeigt
// wird die Rate des Kanals, nicht die eingestellte -- der Fehler war
// genau der Unterschied zwischen beiden.
void CommandBar::buildRateGroup(QHBoxLayout* row)
{
    Group& g = addGroup(QStringLiteral("Rate"), row);
    m_rateLabel = new QLabel(QStringLiteral("—"), this);
    m_rateLabel->setAlignment(Qt::AlignCenter);
    m_rateLabel->setToolTip(tr("Not connected"));
    if (g.row) { g.row->addWidget(m_rateLabel); }
    applyRateStyle(false);
}

void CommandBar::applyRateStyle(bool warn)
{
    if (!m_rateLabel) { return; }
    const QString top = QString::fromLatin1(warn ? Style::kAmberBg : Style::kGlassBtnTop);
    const QString bot = QString::fromLatin1(warn ? Style::kAmberBg : Style::kGlassBtnBot);
    const QString border = QString::fromLatin1(warn ? Style::kAmberBorder : Style::kBorder);
    const QString text = QString::fromLatin1(warn ? Style::kAmberText : Style::kTextSecondary);
    m_rateLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %1, stop:1 %2);"
        "  border: 1px solid %3; border-radius: %5px;"
        "  color: %4; font-size: %7px; font-weight: 600;"
        "  padding: 0 %8px; min-height: %6px; max-height: %6px;"
        "}")
        .arg(top, bot, border, text)
        .arg(kPillRadius)
        .arg(pillHoehe())
        .arg(pillSchrift())
        .arg(pillPolster()));
}

namespace {
QString rateText(int hz)
{
    if (hz % 1000 == 0) {
        return QStringLiteral("%1 kHz").arg(hz / 1000);
    }
    return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 1);
}
} // namespace

void CommandBar::setRateReadout(int channelRateHz, int measuredRateHz)
{
    if (!m_rateLabel) { return; }
    if (channelRateHz <= 0) {
        m_rateLabel->setText(QStringLiteral("—"));
        m_rateLabel->setToolTip(tr("Not connected"));
        m_rateMismatchSeconds = 0;
        m_rateWarn = false;
        applyRateStyle(false);
        return;
    }
    m_rateLabel->setText(rateText(channelRateHz));

    // Nur eine gemessene, deutlich abweichende Rate zaehlt, und erst nach
    // zwei Sekunden in Folge: die erste Sekunde nach dem Verbinden ist
    // angebrochen, und UDP kommt gebuendelt.
    const bool mismatch = measuredRateHz > 0
        && std::abs(measuredRateHz - channelRateHz) > channelRateHz / 10;
    m_rateMismatchSeconds = mismatch ? m_rateMismatchSeconds + 1 : 0;
    const bool warn = m_rateMismatchSeconds >= 2;

    if (warn) {
        m_rateLabel->setToolTip(
            tr("The receive channel runs at %1 but %2 is arriving — that is "
                "why it sounds wrong.")
                .arg(rateText(channelRateHz), rateText(measuredRateHz)));
    } else if (measuredRateHz > 0) {
        m_rateLabel->setToolTip(tr("Empfangskanal %1 · gemessen %2")
                                    .arg(rateText(channelRateHz),
                                         rateText(measuredRateHz)));
    } else {
        m_rateLabel->setToolTip(tr("Empfangskanal %1").arg(rateText(channelRateHz)));
    }
    if (warn != m_rateWarn) {
        m_rateWarn = warn;
        applyRateStyle(warn);
    }
    zieheLeereGruppenZurueck();
}

QString CommandBar::rateReadoutText() const
{
    return m_rateLabel ? m_rateLabel->text() : QString();
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
    // ── Ein Druck auf DIGU/DIGL ist ein Druck auf „Digimodus" ────────
    //
    // Betreiber 2026-10-28: „sobald ich auf digi klicke muss das digi
    // fenster automatisch aufgehen und die frequenz dort auch
    // hinspringen".
    //
    // Bis dahin tat dieser Knopf nur eines: die Betriebsart stellen.
    // Wer am Geraet auf DIGU drueckte, stand danach in DIGU -- auf der
    // Frequenz, auf der er vorher war, ohne Dekoderfenster. Die ganze
    // Herrichtung (Anrufstelle, Filter, Fenster nach vorne) hing allein
    // an den Pillen FT8/FT4 daneben, und das ist ein Unterschied, den
    // man dem Knopf nicht ansieht.
    //
    // Das Signal geht HINAUS, statt hier zu handeln: welche Anrufstelle
    // und welches Fenster, weiss das Hauptfenster
    // (MainWindow::aufDigiStellen) -- die Leiste kennt weder Baender
    // noch Applets.
    const bool digimodus = (m == DSPMode::DIGU || m == DSPMode::DIGL);

    if (m_slice) { m_slice->setDspMode(m); }
    if (digimodus) { emit digimodusGewaehlt(); }
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

void CommandBar::pushBandToModel(Band band)
{
    // Anders als Mode/Step: der eigentliche Bandwechsel (Sperr-Check,
    // Speichern/Wiederherstellen der Slice-Einstellungen) lebt in
    // RadioModel::onBandButtonClicked, nicht hier -- die Leiste kennt
    // die Regeln dafuer nicht und soll sie nicht duplizieren. Nur
    // durchreichen. bandRequested->onBandButtonClicked ist eine
    // Direktverbindung (selber Thread), das Modell steht also schon,
    // wenn pullFromModel() unten laeuft.
    emit bandRequested(band);
    pullFromModel();
}

void CommandBar::pullFromModel()
{
    if (!m_slice) {
        // Ohne Modell darf keine Pille etwas behaupten. Vorher liess diese
        // Wache den Checked-Zustand von VOR dem Verschwinden der Scheibe
        // stehen -- und ein Klick auf eine ANDERE Pille in derselben
        // Gruppe zeigte danach zwei gleichzeitig an: Qt schaltet den
        // eigenen Haken jedes checkable QPushButton beim Klick IMMER um
        // (kein QButtonGroup-Exklusivitaet hier), unabhaengig davon, ob
        // der verbundene Slot (pushModeToModel & Co., auch ueber diese
        // Funktion) mangels Modell etwas bewirkt.
        //
        // Live gegen ein angeschlossenes ANAN 10E gefunden (2026-09-07):
        // LSB und DIGL gleichzeitig als aktiv angezeigt, waehrend
        // SliceModel::dspMode() der echten, aktiven Scheibe durchgehend
        // DIGL blieb -- die Leiste hing an einer Scheibe, die verschwunden
        // war, ohne dass ein erneutes attach() das je bemerkt hatte.
        for (Group* g : m_groups) { setActive(*g, QString()); }
        return;
    }

    // ── Das eingeschaltete Band leuchtet ────────────────────────────
    //
    // Die Hausregel (tst_command_bar): in jeder Gruppe ist GENAU EINE
    // Pille an. Ohne das haette die Bandgruppe drei dunkle Knoepfe —
    // und die Leiste sagte nicht, wo man steht.
    //
    // Das Band kommt aus der FREQUENZ, nicht aus einem eigenen Feld:
    // so stimmt die Anzeige auch nach einem Klick in den Panadapter,
    // nach CAT oder nach einem Speicheraufruf.
    if (Group* g = group(QStringLiteral("Band"))) {
        const Band cur = bandFromFrequency(m_slice->frequency());
        QString label;
        for (const auto& e : m_allBands) {
            if (e.second == cur) { label = e.first; break; }
        }
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            // Wie bei Mode: ein Band ausserhalb der ersten drei rueckt
            // an die letzte sichtbare Stelle, sonst zeigt die Leiste
            // drei Pillen, von denen keine an ist.
            if (!visible && !g->pills.isEmpty()) {
                g->pills.last()->setText(label);
            }
            setActive(*g, label);
        }
    }

    if (Group* g = group(QStringLiteral("Ant"))) {
        setActive(*g, m_slice->rxAntenna());
    }

    // ── Digital leuchtet, wenn es WIRKLICH laeuft ───────────────────
    //
    // Anders als Band oder Mode hat diese Gruppe einen Aus-Zustand:
    // wer SSB faehrt, macht weder FT8 noch FT4, und dann darf hier
    // nichts leuchten. (Dieselbe Begruendung wie bei NR, und
    // tst_command_bar nimmt die Gruppe aus derselben Regel aus.)
    //
    // „Laeuft" heisst nicht „DIGU eingestellt": in DIGU sitzt man auch
    // auf PSK oder Olivia. Es heisst Datenmodus UND Skala auf der
    // Anrufstelle -- genau die zwei Dinge, die der Griff herstellt.
    aktualisiereDigiLampe();

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
        const QString label = runningFilterLabel(lo, hi);

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
            // Siehe relabelFilterPills(): dieselbe Pille kann von "—"
            // oder einer schmaleren Breite hierher gelangt sein; ohne
            // Neuberechnung bleibt sie auf der alten Mindestbreite
            // haengen und die neue, breitere Beschriftung wird an
            // beiden Raendern abgeschnitten.
            setzeMindestbreite(last, label);
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
        const auto liste = nrListe();
        for (const auto& e : liste) {
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
    zieheLeereGruppenZurueck();
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

QStringList CommandBar::allNrLabels() const
{
    QStringList out;
    for (const auto& e : nrListe()) { out << e.first; }
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
