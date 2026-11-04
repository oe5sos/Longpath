// =================================================================
// src/contest/ContestFenster.cpp  (Longpath)
// =================================================================
//
// Longpath-original (2026-11-04). Siehe ContestFenster.h. Diese Datei
// gehoert zur Bibliothek LongpathContestBruecke und sieht die
// Kopfdateien des Contestprogramms -- und NUR die; nichts aus
// Longpaths src/.
// =================================================================
#include "ContestFenster.h"

#include "app/AppController.h"
#include "app/Datenverzeichnis.h"
#include "app/SingleInstanceGuard.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/StyleKit.h"

#include <QDir>
#include <QPointer>
#include <QTimer>

namespace Longpath {

struct ContestFenster::Innen {
    std::unique_ptr<Contestprogramm::SingleInstanceGuard> wache;
    std::unique_ptr<Contestprogramm::AppController>       steuerung;
    QPointer<Contestprogramm::MainWindow>                 fenster;
};

ContestFenster::ContestFenster(QObject* parent)
    : QObject(parent)
    , m_innen(std::make_unique<Innen>())
{
}

ContestFenster::~ContestFenster()
{
    schliesse();
}

QString ContestFenster::datenverzeichnis()
{
    return Contestprogramm::standardDatenverzeichnis();
}

bool ContestFenster::oeffne(QString* grund, QWidget* elternFenster)
{
    const bool schonDa = m_innen->fenster != nullptr;
    if (!lade(grund, elternFenster)) { return false; }
    if (schonDa) {
        m_innen->fenster->setWindowState(
            (m_innen->fenster->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    m_innen->fenster->show();
    m_innen->fenster->raise();
    m_innen->fenster->activateWindow();
    if (!schonDa) { emit geoeffnet(); }
    return true;
}

bool ContestFenster::lade(QString* grund, QWidget* elternFenster)
{
    if (m_innen->fenster) { return true; }

    const QString ordner = datenverzeichnis();
    if (!QDir().mkpath(ordner)) {
        if (grund) {
            *grund = QStringLiteral("Cannot create the data directory: %1").arg(ordner);
        }
        return false;
    }

    // Dieselbe Wache wie eigenstaendig, auf demselben Verzeichnis: laeuft
    // das Contestprogramm schon als eigenes Programm, darf hier kein
    // zweiter Schreiber an dasselbe Log. tryAcquire() holt in dem Fall
    // das fremde Fenster nach vorn.
    auto wache = std::make_unique<Contestprogramm::SingleInstanceGuard>(ordner);
    if (!wache->tryAcquire()) {
        if (grund) {
            *grund = QStringLiteral(
                "The Contestprogramm is already running as its own program; "
                "its window has been brought to the front. Quit it there "
                "first, then open the logbook here.");
        }
        return false;
    }

    auto steuerung = std::make_unique<Contestprogramm::AppController>();
    QString fehler;
    if (!steuerung->openDatabase(Contestprogramm::datenbankPfad(ordner), &fehler)) {
        if (grund) {
            *grund = QStringLiteral("The logbook database could not be opened:\n%1\n%2")
                         .arg(Contestprogramm::datenbankPfad(ordner), fehler);
        }
        wache->release();
        return false;
    }

    // Das Farbthema des Contestprogramms gilt fuer SEINE Widgets; das
    // Stylesheet kommt an das Fenster, nicht an die Anwendung -- Longpath
    // hat sein eigenes.
    Contestprogramm::Style::setActiveTheme(steuerung->settings().colorTheme);
    // Panels als eigene Fenster ohne Systemrahmen, eckig, mit ↙ zurueck
    // -- wie Longpaths eigene Fenster (Betreiber 2026-11-04: "alle panels
    // haben hier noch runde kanten, dies bitte auf eckig umstellen").
    // Eigenstaendig behaelt das Contestprogramm den Systemrahmen.
    Contestprogramm::PanelContainerWidget::setzeRahmenlosSchwebend(elternFenster != nullptr);
    auto* fenster = new Contestprogramm::MainWindow(*steuerung, elternFenster);
    fenster->setAttribute(Qt::WA_DeleteOnClose);
    fenster->setStyleSheet(Contestprogramm::Style::appStyleSheet());
    if (elternFenster) {
        // Dieselbe Ebene wie Longpaths schwebende Panels (PanFloatingWindow,
        // AppletFloatingWindow: Qt::Tool mit dem Hauptfenster als Eltern).
        // Nur so steht das Logbuch nach einem Klick ueber ihnen -- ein
        // gewoehnliches Fenster liegt auf macOS IMMER unter einem NSPanel
        // der schwebenden Ebene, egal wer zuletzt geklickt wurde.
        // Die Knoepfe des Rahmens bleiben: nur der Fenstertyp wechselt.
        fenster->setWindowFlags((fenster->windowFlags() & ~Qt::WindowType_Mask)
                                | Qt::Tool);
        // Ein Werkzeugfenster verschwindet auf macOS, sobald das Programm
        // nicht mehr vorn ist. Fuer ein Logbuch waere das falsch: es soll
        // stehen bleiben, auch wenn der Betreiber kurz im Browser ist.
        fenster->setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    }

    m_innen->wache     = std::move(wache);
    m_innen->steuerung = std::move(steuerung);
    m_innen->fenster   = fenster;

    // Geht das Fenster zu (Schliessknopf, closeEvent sichert das Layout),
    // kommen Controller und Wache hinterher -- aber erst, wenn das Fenster
    // wirklich fort ist; es greift bis zuletzt in den Controller.
    connect(fenster, &QObject::destroyed, this, [this]() {
        m_innen->fenster = nullptr;
        QTimer::singleShot(0, this, [this]() {
            m_innen->steuerung.reset();
            if (m_innen->wache) { m_innen->wache->release(); }
            m_innen->wache.reset();
            emit geschlossen();
        });
    });
    // "Neu starten" aus dem Contestprogramm (Farbthema, Wiederherstellung):
    // eigenstaendig startet das den Prozess neu; hier geht das Fenster zu
    // und wieder auf.
    connect(fenster, &Contestprogramm::MainWindow::restartRequested, this, [this]() {
        schliesse();
        QTimer::singleShot(0, this, [this]() { oeffne(nullptr); });
    });
    connect(m_innen->wache.get(), &Contestprogramm::SingleInstanceGuard::activateRequested,
            this, [this]() { oeffne(nullptr); });
    return true;
}

// ── Die Panels einzeln ──────────────────────────────────────────────

QList<ContestFenster::PanelEintrag> ContestFenster::panels()
{
    static const QHash<QString, QStringList> schlagworte = {
        {QStringLiteral("unifiedlog"),   {QStringLiteral("log"), QStringLiteral("qso"), QStringLiteral("callsign"), QStringLiteral("rufzeichen")}},
        {QStringLiteral("rotorrow"),     {QStringLiteral("rotor"), QStringLiteral("antenne"), QStringLiteral("richtung"), QStringLiteral("azimut")}},
        {QStringLiteral("map"),          {QStringLiteral("karte"), QStringLiteral("map"), QStringLiteral("verbindungen"), QStringLiteral("qsos")}},
        {QStringLiteral("suggestion"),   {QStringLiteral("ziel"), QStringLiteral("naechstes"), QStringLiteral("vorschlag")}},
        {QStringLiteral("ratemeter"),    {QStringLiteral("rate"), QStringLiteral("tempo"), QStringLiteral("qso/h")}},
        {QStringLiteral("checkpartial"), {QStringLiteral("check"), QStringLiteral("partial"), QStringLiteral("dupe")}},
        {QStringLiteral("bandmap"),      {QStringLiteral("bandmap"), QStringLiteral("spots"), QStringLiteral("cluster")}},
        {QStringLiteral("skeds"),        {QStringLiteral("skeds"), QStringLiteral("termine"), QStringLiteral("verabredung")}},
        {QStringLiteral("chat"),         {QStringLiteral("chat"), QStringLiteral("on4kst"), QStringLiteral("kst")}},
        {QStringLiteral("wasserfall"),   {QStringLiteral("wasserfall"), QStringLiteral("waterfall"), QStringLiteral("spektrum")}},
    };
    QList<PanelEintrag> liste;
    for (const Contestprogramm::MainWindow::PanelEintrag& e
         : Contestprogramm::MainWindow::panelEintraege()) {
        liste.append({e.id, e.titel, schlagworte.value(e.id, {e.id})});
    }
    return liste;
}

bool ContestFenster::istPanelSchwebend(const QString& id) const
{
    return m_innen->fenster && m_innen->fenster->istPanelSchwebend(id);
}

bool ContestFenster::zeigePanelAlsFenster(const QString& id, QString* grund,
                                          QWidget* elternFenster)
{
    if (!lade(grund, elternFenster)) { return false; }
    Contestprogramm::MainWindow* log = m_innen->fenster;
    QWidget* panel = log->panelFenster(id);
    if (!panel) {
        if (grund) { *grund = QStringLiteral("Unknown logbook panel: %1").arg(id); }
        return false;
    }
    const bool warSchonFenster = log->istPanelSchwebend(id);
    log->setzePanelSchwebend(id, true);
    if (!warSchonFenster && !log->isVisible() && elternFenster) {
        // Aus einem nie gezeigten Logbuch kommt das Panel ohne Platz:
        // neben das Elternfenster, gestaffelt nach der Zahl der schon
        // schwebenden Panels, damit zwei nicht uebereinander liegen.
        int schwebend = 0;
        for (const PanelEintrag& e : panels()) {
            if (e.id != id && log->istPanelSchwebend(e.id)) { ++schwebend; }
        }
        const QPoint ecke = elternFenster->frameGeometry().topLeft()
                            + QPoint(96 + 40 * schwebend, 120 + 40 * schwebend);
        panel->move(ecke);
    }
    panel->raise();
    panel->activateWindow();
    return true;
}

int ContestFenster::sperreAllePanels(bool an)
{
    if (!m_innen->fenster) { return 0; }
    int n = 0;
    for (const PanelEintrag& e : panels()) {
        auto* panel = qobject_cast<Contestprogramm::PanelContainerWidget*>(
            m_innen->fenster->panelFenster(e.id));
        if (panel && panel->isLocked() != an) {
            panel->setLocked(an);
            ++n;
        }
    }
    return n;
}

void ContestFenster::dockePanel(const QString& id)
{
    if (m_innen->fenster) { m_innen->fenster->setzePanelSchwebend(id, false); }
}

QWidget* ContestFenster::panelFenster(const QString& id) const
{
    return m_innen->fenster ? m_innen->fenster->panelFenster(id) : nullptr;
}

void ContestFenster::schliesse()
{
    if (m_innen->fenster) {
        Contestprogramm::MainWindow* f = m_innen->fenster;
        // Die schwebenden Panels haengen als Geschwister am Longpath-
        // Hauptfenster (2026-11-04), nicht mehr am Logbuch: vor dem
        // Loeschen zurueck in die Flaeche, sonst ueberlebten sie es mit
        // totem Controller.
        for (const PanelEintrag& e : panels()) {
            if (f->istPanelSchwebend(e.id)) { f->setzePanelSchwebend(e.id, false); }
        }
        m_innen->fenster = nullptr;
        // Synchron abbauen, nicht deleteLater: der Controller geht gleich
        // hinterher, und das Fenster darf dann nicht mehr leben.
        f->setAttribute(Qt::WA_DeleteOnClose, false);
        f->close();
        delete f;
    }
    m_innen->steuerung.reset();
    if (m_innen->wache) { m_innen->wache->release(); }
    m_innen->wache.reset();
}

bool ContestFenster::istOffen() const
{
    return !m_innen->fenster.isNull();
}

QWidget* ContestFenster::fenster() const
{
    return m_innen->fenster;
}

} // namespace Longpath
