// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/WindowChrome.cpp  (Longpath)
// =================================================================
// Siehe WindowChrome.h fuer Zweck, Herkunft und Modification history.
// =================================================================

#include "gui/WindowChrome.h"

#include "gui/FramelessMoveHelper.h"
#include "gui/StyleConstants.h"
#include "core/AppSettings.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>

namespace Longpath {

namespace {

constexpr int kBarHeight  = 22;
constexpr int kGripSize   = 16;
// Der gelbe Strich links in der Leiste. Zeus setzt dort eine warme
// Marke; sie sagt „hier anfassen". Wir nehmen denselben Ton, nicht den
// kuehlen Akzent — der Akzent gehoert den Bedienelementen, diese Marke
// gehoert dem Fenster.
//
// Aus der Palette, nicht als eigene Zahl. Hier stand #d8a13a, waehrend
// ContainerWidget denselben Strich seit dem 2026-08-19 in
// Style::kAmberText (#c2924f) malt — zwei Ambertoene fuer dieselbe
// Aussage, und der zweite kam von mir. Genau danach hat der Betreiber
// am 2026-08-20 gefragt: ob das Design ueberall dasselbe ist.
constexpr auto kGripStripe = Style::kAmberText;

// Ein Schluessel je Fenster, damit sich zwei festgestellte Fenster
// nicht gegenseitig ueberschreiben.
QString settingsKeyFor(const QString& id)
{
    return QStringLiteral("WindowLocked_") + id;
}

} // namespace

// ── WindowTitleBar ──────────────────────────────────────────────────

WindowTitleBar::WindowTitleBar(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kBarHeight);
    setCursor(Qt::OpenHandCursor);
    // Der Selektor traegt den Klassennamen, nicht das nackte QWidget.
    // Eine Regel „QWidget { ... }" faellt sonst auf JEDES Kind durch und
    // radiert Knopfraender und -fuellungen aus — genau das ist uns am
    // 2026-08-19 bei den DVK-Zeilen passiert, wo nur noch leere
    // Kaestchen uebrig blieben.
    setStyleSheet(QStringLiteral("WindowTitleBar { %1 }")
                      .arg(Style::titleBarStyle()));

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 4, 0);
    lay->setSpacing(6);

    auto* stripe = new QLabel(this);
    stripe->setFixedWidth(4);
    stripe->setStyleSheet(QStringLiteral("background: %1;").arg(kGripStripe));
    lay->addWidget(stripe);

    m_label = new QLabel(title, this);
    m_label->setStyleSheet(
        QStringLiteral("color: %1; font-size: 10px; font-weight: 600;"
                       " letter-spacing: 0.4px; background: transparent;")
            .arg(Style::kTextPrimary));
    lay->addWidget(m_label);
    lay->addStretch();

    // Das Schloss ganz links neben den anderen Knoepfen: es betrifft
    // das Fenster als Ganzes, nicht seinen Inhalt.
    m_lockBtn = new QPushButton(this);
    m_lockBtn->setFixedSize(16, 16);
    m_lockBtn->setCursor(Qt::ArrowCursor);
    m_lockBtn->setCheckable(true);
    connect(m_lockBtn, &QPushButton::toggled,
            this, &WindowTitleBar::setLocked);
    lay->addWidget(m_lockBtn);

    auto* dock = new QPushButton(QStringLiteral("↙"), this);
    dock->setFixedSize(16, 16);
    dock->setToolTip(QStringLiteral("Zurueck in die Spalte"));
    dock->setCursor(Qt::ArrowCursor);
    dock->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; background: transparent;"
                       " border: none; font-size: 11px; }"
                       "QPushButton:hover { background: %2; }")
            .arg(Style::kTextPrimary, Style::kButtonHover));
    connect(dock, &QPushButton::clicked, this, &WindowTitleBar::dockRequested);
    lay->addWidget(dock);

    auto* close = new QPushButton(QStringLiteral("✕"), this);
    close->setFixedSize(16, 16);
    close->setCursor(Qt::ArrowCursor);
    close->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; background: transparent;"
                       " border: none; font-size: 10px; }"
                       "QPushButton:hover { background: #a03030; color: #fff; }")
            .arg(Style::kTextPrimary));
    connect(close, &QPushButton::clicked, this, &WindowTitleBar::closeRequested);
    lay->addWidget(close);

    applyLockVisuals();   // offenes Schloss, richtiger Zeiger
}

void WindowTitleBar::setTitle(const QString& title)
{
    if (m_label) { m_label->setText(title); }
}

void WindowTitleBar::setLockKey(const QString& key)
{
    m_lockKey = key;
    if (m_lockKey.isEmpty()) { return; }
    // Gemerkten Zustand holen. Ohne das waere das Feststellen eine
    // Geste, die jeder Neustart zurueckdreht.
    const bool on = AppSettings::instance()
                        .value(settingsKeyFor(m_lockKey),
                               QStringLiteral("False"))
                        .toString() == QStringLiteral("True");
    setLocked(on);
}

void WindowTitleBar::setLocked(bool on)
{
    if (m_locked == on) { applyLockVisuals(); return; }
    m_locked = on;

    // Die Eigenschaft am FENSTER, nicht an der Leiste: FramelessResizer
    // horcht auf dem Fenster und fragt genau diese ab.
    if (QWidget* win = window()) {
        win->setProperty("longpathWindowLocked", on);
    }
    // Der Anfasser unten rechts gehoert zum selben Fenster.
    if (QWidget* win = window()) {
        for (ResizeGrip* g : win->findChildren<ResizeGrip*>()) {
            g->setLocked(on);
        }
    }
    applyLockVisuals();

    if (!m_lockKey.isEmpty()) {
        AppSettings::instance().setValue(
            settingsKeyFor(m_lockKey),
            on ? QStringLiteral("True") : QStringLiteral("False"));
    }
    emit lockedChanged(on);
}

void WindowTitleBar::applyLockVisuals()
{
    if (!m_lockBtn) { return; }
    QSignalBlocker block(m_lockBtn);
    m_lockBtn->setChecked(m_locked);
    m_lockBtn->setText(m_locked ? QStringLiteral("🔒")
                                : QStringLiteral("🔓"));
    m_lockBtn->setToolTip(m_locked
        ? QStringLiteral("Festgestellt — Klicken zum Loesen. "
                         "Das Fenster laesst sich weder schieben noch "
                         "in der Groesse aendern.")
        : QStringLiteral("Feststellen — dann bleibt das Fenster, wo es "
                         "steht, und behaelt seine Groesse."));
    m_lockBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: transparent; border: none;"
                       " font-size: 9px; padding: 0; }"
                       "QPushButton:hover { background: %1; }")
            .arg(Style::kButtonHover));
    setCursor(m_locked ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void WindowTitleBar::mousePressEvent(QMouseEvent* ev)
{
    // Festgestellt heisst festgestellt.
    if (m_locked) { QWidget::mousePressEvent(ev); return; }
    if (FramelessMoveHelper::start(this, ev)) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    QWidget::mousePressEvent(ev);
}

void WindowTitleBar::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_locked) { QWidget::mouseMoveEvent(ev); return; }
    if (FramelessMoveHelper::move(this, ev)) { return; }
    QWidget::mouseMoveEvent(ev);
}

void WindowTitleBar::mouseReleaseEvent(QMouseEvent* ev)
{
    if (FramelessMoveHelper::finish(this, ev)) {
        setCursor(Qt::OpenHandCursor);
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void WindowTitleBar::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        emit dockRequested();
        ev->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(ev);
}

// ── ResizeGrip ──────────────────────────────────────────────────────

ResizeGrip::ResizeGrip(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(kGripSize, kGripSize);
    setCursor(Qt::SizeFDiagCursor);
    setToolTip(QStringLiteral("Groesse ziehen"));
    // Der Anfasser sitzt UEBER dem Inhalt, nicht daneben. Ein Applet,
    // das bis in die Ecke reicht, faengt sonst den Druck ab, und der
    // Anfasser waere zwar zu sehen, aber nicht zu benutzen.
    //
    // Und er muss selbst ein natives Fenster sein.
    //
    // Der Panadapter ist ein QRhiWidget mit WA_NativeWindow; unser
    // eigener Quelltext haelt in SpectrumWidget.cpp:551 fest, warum das
    // hier zaehlt: „QRhiWidget with WA_NativeWindow on macOS does not
    // support child widget overlays". Ein natives NSView zeichnet immer
    // ueber allen NICHT-nativen Geschwistern, und kein raise() aendert
    // daran etwas. Ein gewoehnlicher Anfasser laege im Panadapterfenster
    // unsichtbar hinter der Wasserfallflaeche — genau der Fehler, der
    // uns am 2026-08-19/20 die Kacheln gekostet hat.
    //
    // Native Geschwister dagegen sortieren sich untereinander, und dann
    // greift raise() wieder.
    setAttribute(Qt::WA_NativeWindow);
    raise();
}

void ResizeGrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    // Klammern, keine runden: QPen pen(QColor(...)) liest der
    // Uebersetzer als Funktionsdeklaration, nicht als Objekt.
    QColor ink(QString::fromLatin1(Style::kTextPrimary));
    if (m_locked) { ink.setAlpha(70); }   // festgestellt: nur noch angedeutet
    QPen pen{ink};
    pen.setWidth(1);
    p.setPen(pen);
    // Drei Schraegstriche, von aussen nach innen kuerzer — die Form, die
    // jedes Fenstersystem an dieser Ecke zeichnet. Sie wird ohne
    // Beschriftung verstanden.
    const int w = width() - 3;
    const int h = height() - 3;
    for (int i = 0; i < 3; ++i) {
        const int off = i * 4;
        p.drawLine(w - off, h, w, h - off);
    }
}

void ResizeGrip::setLocked(bool on)
{
    if (m_locked == on) { return; }
    m_locked = on;
    setCursor(on ? Qt::ArrowCursor : Qt::SizeFDiagCursor);
    setToolTip(on ? QStringLiteral("Fenster ist festgestellt")
                  : QStringLiteral("Groesse ziehen"));
    update();   // blass zeichnen
}

void ResizeGrip::mousePressEvent(QMouseEvent* ev)
{
    if (m_locked) { QWidget::mousePressEvent(ev); return; }
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    QWidget* win = window();
    if (!win) { return; }
    m_dragging    = true;
    m_pressGlobal = ev->globalPosition().toPoint();
    m_startSize   = win->size();
    ev->accept();
}

void ResizeGrip::mouseMoveEvent(QMouseEvent* ev)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(ev);
        return;
    }
    QWidget* win = window();
    if (!win) { return; }
    const QPoint d = ev->globalPosition().toPoint() - m_pressGlobal;
    // In beide Achsen zugleich, und nie unter die Untergrenze des
    // Fensters: ein Zug nach links oben verkleinert, bis das Fenster
    // sagt, dass es nicht weiter kann.
    const QSize floorSz = win->minimumSize();
    win->resize(qMax(m_startSize.width()  + d.x(), floorSz.width()),
                qMax(m_startSize.height() + d.y(), floorSz.height()));
    ev->accept();
}

void ResizeGrip::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        m_dragging = false;
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

namespace {

// Haelt den Anfasser in der Ecke, wenn sich das Fenster aendert.
class GripKeeper : public QObject {
public:
    GripKeeper(QWidget* window, ResizeGrip* grip)
        : QObject(window), m_window(window), m_grip(grip)
    {
        window->installEventFilter(this);
        place();
    }

    void place()
    {
        if (!m_window || !m_grip) { return; }
        m_grip->move(m_window->width()  - m_grip->width()  - 2,
                     m_window->height() - m_grip->height() - 2);
        m_grip->raise();
    }

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (obj == m_window
            && (ev->type() == QEvent::Resize || ev->type() == QEvent::Show)) {
            place();
        }
        return QObject::eventFilter(obj, ev);
    }

private:
    QWidget*    m_window{nullptr};
    ResizeGrip* m_grip{nullptr};
};

} // namespace

ResizeGrip* attachResizeGrip(QWidget* window)
{
    if (!window) { return nullptr; }
    auto* grip = new ResizeGrip(window);
    new GripKeeper(window, grip);   // Eigentum liegt beim Fenster
    return grip;
}

} // namespace Longpath
