// =================================================================
// src/gui/widgets/WidgetPicker.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See WidgetPicker.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-15 — Kategorien, Suche, Karten statt flacher Hakenliste.
// =================================================================

#include "gui/widgets/WidgetPicker.h"

#include "gui/StyleConstants.h"
#include "gui/applets/AppletVisibilityController.h"

#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>

#include <functional>
#include <QVBoxLayout>

namespace NereusSDR {
namespace {

/// Eine Karte, die auf einen Klick reagiert. QFrame kann das nicht von
/// sich aus, und ein Knopf mit zwei Textzeilen unterschiedlicher Größe
/// wäre ein Stylesheet-Ringkampf.
class CardFrame : public QWidget {
public:
    using QWidget::QWidget;
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && isEnabled() && onClick) {
            onClick();
        }
        QWidget::mouseReleaseEvent(e);
    }
};

} // namespace

QString WidgetPicker::allCategory() { return QStringLiteral("Alle"); }

WidgetPicker::WidgetPicker(AppletVisibilityController* vis, QWidget* parent)
    : QWidget(parent), m_vis(vis), m_category(allCategory())
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "WidgetPicker { background: %1; border: 1px solid %2;"
        "               border-radius: 8px; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorder)));
    // ── Wie groß ─────────────────────────────────────────────────────
    //
    // Die Obergrenze ist der Punkt. Ohne sie wächst adjustSize() auf
    // die Höhe ALLER Karten — bei dreizehn Widgets war das Fenster
    // 1100 Pixel hoch und verdeckte das halbe Programm, obwohl der
    // Rollbereich darunter genau dafür da ist. Ein Auswähler soll neben
    // dem stehen, worüber man entscheidet, nicht davor.
    setMinimumSize(620, 380);
    setMaximumHeight(560);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Hier stand eine Überschrift „WIDGET HINZUFÜGEN". Sie stammt aus
    // der Zeit, als das Ding ein rahmenloser Aufklapper war. Jetzt ist
    // es ein Fenster mit Titelleiste, und die sagt dasselbe — zweimal
    // derselbe Satz übereinander ist keine Gestaltung, sondern ein
    // vergessener Umbau.
    outer->addSpacing(10);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    outer->addLayout(body, 1);

    // ── Kategorien links ─────────────────────────────────────────────
    m_categories = new QListWidget(this);
    m_categories->setFixedWidth(178);
    m_categories->setFrameShape(QFrame::NoFrame);
    m_categories->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none;"
        "  outline: none; padding: 4px 12px; }"
        "QListWidget::item { color: %1; padding: 9px 12px;"
        "  border-radius: 6px; }"
        "QListWidget::item:selected { background: %2; color: %3; }")
        .arg(QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kBlueBg),
             QString::fromLatin1(Style::kBlueText)));
    body->addWidget(m_categories);
    connect(m_categories, &QListWidget::currentTextChanged,
            this, [this](const QString& t) { setCategory(t); });

    // ── Suche und Karten rechts ──────────────────────────────────────
    auto* right = new QVBoxLayout;
    right->setContentsMargins(6, 4, 16, 16);
    right->setSpacing(10);
    body->addLayout(right, 1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Widgets durchsuchen…"));
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 6px; color: %3; padding: 7px 11px;"
        "  font-size: 12px; }")
        .arg(QString::fromLatin1(Style::kInsetBg),
             QString::fromLatin1(Style::kBorder),
             QString::fromLatin1(Style::kTextPrimary)));
    right->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged,
            this, [this](const QString& t) { setSearch(t); });

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* holder = new QWidget(scroll);
    m_cards = new QVBoxLayout(holder);
    m_cards->setContentsMargins(0, 0, 0, 0);
    m_cards->setSpacing(8);
    m_cards->addStretch(1);
    scroll->setWidget(holder);
    right->addWidget(scroll, 1);

    rebuild();

    if (m_vis) {
        connect(m_vis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString&, bool) { refresh(); });
        connect(m_vis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString&, bool) { refresh(); });
    }
}

void WidgetPicker::rebuild()
{
    m_byId.clear();
    m_order.clear();
    while (m_cards->count() > 1) {                 // die Dehnung bleibt
        QLayoutItem* it = m_cards->takeAt(0);
        if (QWidget* w = it->widget()) { w->deleteLater(); }
        delete it;
    }
    m_categories->clear();
    if (!m_vis) { return; }

    m_categories->addItem(allCategory());
    for (const QString& c : m_vis->categories()) { m_categories->addItem(c); }
    m_categories->setCurrentRow(0);

    for (const QString& id : m_vis->registeredIds()) {
        const bool avail = m_vis->isAvailable(id);

        auto* frame = new CardFrame(this);
        frame->setAttribute(Qt::WA_StyledBackground, true);
        frame->setCursor(avail ? Qt::PointingHandCursor : Qt::ArrowCursor);
        frame->setEnabled(avail);

        auto* col = new QVBoxLayout(frame);
        col->setContentsMargins(15, 11, 15, 12);
        col->setSpacing(4);

        auto* titleRow = new QHBoxLayout;
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(9);
        auto* name = new QLabel(m_vis->displayName(id), frame);
        {
            QFont f = name->font();
            f.setPixelSize(13);
            f.setBold(true);
            name->setFont(f);
        }
        titleRow->addWidget(name);
        if (!avail) {
            // Die Vorlage schreibt DISABLED neben den Namen und den
            // Grund darunter. Sagen, wo man es einschaltet, ist der
            // Unterschied zwischen einer Auskunft und einem grauen Feld.
            auto* badge = new QLabel(QStringLiteral("ABGESCHALTET"), frame);
            QFont bf = badge->font();
            bf.setPixelSize(9);
            bf.setBold(true);
            bf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
            badge->setFont(bf);
            badge->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                .arg(QString::fromLatin1(Style::kAmberText)));
            titleRow->addWidget(badge);
        }
        titleRow->addStretch(1);
        col->addLayout(titleRow);

        const QString sub = avail
            ? m_vis->keywords(id).join(QStringLiteral(" · "))
            : QStringLiteral("Die zugehörige Hardware oder Hauptschaltung "
                             "ist aus.");
        auto* keys = new QLabel(sub, frame);
        {
            QFont f = keys->font();
            f.setPixelSize(11);
            keys->setFont(f);
        }
        keys->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
        col->addWidget(keys);

        auto click = [this, id]() {
            if (!m_vis) { return; }
            const bool next = !m_vis->isVisible(id);
            m_vis->setVisible(id, next);
            emit toggled(id, next);
        };
        frame->onClick = click;

        m_cards->insertWidget(m_cards->count() - 1, frame);
        m_byId.insert(id, Card{frame, id, avail, m_vis->isVisible(id), click});
        m_order << id;
    }
    refresh();
}

void WidgetPicker::refresh()
{
    if (!m_vis) { return; }
    for (const QString& id : m_order) {
        Card& c = m_byId[id];
        c.checked = m_vis->isVisible(id);
        c.enabled = m_vis->isAvailable(id);
        if (!c.frame) { continue; }
        c.frame->setEnabled(c.enabled);
        // Eine gewählte Karte trägt den Auswahlrahmen — genau eine
        // Eigenschaft, zwei Zustände, kein Haken nötig.
        c.frame->setStyleSheet(QStringLiteral(
            "CardFrame { background: %1; border: 1px solid %2;"
            "            border-radius: 8px; }")
            .arg(QString::fromLatin1(c.checked ? Style::kBlueBg
                                               : Style::kInsetBg),
                 QString::fromLatin1(c.checked ? Style::kBlueBorder
                                               : Style::kBorderSubtle)));
    }
    applyFilter();
}

void WidgetPicker::applyFilter()
{
    if (!m_vis) { return; }
    for (const QString& id : m_order) {
        Card& c = m_byId[id];
        if (!c.frame) { continue; }
        const bool byCat = m_category == allCategory()
                        || m_vis->category(id) == m_category;
        const bool byText = m_vis->matches(id, m_needle);
        c.frame->setVisible(byCat && byText);
    }
}

void WidgetPicker::setCategory(const QString& category)
{
    if (category.isEmpty() || m_category == category) { return; }
    m_category = category;
    applyFilter();
}

void WidgetPicker::setSearch(const QString& needle)
{
    if (m_needle == needle) { return; }
    m_needle = needle;
    applyFilter();
}

QStringList WidgetPicker::categoryColumn() const
{
    QStringList out;
    for (int i = 0; i < m_categories->count(); ++i) {
        out << m_categories->item(i)->text();
    }
    return out;
}

QStringList WidgetPicker::entries() const
{
    QStringList out;
    for (const QString& id : m_order) {
        const Card* c = card(id);
        // isVisible() taugt hier nicht: solange der Dialog selbst nicht
        // gezeigt wurde, ist jedes Kind unsichtbar, und der Test saehe
        // eine leere Liste. isHidden() fragt nur, ob es AUSDRUECKLICH
        // versteckt wurde — und genau das macht der Filter.
        if (c && c->frame && !c->frame->isHidden()) { out << id; }
    }
    return out;
}

WidgetPicker::Card* WidgetPicker::card(const QString& id)
{
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it.value();
}

const WidgetPicker::Card* WidgetPicker::card(const QString& id) const
{
    const auto it = m_byId.constFind(id);
    return it == m_byId.constEnd() ? nullptr : &it.value();
}

bool WidgetPicker::isChecked(const QString& id) const
{
    const Card* c = card(id);
    return c && c->checked;
}

bool WidgetPicker::isEnabled(const QString& id) const
{
    const Card* c = card(id);
    return c && c->enabled;
}

bool WidgetPicker::toggle(const QString& id)
{
    Card* c = card(id);
    if (!c || !c->enabled || !c->frame || c->frame->isHidden()) {
        return false;
    }
    if (!c->click) { return false; }
    c->click();
    return true;
}

// ── Das Plus ─────────────────────────────────────────────────────────

AddWidgetButton::AddWidgetButton(AppletVisibilityController* vis,
                                 QWidget* parent)
    : QWidget(parent), m_vis(vis)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);

    auto* plus = new QPushButton(QStringLiteral("+"), this);
    plus->setFixedSize(kSide, kSide);
    plus->setCursor(Qt::PointingHandCursor);
    plus->setToolTip(QStringLiteral("Widget hinzufügen oder entfernen"));
    // Gestrichelt wie in der Vorlage: der Rand sagt „hier ist noch
    // nichts, hier kann etwas hin". Ein durchgezogener Rahmen sähe aus
    // wie ein Knopf, der schon etwas ist.
    plus->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1;"
        "  border: 1px dashed %2; border-radius: 8px;"
        "  font-size: 17px; }"
        "QPushButton:hover { color: %3; border-color: %4; background: %5; }")
        .arg(QString::fromLatin1(Style::kTextScale),
             QString::fromLatin1(Style::kBorder),
             QString::fromLatin1(Style::kTextPrimary),
             QString::fromLatin1(Style::kBlueBorder),
             QString::fromLatin1(Style::kButtonBg)));
    col->addWidget(plus);

    connect(plus, &QPushButton::clicked, this, &AddWidgetButton::openPicker);
}

void AddWidgetButton::openPicker()
{
    if (!m_picker) {
        // ── Ein Fenster, kein Aufklapper ─────────────────────────────
        //
        // Erste Fassung war Qt::Popup an der Mausposition. Zwei Fehler
        // in drei Zeilen, beide am 2026-08-15 im Betrieb aufgefallen:
        //
        // Ein Popup schließt beim ersten Klick daneben. Wer sein
        // Fenster einrichtet, schaltet aber fünf Widgets nacheinander
        // ein und will dazwischen hinsehen — und ein Popup lässt sich
        // grundsätzlich nicht verschieben, also verdeckt es genau das,
        // worüber man gerade entscheidet.
        //
        // Qt::Tool: eigenes Fenster mit Titelleiste, verschiebbar,
        // bleibt offen, bleibt über dem Hauptfenster.
        m_picker = new WidgetPicker(m_vis, this);
        m_picker->setWindowFlags(Qt::Tool);
        m_picker->setWindowTitle(QStringLiteral("Widget hinzufügen"));
        connect(m_picker, &WidgetPicker::toggled,
                this, &AddWidgetButton::toggled);
    }
    m_picker->refresh();
    m_picker->adjustSize();
    m_picker->move(placeNear(m_picker->size()));
    m_picker->show();
    m_picker->raise();
    m_picker->activateWindow();
}

QPoint AddWidgetButton::placeNear(const QSize& want) const
{
    // Unter dem Plus, rechtsbündig — und dann in den Bildschirm
    // geschoben.
    //
    // Vorher wurde stur an QCursor::pos() gesetzt. Das Plus sitzt am
    // rechten Ende der Kommandoleiste, also stand der Zeiger dort, also
    // öffnete ein 620 Pixel breites Fenster über den rechten Rand
    // hinaus: sichtbar blieb ein Streifen von zwei Zentimetern. Nicht
    // „schlecht platziert" — unbenutzbar.
    const QPoint anchor = mapToGlobal(QPoint(width(), height() + 6));
    QRect r(anchor.x() - want.width(), anchor.y(),
            want.width(), want.height());

    const QScreen* s = screen();
    if (!s) { s = QGuiApplication::primaryScreen(); }
    if (s) {
        const QRect avail = s->availableGeometry();
        // Erst rechts/unten hineinschieben, dann links/oben — in dieser
        // Reihenfolge, sonst schiebt der zweite Schritt den ersten
        // wieder hinaus, wenn das Fenster größer als der Schirm ist.
        if (r.right()  > avail.right())  { r.moveRight(avail.right() - 8); }
        if (r.bottom() > avail.bottom()) { r.moveBottom(avail.bottom() - 8); }
        if (r.left() < avail.left()) { r.moveLeft(avail.left() + 8); }
        if (r.top()  < avail.top())  { r.moveTop(avail.top() + 8); }
    }
    return r.topLeft();
}

} // namespace NereusSDR
