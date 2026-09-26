// =================================================================
// src/gui/applets/AppletFloatingWindow.cpp  (Longpath)
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
#include <QDebug>
#include <QPainter>
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
    // Glas & Tiefe (2026-09-17): die Platte ist ein Verlauf, oben hell,
    // unten dunkel, mit feinem Rahmen. Die Applets darin sind
    // durchsichtig (ihre Boedies setzen kein kPanelBg mehr), sonst
    // deckte ein flaches Grau den Verlauf wieder zu. WA_StyledBackground
    // ausdruecklich: ein blosses QWidget malt seinen Stylesheet-Grund
    // sonst nicht selbst, und bisher war es der opake Body, der ihn
    // vortaeuschte.
    // Der Grund wird in paintEvent() gemalt, nicht per Stylesheet: der
    // alte Selektor "AppletFloatingWindow { … }" hat NIE gegriffen
    // (Klasse im Namensraum) — was man als Fenstergrund sah, war der
    // opake Body des Applets — und auch ein objectName-Selektor blieb
    // an diesem rahmenlosen Top-Level-Fenster auf dem Werkzeug-Blatt
    // (tst_tx_entwurf_sheet platte) ohne Wirkung. Ein eigenes
    // paintEvent ist an keiner Selektor-Feinheit interessiert.
    setObjectName(QStringLiteral("appletFloatingWindow"));
    m_createdAt.start();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Die eigene Titelleiste. Sie ersetzt die des Betriebssystems, die
    // wir gleich abschalten, und ist der Griff fuer das ganze Fenster.
    m_titleBar = new WindowTitleBar(
        applet ? applet->appletTitle() : QStringLiteral("Longpath"), this);
    // × heisst andocken -- DIREKT, nicht ueber close(): seit dem
    // 2026-09-17 dockt ein QCloseEvent nie mehr (siehe closeEvent()),
    // weil es nur noch vom System kommt. Der Knopf ist die Absicht des
    // Bedienenden und geht denselben Weg wie der Pfeil.
    connect(m_titleBar, &WindowTitleBar::closeRequested, this, [this]() {
        emit dockRequested(appletId());
    });
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
        // Durchsichtig, samt Viewport: sonst deckt der Rollbereich die
        // Platte (paintEvent) mit seinem Palettengrund zu. Auf dem
        // Werkzeug-Blatt war das Fenster deshalb hell, obwohl Verlauf
        // und Rahmen laengst gemalt wurden — sie lagen DARUNTER.
        m_scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"));
        m_scroll->viewport()->setAutoFillBackground(false);
        m_scroll->setWidget(applet);
        applet->setAutoFillBackground(false);
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
    // ── Ein Schliess-EREIGNIS dockt NIE an ──────────────────────────
    //
    // Der Betreiber am 2026-09-17, zum wiederholten Mal: "profile
    // bleiben wieder nicht automatisch gespeichert!!!!!" Die Logs des
    // Tages: gestartet mit 6 schwebenden Applets, beim Beenden 4
    // gesichert; gestartet mit 4, beim Beenden 1 -- und zwischendrin
    // kein Profilwechsel, kein Andock-Klick, nichts.
    //
    // Hier stand "Schliessen HEISST andocken" (emit dockRequested).
    // Das war fuer den ×-Knopf gedacht -- der geht aber laengst
    // ueber WindowTitleBar::closeRequested direkt an dockRequested
    // und NICHT ueber close(). Ein QCloseEvent erreicht dieses
    // rahmenlose Fenster nur noch vom SYSTEM: beim Beenden ueber Dock
    // oder Apfelmenue (closeAllWindows) oder als macOS-Nebenwirkung
    // eines Vollbild-/Space-Wechsels. Und Qt garantiert nicht, dass
    // MainWindow::closeEvent (setzt m_shuttingDown, nimmt das Profil
    // auf) VOR diesen Ereignissen laeuft -- siehe die Quit-Aktion in
    // MainWindow (2026-08-30, "habe ich gemacht, leider nein"). Kam
    // ein schwebendes Fenster zuerst dran, dockte es sich an, rief
    // captureIntoCurrent()+save() und die Aufnahme beim Beenden sah
    // ein Applet weniger. Je nach Reihenfolge ein anderes: genau das
    // Schrumpfen von Sitzung zu Sitzung.
    //
    // Darum: annehmen, nie andocken. Beim Beenden stirbt das Fenster
    // ohnehin mit dem Programm, und das Profil behaelt es als
    // schwebend -- das ist der Stand, den der Betreiber gesehen hat.
    // Ein System-Schliessen mitten im Betrieb (falls es das gibt)
    // versteckt das Fenster nur; es bleibt in m_floatingApplets, das
    // Profil bleibt richtig, und der Auswaehler (+) zeigt es wieder.
    // Die Zeile darunter sagt im Log, wann und woher es kam.
    qWarning() << "[AppletFloatClose]" << appletId()
               << "spontaneous=" << ev->spontaneous()
               << "-- nicht angedockt, siehe closeEvent()";

    // Vor dem Weggehen durchschreiben: der Zug, dem sofort das
    // Schliessen folgt, darf nicht in der Wartezeit hängen bleiben.
    if (m_settleTimer && m_settleTimer->isActive()) {
        m_settleTimer->stop();
        emit geometrySettled(appletId());
    }
    ev->accept();
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
    // Raster auch fuer die Groesse (2026-09-26): alle vier Kanten, siehe
    // WindowPlacement.h snappedFrameRect.
    snapToGridAfterSettle(this);
}

void AppletFloatingWindow::paintEvent(QPaintEvent*)
{
    // Glas & Tiefe (2026-09-17): die Platte — Verlauf von oben hell
    // nach unten dunkel, feiner Rahmen. Dieselbe Platte wie eine
    // gedockte Zelle (GridCellWidget); die Applets darin sind
    // durchsichtig, damit sie durchscheint.
    QPainter p(this);
    Style::paintWindowPlate(p, rect());
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
