// =================================================================
// src/gui/SideAreaWindow.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe SideAreaWindow.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-25 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/SideAreaWindow.h"

#include "gui/FramelessResizer.h"
#include "gui/MacFloatingWindowBehavior.h"
#include "gui/StyleConstants.h"
#include "gui/WindowChrome.h"

#include <QAbstractButton>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace Longpath {

// ── Ein Symbol in der Leiste ─────────────────────────────────────────
//
// Selbst gemalt statt Stylesheet: die Auswahl ist ein Verlauf mit
// Lichtkante und Hof (Glas & Tiefe, kGlassSel*), und der Kompass für
// Rotor/Log ist eine Zeichnung, kein Zeichen. Alle anderen Seiten tragen
// ein Kürzel aus ihrem Titel — dieselbe Idee wie die Buchstaben der
// Profilschiene links (ProfileRail): mit dem Augenwinkel lesbar, der
// volle Name steht im Tooltip.
class SideRailButton : public QAbstractButton {
public:
    enum class Kind { Text, Compass, Plus };

    SideRailButton(Kind kind, const QString& label, QWidget* parent)
        : QAbstractButton(parent), m_kind(kind), m_label(label)
    {
        setFixedSize(28, 28);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
    }

    void setSelected(bool on) { if (m_selected != on) { m_selected = on; update(); } }
    bool isSelected() const { return m_selected; }

    std::function<void()> onContextMenu;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor ink = m_selected
            ? QColor(Style::hexRole(Style::kGlassSelText))
            : QColor(Style::hexRole(Style::kTitleText));

        if (m_selected) {
            // Hof, dann die Platte mit Verlauf und Rahmen.
            QColor glow(Style::hexRole(Style::kGlassSelBorder));
            glow.setAlpha(60);
            p.setPen(QPen(glow, 3.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 7, 7);
            QLinearGradient g(r.topLeft(), r.bottomLeft());
            g.setColorAt(0.0, QColor(Style::hexRole(Style::kGlassSelTop)));
            g.setColorAt(1.0, QColor(Style::hexRole(Style::kGlassSelBot)));
            p.setBrush(g);
            p.setPen(QPen(QColor(Style::hexRole(Style::kGlassSelBorder)), 1.0));
            p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), 6, 6);
        } else if (underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 18));
            p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), 6, 6);
        }

        const QPointF c = r.center();
        switch (m_kind) {
        case Kind::Compass: {
            p.setPen(QPen(ink, 1.3));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 8.0, 8.0);
            // Nadel: Nordhälfte in Bernstein, Südhälfte in der Schrift.
            QPainterPath north;
            north.moveTo(c.x(), c.y() - 6.5);
            north.lineTo(c.x() + 2.2, c.y());
            north.lineTo(c.x() - 2.2, c.y());
            north.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(Style::hexRole(Style::kAmberText)));
            p.drawPath(north);
            QPainterPath south;
            south.moveTo(c.x(), c.y() + 6.5);
            south.lineTo(c.x() + 2.2, c.y());
            south.lineTo(c.x() - 2.2, c.y());
            south.closeSubpath();
            p.setBrush(ink);
            p.drawPath(south);
            break;
        }
        case Kind::Plus: {
            QPen dash(QColor(Style::hexRole(Style::kTextScale)), 1.0, Qt::DashLine);
            p.setPen(dash);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(4, 4, -4, -4), 5, 5);
            p.setPen(QPen(QColor(Style::hexRole(Style::kTitleText)), 1.4));
            p.drawLine(QPointF(c.x() - 4, c.y()), QPointF(c.x() + 4, c.y()));
            p.drawLine(QPointF(c.x(), c.y() - 4), QPointF(c.x(), c.y() + 4));
            break;
        }
        case Kind::Text: {
            QFont f = font();
            f.setPixelSize(m_label.size() > 2 ? 9 : 10);
            f.setBold(true);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
            p.setFont(f);
            p.setPen(ink);
            p.drawText(r, Qt::AlignCenter, m_label);
            break;
        }
        }
    }

    void contextMenuEvent(QContextMenuEvent* ev) override
    {
        if (onContextMenu) { onContextMenu(); ev->accept(); return; }
        QAbstractButton::contextMenuEvent(ev);
    }

private:
    Kind m_kind;
    QString m_label;
    bool m_selected{false};
};

namespace {

// Das Kürzel einer Seite: zwei Anfangsbuchstaben bei mehreren Wörtern
// („Bandwidth Filter" → BF), sonst der Titel selbst, wenn er kurz ist
// („RX"), sonst seine ersten zwei Buchstaben.
QString railLabelFor(const QString& title)
{
    const QStringList words = title.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() >= 2) {
        return (words.at(0).left(1) + words.at(1).left(1)).toUpper();
    }
    if (title.size() <= 3) { return title.toUpper(); }
    return title.left(2).toUpper();
}

int minExpandedWidth()
{
    return Style::kAppletPanelW + SideAreaWindow::kRailW;
}

} // namespace

SideAreaWindow::SideAreaWindow(QWidget* parent)
    // Qt::Tool wie AppletFloatingWindow: auf macOS ein NSPanel auf der
    // Palettenebene — über dem Hauptfenster, zusammen mit den anderen
    // schwebenden Fenstern.
    : QWidget(parent, Qt::Tool)
{
    setWindowTitle(QStringLiteral("Seitenbereich"));
    setObjectName(QStringLiteral("sideAreaWindow"));

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Links: Kopf + Seiten ─────────────────────────────────────────
    m_body = new QWidget(this);
    auto* bodyLay = new QVBoxLayout(m_body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    m_titleBar = new WindowTitleBar(QStringLiteral("Seitenbereich"), m_body);
    m_titleBar->setLockKey(QStringLiteral("SideArea"));
    // × und Doppelklick/↙ heißen hier „zuklappen": ein Seitenbereich
    // hat keine Spalte, in die er zurückkönnte, und schließen soll ihn
    // niemand aus Versehen — die Leiste bleibt stehen.
    connect(m_titleBar, &WindowTitleBar::closeRequested,
            this, [this]() { setCollapsed(true); });
    connect(m_titleBar, &WindowTitleBar::dockRequested,
            this, [this]() { setCollapsed(true); });
    bodyLay->addWidget(m_titleBar);
    m_stack = new QStackedWidget(m_body);
    bodyLay->addWidget(m_stack, 1);
    outer->addWidget(m_body, 1);

    // ── Rechts: die Leiste ───────────────────────────────────────────
    m_rail = new QWidget(this);
    m_rail->setFixedWidth(kRailW);
    m_railLayout = new QVBoxLayout(m_rail);
    m_railLayout->setContentsMargins(5, 8, 5, 8);
    m_railLayout->setSpacing(6);
    m_railLayout->addStretch(1);
    m_addButton = new SideRailButton(SideRailButton::Kind::Plus, QString(), m_rail);
    m_addButton->setToolTip(QStringLiteral("Fenster in den Seitenbereich legen"));
    connect(m_addButton, &QAbstractButton::clicked, this, [this]() {
        emit addRequested(m_addButton->mapToGlobal(QPoint(0, m_addButton->height())));
    });
    m_railLayout->addWidget(m_addButton, 0, Qt::AlignHCenter);
    outer->addWidget(m_rail, 0);

    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setMinimumWidth(minExpandedWidth());
    setMinimumHeight(160);
    FramelessResizer::install(this, 6, m_titleBar->sizeHint().height());
    attachResizeGrip(this);

    m_settleTimer = new QTimer(this);
    m_settleTimer->setSingleShot(true);
    m_settleTimer->setInterval(kSettleMs);
    connect(m_settleTimer, &QTimer::timeout, this, &SideAreaWindow::stateSettled);

    enableFullScreenAuxiliaryBehavior(this);
}

SideAreaWindow::~SideAreaWindow() = default;

void SideAreaWindow::addPage(const QString& id, const QString& title, QWidget* content)
{
    if (id.isEmpty() || !content || m_pages.contains(id)) { return; }

    Page page;
    page.title = title;
    page.content = content;

    // Rollbereich, durchsichtig — dieselbe Begründung wie in
    // AppletFloatingWindow: sonst zieht die Mindestgröße des Inhalts das
    // Fenster auf, und der Palettengrund deckt die Platte zu.
    page.scroll = new QScrollArea(m_stack);
    page.scroll->setWidgetResizable(true);
    page.scroll->setFrameShape(QFrame::NoFrame);
    page.scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"));
    page.scroll->viewport()->setAutoFillBackground(false);
    content->setParent(page.scroll);
    content->setAutoFillBackground(false);
    page.scroll->setWidget(content);
    content->show();
    m_stack->addWidget(page.scroll);

    const auto kind = id == QLatin1String("WinRotorLog")
        ? SideRailButton::Kind::Compass : SideRailButton::Kind::Text;
    page.button = new SideRailButton(kind, railLabelFor(title), m_rail);
    page.button->setToolTip(title);
    connect(page.button, &QAbstractButton::clicked,
            this, [this, id]() { railClicked(id); });
    page.button->onContextMenu = [this, id]() {
        // Auf dem Heap und mit WA_DeleteOnClose, nicht als Stapelmenü mit
        // exec(): ein exec() hier liefe über eine verschachtelte
        // Ereignisschleife, und ein Schließen des Fensters darin hat am
        // 2026-08-30 genau so einen Absturz gebracht (ProfileRail).
        auto* menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose, true);
        menu->addAction(QStringLiteral("Aus dem Seitenbereich nehmen"),
                        this, [this, id]() { emit removeRequested(id); });
        menu->popup(QCursor::pos());
    };
    // Vor Stretch und „+": die Seiten stehen oben, das Plus unten.
    m_railLayout->insertWidget(m_order.size(), page.button, 0, Qt::AlignHCenter);

    m_pages.insert(id, page);
    m_order.append(id);

    if (m_active.isEmpty()) {
        m_active = id;
        m_stack->setCurrentWidget(page.scroll);
        m_titleBar->setTitle(title);
        emit activeChanged(id);
    }
    refreshRail();
    scheduleSettle();
}

QWidget* SideAreaWindow::takePage(const QString& id)
{
    auto it = m_pages.find(id);
    if (it == m_pages.end()) { return nullptr; }
    Page page = it.value();
    m_pages.erase(it);
    const int idx = m_order.indexOf(id);
    m_order.removeAll(id);

    // Aus dem Rollbereich UND aus der Elternschaft — takeWidget(), nicht
    // removeWidget(): der Rollbereich besitzt sein Widget, und mit ihm
    // stürbe es sonst beim Aufräumen unten (siehe releaseApplet()).
    QWidget* content = page.content.data();
    if (page.scroll) {
        if (page.scroll->widget() == content) { page.scroll->takeWidget(); }
        m_stack->removeWidget(page.scroll);
        page.scroll->deleteLater();
    }
    if (content) { content->setParent(nullptr); }
    if (page.button) {
        m_railLayout->removeWidget(page.button);
        page.button->deleteLater();
    }

    if (m_active == id) {
        m_active.clear();
        if (!m_order.isEmpty()) {
            setActive(m_order.at(qBound(0, idx, int(m_order.size()) - 1)));
        } else {
            m_titleBar->setTitle(QStringLiteral("Seitenbereich"));
            emit activeChanged(QString());
        }
    }
    refreshRail();
    scheduleSettle();
    return content;
}

QWidget* SideAreaWindow::pageContent(const QString& id) const
{
    auto it = m_pages.constFind(id);
    return it == m_pages.cend() ? nullptr : it->content.data();
}

void SideAreaWindow::setActive(const QString& id)
{
    auto it = m_pages.constFind(id);
    if (it == m_pages.cend()) { return; }
    const bool changed = (m_active != id);
    m_active = id;
    m_stack->setCurrentWidget(it->scroll);
    m_titleBar->setTitle(it->title);
    if (m_collapsed) { setCollapsed(false); }
    refreshRail();
    if (changed) {
        emit activeChanged(id);
        scheduleSettle();
    }
}

void SideAreaWindow::railClicked(const QString& id)
{
    if (!m_pages.contains(id)) { return; }
    if (id == m_active) {
        // Das aktive Symbol: auf und zu, wie ein Schalter.
        setCollapsed(!m_collapsed);
        return;
    }
    setActive(id);
}

int SideAreaWindow::expandedWidth() const
{
    if (m_collapsed) { return qMax(m_expandedWidth, minExpandedWidth()); }
    return qMax(width(), minExpandedWidth());
}

void SideAreaWindow::setExpandedWidth(int w)
{
    m_expandedWidth = qMax(w, minExpandedWidth());
    if (!m_collapsed) {
        m_applyingCollapse = true;
        resize(m_expandedWidth, height());
        m_applyingCollapse = false;
    }
}

void SideAreaWindow::setCollapsed(bool on)
{
    if (on == m_collapsed) { return; }
    m_applyingCollapse = true;
    const QRect g = geometry();
    int delta = 0;
    if (on) {
        m_expandedWidth = qMax(g.width(), minExpandedWidth());
        delta = g.width() - kRailW;
        m_body->hide();
        setMinimumWidth(kRailW);
        // Der rechte Rand bleibt stehen: die Leiste wandert nicht.
        setGeometry(g.x() + delta, g.y(), kRailW, g.height());
        m_collapsed = true;
    } else {
        const int w = qMax(m_expandedWidth, minExpandedWidth());
        delta = w - g.width();
        m_body->show();
        setMinimumWidth(minExpandedWidth());
        setGeometry(g.x() - delta, g.y(), w, g.height());
        m_collapsed = false;
    }
    m_applyingCollapse = false;
    refreshRail();
    emit collapsedChanged(m_collapsed, delta);
    scheduleSettle();
}

QVariantMap SideAreaWindow::captureState() const
{
    QVariantMap s;
    s.insert(QStringLiteral("pages"), m_order);
    s.insert(QStringLiteral("active"), m_active);
    s.insert(QStringLiteral("collapsed"), m_collapsed);
    // Immer die aufgeklappte Lage: der rechte Rand ist in beiden
    // Zuständen derselbe, links liegt die volle Breite.
    const QRect g = geometry();
    const int w = expandedWidth();
    s.insert(QStringLiteral("x"), g.x() + g.width() - w);
    s.insert(QStringLiteral("y"), g.y());
    s.insert(QStringLiteral("w"), w);
    s.insert(QStringLiteral("h"), g.height());
    return s;
}

QStringList SideAreaWindow::railIdsForTest() const
{
    return m_order;
}

QString SideAreaWindow::titleForTest() const
{
    auto it = m_pages.constFind(m_active);
    return it == m_pages.cend() ? QString() : it->title;
}

void SideAreaWindow::refreshRail()
{
    for (auto it = m_pages.begin(); it != m_pages.end(); ++it) {
        if (it->button) {
            // Zugeklappt ist nichts ausgewählt: kein Symbol soll behaupten,
            // seine Seite sei zu sehen.
            it->button->setSelected(!m_collapsed && it.key() == m_active);
        }
    }
}

void SideAreaWindow::scheduleSettle()
{
    if (m_settleTimer) { m_settleTimer->start(); }
}

void SideAreaWindow::paintEvent(QPaintEvent*)
{
    // Dieselbe Platte wie AppletFloatingWindow, die Leiste eine Spur
    // dunkler und mit einer feinen Kante zum Inhalt.
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(Style::hexRole(Style::kGlassPanelTop)));
    g.setColorAt(1.0, QColor(Style::hexRole(Style::kGlassPanelBot)));
    p.fillRect(rect(), g);
    if (m_rail) {
        const QRect rr = m_rail->geometry();
        p.fillRect(rr, QColor(0, 0, 0, 60));
        if (!m_collapsed) {
            p.setPen(QColor(255, 255, 255, 14));
            p.drawLine(rr.topLeft(), rr.bottomLeft());
        }
    }
    p.setPen(QColor(Style::hexRole(Style::kBorderSubtle)));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void SideAreaWindow::moveEvent(QMoveEvent* ev)
{
    QWidget::moveEvent(ev);
    if (!m_applyingCollapse) { scheduleSettle(); }
}

void SideAreaWindow::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    if (!m_applyingCollapse && !m_collapsed) {
        m_expandedWidth = width();
    }
    if (!m_applyingCollapse) { scheduleSettle(); }
}

void SideAreaWindow::closeEvent(QCloseEvent* ev)
{
    // Ein Schließ-EREIGNIS kommt nur noch vom System (Beenden über Dock
    // oder Apfelmenü) — wie bei AppletFloatingWindow ändert es nichts am
    // Zustand, sonst schriebe das Beenden „zugeklappt" oder „leer" ins
    // Profil.
    ev->accept();
}

} // namespace Longpath
