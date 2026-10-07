// =================================================================
// src/gui/widgets/ProfileRail.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See ProfileRail.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/ProfileRail.h"
#include "gui/ScopedChildWidget.h"

#include "gui/LayoutProfiles.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/CommandBar.h"

#include <QEvent>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Longpath {
namespace {

// Die Schiene ist so hoch wie eine Pille AUSSEN: Inhaltshoehe plus die
// beiden Raender. Siehe die Begruendung an kLeistenSeite.
static_assert(ProfileRail::kLeistenSeite == CommandBar::kPillHeight + 2,
              "Die Schiene muss so hoch sein wie eine Pille mit ihren Raendern.");

QString badgeStyle(bool active)
{
    // Das aktive Abzeichen ist gefüllt, die anderen sind Umrisse. Ein
    // Rand allein reicht nicht: bei fünf Profilen sucht man sonst, und
    // die Schiene soll man mit dem Augenwinkel lesen.
    const QString bg     = active ? QString::fromLatin1(Style::kBlueBg)
                                  : QString::fromLatin1(Style::kButtonBg);
    const QString border = active ? QString::fromLatin1(Style::kBlueBorder)
                                  : QString::fromLatin1(Style::kBorder);
    const QString text   = active ? QString::fromLatin1(Style::kBlueText)
                                  : QString::fromLatin1(Style::kTextSecondary);
    // `padding: 0` ist kein Schmuck. Style::kButtonStyle gilt app-weit
    // fuer jeden Knopf ohne eigene Angabe und bringt `padding: 4px 12px`
    // mit: von 30 Pixeln Kantenlaenge blieben damit 4 fuer den
    // Buchstaben, und aus dem „N" wurde ein Schraegstrich. Am
    // 2026-10-07 am Schirm gefunden -- die Schiene am linken Rand zeigte
    // das seit dem app-weiten Blatt vom 2026-09-18, also drei Wochen
    // lang. Dieselbe Falle wie bei den Zoomknoepfen am 2026-09-25
    // (tst_zoom_buttons_legible).
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        "  border-radius: %4px; font-size: 13px; font-weight: bold;"
        "  padding: 0; }"
        "QPushButton:hover { border: 1px solid %5; }")
        .arg(bg, border, text)
        .arg(ProfileRail::kBadgeSide / 2)
        .arg(QString::fromLatin1(Style::kBlueBorder));
}

// ── Zwei Formen fuer zwei Plaetze (2026-10-07) ──────────────────────
//
// Am linken Rand steht die Schiene fuer sich: dort ist die runde
// Scheibe richtig, sie gehoert zu nichts anderem und darf anders
// aussehen als der Fensterinhalt.
//
// In der Leiste steht sie in EINER Reihe mit BAND, MODE, FILTER,
// STEP, NR und RATE. Martin am 2026-10-07 zu genau diesem Bild: „die
// leiste oben passt aber noch immer nicht". Am Bildschirm nachgesehen:
// drei Kreise von 30 px zwischen lauter abgerundeten Rechtecken von
// 27 px, dazu Schrift 13 fett gegen 11/600 und eine flache blaue
// Fuellung gegen den Auswahlverlauf der Pillen. Vier Unterschiede in
// einer Reihe -- das liest sich als eingeklebt, nicht als Teil.
//
// Darum nimmt die Schiene in der Leiste KEINEN eigenen Stil mehr,
// sondern den der Pillen (CommandBar::pillStyle). Nicht nachgebaut,
// sondern derselbe Aufruf: ein nachgebauter Verlauf laeuft beim
// naechsten Stilblatt auseinander, und das faellt niemandem auf.
QString leistenStil()
{
    return CommandBar::pillStyle();
}

} // namespace

QString ProfileRail::initialFor(const QString& name)
{
    // Der erste Buchstabe, nicht das erste Zeichen. Ein Profil, das
    // „ CW" oder „(alt) SSB" heißt, bekäme sonst ein Leerzeichen oder
    // eine Klammer als Abzeichen.
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) { return QString(c.toUpper()); }
    }
    return QStringLiteral("?");
}

ProfileRail::ProfileRail(LayoutProfiles* profiles,
                         Qt::Orientation richtung, QWidget* parent)
    : QWidget(parent), m_profiles(profiles), m_richtung(richtung)
{
    setAttribute(Qt::WA_StyledBackground, true);

    if (m_richtung == Qt::Vertical) {
        setFixedWidth(kWidth);
        // Die Trennlinie steht rechts, zum Fensterinhalt hin.
        setStyleSheet(QStringLiteral(
            "ProfileRail { background: %1; border-right: 1px solid %2; }")
            .arg(QString::fromLatin1(Style::kPanelBg),
                 QString::fromLatin1(Style::kBorderSubtle)));
        m_column = new QVBoxLayout(this);
        m_column->setContentsMargins(7, 9, 7, 9);
    } else {
        // In der Leiste: keine eigene Flaeche und keine Trennlinie. Die
        // Abzeichen sollen wie die Pillen daneben auf dem Leistengrund
        // sitzen, sonst liegt ein Kasten im Kasten.
        setFixedHeight(kLeistenSeite);
        setStyleSheet(QStringLiteral("ProfileRail { background: transparent; }"));
        m_column = new QHBoxLayout(this);
        m_column->setContentsMargins(0, 0, 0, 0);
    }
    m_column->setSpacing(m_richtung == Qt::Vertical ? 7 : 5);

    m_plus = new QPushButton(QStringLiteral("+"), this);
    setzeMasse(m_plus);
    m_plus->setCursor(Qt::PointingHandCursor);
    m_plus->setToolTip(QStringLiteral(
        "Neues Profil aus der jetzigen Anordnung.\n\n"
        "Das bisherige behält seinen Aufbau — wie „Speichern unter“."));
    // Gestrichelt bleibt es an beiden Plaetzen -- das ist die Zusage
    // „hier kommt noch etwas hin" und gilt unabhaengig von der Form.
    // Nur die Rundung folgt dem Platz: Scheibe am Rand, Pillenradius
    // in der Leiste.
    //
    // ── Warum hier `padding: 0 11px` stehen MUSS ────────────────────
    //
    // Am 2026-10-07 am Schirm gemessen: das Plus war 37 Pixel hoch, die
    // Pillen daneben 29. Acht Pixel zu viel, und genau acht stehen in
    // Style::kButtonStyle: `padding: 4px 12px`. Das Blatt gilt fuer
    // jeden Knopf im Fenster, auch fuer einen mit eigenem Stilblatt,
    // solange dieses zum Innenabstand schweigt -- pillStyle() setzt
    // `padding: 0 11px` und ist deshalb 29; hier fehlte es.
    //
    // Die Hoehe steht ausdruecklich dabei, weil dieses Plus NICHT
    // pillStyle() traegt (es ist gestrichelt) und sie sonst niemand
    // liefert.
    m_plus->setStyleSheet(
        m_richtung == Qt::Vertical
            ? QStringLiteral(
                  "QPushButton { background: transparent; color: %1;"
                  "  border: 1px dashed %2; border-radius: %3px;"
                  "  font-size: 16px; padding: 0; }"
                  "QPushButton:hover { color: %4; border: 1px dashed %4; }")
                  .arg(QString::fromLatin1(Style::kTextScale),
                       QString::fromLatin1(Style::kBorder))
                  .arg(kBadgeSide / 2)
                  .arg(QString::fromLatin1(Style::kBlueBorder))
            : QStringLiteral(
                  "QPushButton { background: transparent; color: %1;"
                  "  border: 1px dashed %2; border-radius: %3px;"
                  "  font-size: 14px; padding: 0 11px;"
                  "  min-height: %5px; max-height: %5px; }"
                  "QPushButton:hover { color: %4; border: 1px dashed %4; }")
                  .arg(QString::fromLatin1(Style::kTextScale),
                       QString::fromLatin1(Style::kBorder))
                  .arg(CommandBar::kPillRadius)
                  .arg(QString::fromLatin1(Style::kBlueBorder))
                  .arg(CommandBar::kPillHeight));
    connect(m_plus, &QPushButton::clicked,
            this, &ProfileRail::newProfileRequested);

    rebuild();

    if (m_profiles) {
        connect(m_profiles, &LayoutProfiles::profilesChanged,
                this, &ProfileRail::rebuild);
        connect(m_profiles, &LayoutProfiles::currentChanged,
                this, [this](const QString&) { rebuild(); });
    }
}

// ── Warum die Breite in der Leiste NICHT festgenagelt wird ──────────
//
// Erster Versuch am 2026-10-07 war setFixedSize(27, 27) -- quadratisch
// wie vorher die Scheibe, nur eckig. Am Bildschirm kam dabei aus dem
// „N" ein Schraegstrich, aus dem „B" ein „Ǝ" und aus dem „+" ein
// senkrechter Strich: alle drei an beiden Seiten abgeschnitten.
//
// Grund: Stilblaetter in Qt vererben sich. CommandBar setzt fuer ihre
// Pillen `padding: 0 11px`, und das gilt auch fuer jeden fremden Knopf,
// der in ihr sitzt -- auch fuer einen mit eigenem Stilblatt, solange
// dieses zum Innenabstand nichts sagt. Von 27 Pixeln blieben nach
// 2×11 Abstand und 2×1 Rand genau 3 fuer den Buchstaben.
//
// Die Leiste selbst kennt das Problem seit dem 2026-08-23 („aus ,10 Hz'
// wurde ,I0 Hz'") und loest es ueber die Schriftmetrik. Dieselbe Regel
// hier: die Hoehe steht fest, die Breite ergibt sich aus dem, was der
// Buchstabe braucht. Senkrecht am Rand bleibt es beim Quadrat -- eine
// Scheibe mit zwei verschiedenen Halbmessern waere ein Ei.
void ProfileRail::setzeMasse(QPushButton* b) const
{
    if (m_richtung == Qt::Vertical) {
        b->setFixedSize(kBadgeSide, kBadgeSide);
        return;
    }
    // Die Hoehe kommt aus dem Stilblatt der Pillen (min-height =
    // max-height), nicht von hier. Sie hier zu setzen hiesse, dieselbe
    // Zahl ein zweites Mal zu pflegen -- und beim naechsten Stilblatt
    // stuenden die Abzeichen zwei Pixel daneben, ohne dass es auffiele.
    b->setMinimumWidth(b->fontMetrics().horizontalAdvance(b->text()) + 20);
}

void ProfileRail::rebuild()
{
    // Abzeichen einsammeln und wegwerfen, das Plus bleibt. Es unten neu
    // anzulegen wäre einfacher und würde bei jedem Profilwechsel den
    // Knopf unter dem Mauszeiger austauschen.
    for (QPushButton* b : m_badges) {
        m_column->removeWidget(b);
        b->deleteLater();
    }
    m_badges.clear();
    m_closers.clear();   // Kinder der Abzeichen, sterben mit ihnen
    m_order.clear();
    m_column->removeWidget(m_plus);
    while (QLayoutItem* it = m_column->takeAt(0)) { delete it; }

    if (!m_profiles) {
        m_column->addWidget(m_plus, 0, mittig());
        m_column->addStretch(1);
        return;
    }

    const QString current = m_profiles->current();
    for (const QString& name : m_profiles->names()) {
        auto* b = new QPushButton(initialFor(name), this);
        setzeMasse(b);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(name);
        if (m_richtung == Qt::Vertical) {
            b->setStyleSheet(badgeStyle(name == current));
        } else {
            // In der Leiste traegt das Abzeichen den Pillenstil, und das
            // aktive wird ueber :checked hervorgehoben -- derselbe Weg,
            // den CommandBar fuer BAND und MODE geht.
            //
            // autoExclusive: ohne das nimmt ein Klick auf das BEREITS
            // aktive Abzeichen ihm die Fuellung. LayoutProfiles meldet
            // bei activate() auf den schon laufenden Namen keine
            // Aenderung, also baut sich die Schiene nicht neu auf, und
            // der falsche Zustand bliebe stehen. Mit autoExclusive
            // laesst Qt das letzte gedrueckte Element gedrueckt.
            b->setCheckable(true);
            b->setAutoExclusive(true);
            b->setChecked(name == current);
            b->setStyleSheet(leistenStil());
        }
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QPushButton::clicked, this, [this, name]() {
            if (m_profiles) { m_profiles->activate(name); }
        });
        connect(b, &QWidget::customContextMenuRequested,
                this, [this, b, name](const QPoint& p) {
            showMenuFor(name, b->mapToGlobal(p));
        });
        // ── Das X am Abzeichen (2026-08-19) ─────────────────────────
        //
        // Auf Ansage des Betreibers: „man sollte die runden Buttons
        // (profile) auch mit einem X am Tab loeschen koennen."
        //
        // Loeschen gab es schon per Rechtsklick — aber ein Rechtsklick
        // ist kein Angebot: nichts auf dem Abzeichen sagt, dass es geht.
        // Dasselbe Argument wie beim Mausrad auf der Weltkugel.
        //
        // Es erscheint beim Ueberfahren, nicht dauernd: acht Abzeichen
        // mit acht Kreuzen sehen aus wie eine Warnung, und man loescht
        // ein Profil selten.
        if (m_profiles->names().size() > 1) {
            auto* x = new QPushButton(QStringLiteral("\u00D7"), b);
            x->setFixedSize(14, 14);
            x->setCursor(Qt::PointingHandCursor);
            // Waagrecht ist die Breite nicht mehr die Kantenlaenge
            // (siehe setzeMasse), darum an der tatsaechlichen Breite
            // ausrichten -- sonst saesse das Kreuz mitten im Buchstaben.
            x->move(b->sizeHint().width() - 15, 1);
            x->setToolTip(QStringLiteral("Profil „%1\u201C loeschen").arg(name));
            x->setStyleSheet(QStringLiteral(
                // `padding: 0` auch hier: 14 Pixel Kantenlaenge minus
                // der app-weite Innenabstand von 2x12 waere negativ --
                // das Kreuz haette gar keine Flaeche. Siehe HAUSSTIL.md,
                // „der geerbte Innenabstand".
                "QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 7px; font-size: 11px; font-weight: bold;"
                "  padding: 0; }"
                "QPushButton:hover { background: %3; color: #ffffff; }")
                    .arg(QString::fromLatin1(Style::kButtonBg),
                         QString::fromLatin1(Style::kTextSecondary),
                         QString::fromLatin1(Style::kRedBg)));
            x->hide();
            b->installEventFilter(this);
            m_closers.insert(name, x);

            connect(x, &QPushButton::clicked, this, [this, name]() {
                emit removeRequested(name);
            });
        }

        m_column->addWidget(b, 0, mittig());
        m_badges.insert(name, b);
        m_order << name;
    }

    m_column->addWidget(m_plus, 0, mittig());
    // Senkrecht schiebt die Dehnung die Abzeichen nach oben. Waagrecht
    // darf sie NICHT dazu: die Leiste setzt die Breite, und eine Dehnung
    // darin wuerde die Abzeichen auseinanderziehen.
    if (m_richtung == Qt::Vertical) { m_column->addStretch(1); }
}

// Das X erscheint, solange der Zeiger auf dem Abzeichen steht.
bool ProfileRail::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        const bool show = (event->type() == QEvent::Enter);
        for (auto it = m_badges.cbegin(); it != m_badges.cend(); ++it) {
            if (it.value() != watched) { continue; }
            if (QPushButton* x = m_closers.value(it.key(), nullptr)) {
                x->setVisible(show);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ProfileRail::showMenuFor(const QString& name, const QPoint& globalPos)
{
    // Am Hauptfenster, nicht an der Leiste (2026-09-27): die Leiste hat
    // ein eigenes natives Fenster, und ein Menue daran bekam von Qt keinen
    // Fensterbezug ("QWidgetWindow(... ProfileRailClassWindow) must be a
    // top level window." bei jedem Rechtsklick in Martins Protokoll) --
    // auf macOS kann es dann hinter schwebenden Werkzeugfenstern landen.
    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<ProfileRail> self(this);
    QMenu& menu = *menuOwner.get();
    // Der volle Name als Überschrift. Auf dem Abzeichen steht nur ein
    // Buchstabe, und ein Menü mit „Löschen“ über einem „C“ ist zu wenig
    // Auskunft für etwas, das nicht rückgängig zu machen ist.
    QAction* title = menu.addAction(name);
    title->setEnabled(false);
    menu.addSeparator();

    QAction* save = menu.addAction(QStringLiteral("Jetzt sichern"));
    // captureIntoCurrent() sichert immer das AKTIVE Profil, gleich, auf
    // welches Abzeichen man rechtsklickt -- der Menuepunkt darf also nur
    // dort erscheinen, sonst waere "Jetzt sichern" auf einem anderen
    // Abzeichen ein Versprechen, das der Aufruf gar nicht einloest.
    save->setEnabled(m_profiles && m_profiles->current() == name);
    QAction* exportToDesktop = menu.addAction(QStringLiteral("Auf Schreibtisch sichern…"));
    QAction* importFromDesktop = menu.addAction(QStringLiteral("Vom Schreibtisch laden…"));
    menu.addSeparator();
    QAction* ren = menu.addAction(QStringLiteral("Umbenennen…"));
    QAction* dup = menu.addAction(QStringLiteral("Duplizieren…"));
    menu.addSeparator();
    QAction* del = menu.addAction(QStringLiteral("Löschen"));
    // Das letzte Profil lässt sich nicht löschen. Ohne Profil gehört
    // das Fenster keinem, und die nächste Umgestaltung landete nirgends.
    del->setEnabled(m_profiles && m_profiles->names().size() > 1);

    // ── Wohin die Schiene gehoert, entscheidet der Betreiber ─────────
    //
    // 2026-10-06, nachdem sie von links in die Leiste gewandert war:
    // „macht sinn, dass man dies individuell verschieben und anpassen
    // kann". Also nicht neu festgenagelt, sondern hier umschaltbar --
    // an der Stelle, an der man ohnehin rechtsklickt, statt in einer
    // Einstellungsseite, die man erst finden muss.
    menu.addSeparator();
    QAction* verschieben = menu.addAction(
        m_richtung == Qt::Horizontal
            ? QStringLiteral("Schiene an den linken Rand")
            : QStringLiteral("Schiene in die Leiste oben"));

    QAction* chosen = menu.exec(globalPos);
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    // Weil das Menue am Hauptfenster haengt, kann die Leiste auch allein
    // gestorben sein: dafuer `self`.
    if (!menuOwner || !self) { return; }
    if (chosen == save)      { emit saveRequested(name); }
    else if (chosen == exportToDesktop) { emit exportRequested(name); }
    else if (chosen == importFromDesktop) { emit importRequested(name); }
    else if (chosen == ren)  { emit renameRequested(name); }
    else if (chosen == dup)  { emit duplicateRequested(name); }
    else if (chosen == del)  { emit removeRequested(name); }
    else if (chosen == verschieben) {
        emit placementToggleRequested(m_richtung == Qt::Horizontal
                                          ? Qt::Vertical : Qt::Horizontal);
    }
}

QStringList ProfileRail::badges() const { return m_order; }

QString ProfileRail::activeBadge() const
{
    return m_profiles ? m_profiles->current() : QString();
}

bool ProfileRail::clickBadge(const QString& name)
{
    QPushButton* b = m_badges.value(name, nullptr);
    if (!b) { return false; }
    b->click();
    return true;
}

} // namespace Longpath
