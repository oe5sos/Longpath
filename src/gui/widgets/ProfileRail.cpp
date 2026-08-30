// =================================================================
// src/gui/widgets/ProfileRail.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See ProfileRail.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/ProfileRail.h"

#include "gui/LayoutProfiles.h"
#include "gui/StyleConstants.h"

#include <QEvent>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {
namespace {

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
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        "  border-radius: %4px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { border: 1px solid %5; }")
        .arg(bg, border, text)
        .arg(ProfileRail::kBadgeSide / 2)
        .arg(QString::fromLatin1(Style::kBlueBorder));
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

ProfileRail::ProfileRail(LayoutProfiles* profiles, QWidget* parent)
    : QWidget(parent), m_profiles(profiles)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kWidth);
    setStyleSheet(QStringLiteral(
        "ProfileRail { background: %1; border-right: 1px solid %2; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorderSubtle)));

    m_column = new QVBoxLayout(this);
    m_column->setContentsMargins(7, 9, 7, 9);
    m_column->setSpacing(7);

    m_plus = new QPushButton(QStringLiteral("+"), this);
    m_plus->setFixedSize(kBadgeSide, kBadgeSide);
    m_plus->setCursor(Qt::PointingHandCursor);
    m_plus->setToolTip(QStringLiteral(
        "Neues Profil aus der jetzigen Anordnung.\n\n"
        "Das bisherige behält seinen Aufbau — wie „Speichern unter“."));
    m_plus->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1;"
        "  border: 1px dashed %2; border-radius: %3px; font-size: 16px; }"
        "QPushButton:hover { color: %4; border: 1px dashed %4; }")
        .arg(QString::fromLatin1(Style::kTextScale),
             QString::fromLatin1(Style::kBorder))
        .arg(kBadgeSide / 2)
        .arg(QString::fromLatin1(Style::kBlueBorder)));
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
        m_column->addWidget(m_plus, 0, Qt::AlignHCenter);
        m_column->addStretch(1);
        return;
    }

    const QString current = m_profiles->current();
    for (const QString& name : m_profiles->names()) {
        auto* b = new QPushButton(initialFor(name), this);
        b->setFixedSize(kBadgeSide, kBadgeSide);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(name);
        b->setStyleSheet(badgeStyle(name == current));
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
            x->move(kBadgeSide - 15, 1);
            x->setToolTip(QStringLiteral("Profil „%1\u201C loeschen").arg(name));
            x->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 7px; font-size: 10px; font-weight: bold; }"
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

        m_column->addWidget(b, 0, Qt::AlignHCenter);
        m_badges.insert(name, b);
        m_order << name;
    }

    m_column->addWidget(m_plus, 0, Qt::AlignHCenter);
    m_column->addStretch(1);
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
    QMenu menu(this);
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
    menu.addSeparator();
    QAction* ren = menu.addAction(QStringLiteral("Umbenennen…"));
    QAction* dup = menu.addAction(QStringLiteral("Duplizieren…"));
    menu.addSeparator();
    QAction* del = menu.addAction(QStringLiteral("Löschen"));
    // Das letzte Profil lässt sich nicht löschen. Ohne Profil gehört
    // das Fenster keinem, und die nächste Umgestaltung landete nirgends.
    del->setEnabled(m_profiles && m_profiles->names().size() > 1);

    QAction* chosen = menu.exec(globalPos);
    if (chosen == save)      { emit saveRequested(name); }
    else if (chosen == exportToDesktop) { emit exportRequested(name); }
    else if (chosen == ren)  { emit renameRequested(name); }
    else if (chosen == dup)  { emit duplicateRequested(name); }
    else if (chosen == del)  { emit removeRequested(name); }
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
