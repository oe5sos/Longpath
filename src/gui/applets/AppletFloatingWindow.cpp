// =================================================================
// src/gui/applets/AppletFloatingWindow.cpp  (NereusSDR)
// =================================================================
// Siehe AppletFloatingWindow.h für Zweck, Eigentum und Geometrie.
// =================================================================

#include "gui/applets/AppletFloatingWindow.h"

#include "gui/StyleConstants.h"
#include "gui/FramelessMoveHelper.h"
#include "gui/FramelessResizer.h"
#include "gui/MacFloatingWindowBehavior.h"
#include "gui/WindowChrome.h"
#include "gui/WindowPlacement.h"
#include "gui/applets/AppletWidget.h"

#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Longpath {

AppletFloatingWindow::AppletFloatingWindow(AppletWidget* applet,
                                           const QString& panelId,
                                           int dockIndex, QWidget* parent)
    // Qt::Tool statt Qt::Window: auf macOS ein NSPanel mit der
    // Sammelregel „FullScreenAuxiliary" — es schwebt ueber einem
    // Elternfenster im Vollbild, statt in dessen Flaeche einzuziehen.
    // Siehe die ausfuehrliche Begruendung in PanFloatingWindow.cpp.
    : QWidget(parent, Qt::Tool)
    , m_applet(applet)
    , m_panelId(panelId)
    , m_dockIndex(dockIndex)
{
    setWindowTitle(applet ? applet->appletTitle()
                          : QStringLiteral("Longpath"));
    setStyleSheet(QStringLiteral("AppletFloatingWindow { background: %1; }")
                      .arg(Style::kPanelBg));
    m_createdAt.start();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Die eigene Titelleiste. Sie ersetzt die des Betriebssystems, die
    // wir gleich abschalten, und ist der Griff fuer das ganze Fenster.
    m_titleBar = new WindowTitleBar(
        applet ? applet->appletTitle() : QStringLiteral("Longpath"), this);
    connect(m_titleBar, &WindowTitleBar::closeRequested,
            this, &QWidget::close);
    connect(m_titleBar, &WindowTitleBar::dockRequested, this, [this]() {
        // ── Der Knopf, der zufaellig genau dort landet ────────────────
        //
        // Betreiber 2026-08-30: "frequenzwindow laesst sich nicht
        // ordentlich abheften, springt immer." Ursache: dieses Fenster
        // erscheint exakt dort, wo eben noch die Spaltenzeile mit dem
        // ↗-Knopf stand (detachApplet() setzt es bewusst dorthin, siehe
        // MainWindow.cpp "pickedUpAt" — Absicht, nicht Zufall). Beide
        // Kopfleisten reihen ihre Knoepfe rechtsbuendig auf: der ↗ in
        // der Spalte ist der zweite von rechts (neben ✕), und der ↙
        // hier im neuen Fenster ist ES AUCH (neben ✕, mit dem
        // Schloss davor). Beide Fenster sind ungefaehr gleich breit,
        // also faellt ein zweiter Klick an derselben Bildschirmstelle
        // — instinktiv, wenn vom Abloesen selbst nichts zu sehen war —
        // direkt auf diesen Knopf und dockt sofort wieder an.
        //
        // Die kurze Sperrfrist aendert am Knopf selbst nichts (Design
        // bleibt Sache des Betreibers), sie schluckt nur den einen
        // Klick, der noch zum alten Gestus gehoert.
        if (m_createdAt.isValid() && m_createdAt.elapsed() < kDockGuardMs) {
            return;
        }
        emit dockRequested(appletId());
    });
    m_titleBar->setLockKey(QStringLiteral("Applet_%1").arg(panelId));
    lay->addWidget(m_titleBar);

    if (applet) {
        // ── Der Inhalt kommt in einen Rollbereich ───────────────────
        //
        // Sonst bestimmt die Mindestgroesse des Applets die des
        // Fensters, und bei den groesseren Applets heisst das:
        // bildschirmfuellend. Der Betreiber am 2026-08-20, nachdem ich
        // es schon einmal falsch repariert hatte: „es geht immer ein
        // neues fenster bildschirm fuellend auf, dass ist falsch".
        //
        // Mein erster Anlauf setzte nur minimumHeight(0) auf dem
        // Applet. Das half beim RX-Applet (300x368 gemessen) und half
        // nicht bei den anderen, weil deren Anordnung die Untergrenze
        // aus ihren KINDERN zieht — die bleibt.
        //
        // Ein Rollbereich schneidet diese Kette durch: er hat selbst
        // fast keine Untergrenze, und was nicht hineinpasst, wird
        // gerollt statt das Fenster aufzuziehen. Dieselbe Loesung, die
        // die Applet-Spalte schon benutzt.
        applet->setParent(this);
        applet->show();

        m_scroll = new QScrollArea(this);
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_scroll->setWidget(applet);
        lay->addWidget(m_scroll);
    }
    setMinimumWidth(Style::kAppletPanelW);

    // ── Ziehen und Groessenaendern, wie bei AetherSDR ────────────────
    //
    // Der Betreiber, sinngemaess zum zehnten Mal am 2026-08-20: „jedes
    // Fenster muss sich an der oberen Leiste ueberall hinschieben
    // lassen, und unten rechts muss ein Griff sein, mit dem ich es
    // groesser und kleiner ziehen kann."
    //
    // Bis heute hing hier ein gewoehnliches Qt::Window mit dem Rahmen
    // des Betriebssystems. Das laesst sich zwar schieben, sieht aber
    // aus wie ein fremdes Fenster und hat oben eine graue Leiste, die
    // nicht zum Programm gehoert.
    //
    // AetherSDR macht es rahmenlos und legt zwei Helfer darunter, die
    // wir am 2026-08-20 portiert haben:
    //   FramelessMoveHelper  — Ziehen an einer Handhabe (hier: die
    //                          Titelleiste des Applets)
    //   FramelessResizer     — Groessenaendern an ALLEN Kanten und
    //                          Ecken, unten rechts eingeschlossen
    //
    // topMoveReserve == kTitleBarH: der obere Streifen gehoert dem
    // Ziehen, nicht dem Groessenaendern. Ohne das schnappt der Resizer
    // den Griff weg, und das Fenster laesst sich nicht mehr bewegen —
    // AetherSDR hat das als #4266 gelernt.
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    FramelessResizer::install(this, 6, m_titleBar->height());
    attachResizeGrip(this);

    m_settleTimer = new QTimer(this);
    m_settleTimer->setSingleShot(true);
    m_settleTimer->setInterval(kSettleMs);
    connect(m_settleTimer, &QTimer::timeout, this, [this]() {
        emit geometrySettled(appletId());
    });

    // Betreiber, wiederholt gemeldet: dieses Fenster blieb im nativen
    // Vollbild auf dem alten Space zurueck, statt mit dem Hauptfenster
    // zu wandern -- der "FullScreenAuxiliary"-Kommentar oben an
    // Qt::Tool war nie mehr als eine Annahme, siehe
    // MacFloatingWindowBehavior.h.
    enableFullScreenAuxiliaryBehavior(this);
}

void AppletFloatingWindow::applyDefaultSize()
{
    if (m_sizedOnce) { return; }   // wer nachher zieht, darf es behalten
    m_sizedOnce = true;

    // Der Rollbereich hat selbst kaum eine Untergrenze; hier wird nur
    // noch festgelegt, wie klein das Fenster von Hand werden darf.
    setMinimumHeight(m_titleBar ? m_titleBar->height() + 60 : 80);

    // Die Wunschgroesse ist die des Inhalts — aber gedeckelt. Ein
    // abgeloestes Applet ist ein Werkzeug neben der Konsole, kein
    // zweiter Vollbild.
    const int barH = m_titleBar ? m_titleBar->height() : 0;
    QSize want(Style::kAppletPanelW + 40,
               qMax(160, m_applet ? m_applet->sizeHint().height() + barH
                                  : 240));
    if (m_applet) {
        want.setWidth(qMax(want.width(), m_applet->sizeHint().width() + 4));
    }
    if (QScreen* sc = screen()) {
        const QSize avail = sc->availableSize();
        want.setWidth (qMin(want.width(),  avail.width()  - 80));
        // Hoechstens zwei Drittel des Schirms: ein abgeloestes Applet
        // ist ein Werkzeug neben der Konsole, kein zweiter Vollbild.
        want.setHeight(qMin(want.height(), (avail.height() * 2) / 3));
        want.setWidth (qMin(want.width(),  (avail.width()  * 1) / 2));
    }
    resize(want);
}

AppletFloatingWindow::~AppletFloatingWindow() = default;

AppletWidget* AppletFloatingWindow::releaseApplet()
{
    AppletWidget* a = m_applet.data();
    if (!a) { return nullptr; }
    // Aus dem Behaelter UND aus der Elternschaft. Nur zu entnehmen
    // liesse das Applet Kind dieses Fensters — es stürbe mit ihm, und
    // der Aufrufer hielte einen baumelnden Zeiger auf etwas, das er
    // gerade zurückbekommen zu haben glaubt.
    //
    // takeWidget(), nicht removeWidget(): seit dem 2026-08-20 haengt
    // das Applet in einem QScrollArea (siehe Baukasten), und ein
    // Rollbereich BESITZT sein Widget. layout()->removeWidget(a)
    // fuende es dort gar nicht — das Applet bliebe Kind des
    // Rollbereichs und stuerbe beim deleteLater des Fensters, mitten
    // im Andocken.
    if (m_scroll && m_scroll->widget() == a) {
        m_scroll->takeWidget();          // gibt die Elternschaft ab
    } else if (layout()) {
        layout()->removeWidget(a);
    }
    a->setParent(nullptr);
    m_applet.clear();
    return a;
}

void AppletFloatingWindow::closeEvent(QCloseEvent* ev)
{
    // Schliessen HEISST andocken. Ein Applet, das man wegklickt und das
    // danach nirgends mehr auftaucht, ist verloren — es hat, anders als
    // ein Werkzeugfenster, keinen eigenen Menüeintrag zum Wiederholen.
    // Die Sichtbarkeit regelt der AppletVisibilityController, und der
    // sagt zu diesem Applet weiterhin „sichtbar".
    //
    // Vor dem Andocken durchschreiben: der Zug, dem sofort das
    // Schliessen folgt, darf nicht in der Wartezeit hängen bleiben.
    if (m_settleTimer && m_settleTimer->isActive()) {
        m_settleTimer->stop();
        emit geometrySettled(appletId());
    }
    ev->accept();
    emit dockRequested(appletId());
}

void AppletFloatingWindow::moveEvent(QMoveEvent* ev)
{
    QWidget::moveEvent(ev);
    scheduleGeometryReport();
    // Betreiber 2026-09-02: schwebende Fenster sollen zueinander
    // fluchten. Gedaempft (siehe WindowPlacement.h) -- ein direktes
    // Runden hier wuerde gegen das native Ziehen kaempfen.
    snapToGridAfterSettle(this);
}

void AppletFloatingWindow::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    scheduleGeometryReport();
}

void AppletFloatingWindow::scheduleGeometryReport()
{
    // isVisible(): das Herstellen beim Start setzt die Geometrie, bevor
    // das Fenster steht. Ohne diese Bedingung meldete allein das
    // Herstellen eine Änderung zurück ins Profil — und schriebe damit
    // gerade das, was es eben gelesen hat.
    if (m_settleTimer && isVisible()) {
        m_settleTimer->start();
    }
}

} // namespace Longpath
