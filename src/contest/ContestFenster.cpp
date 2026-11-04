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

bool ContestFenster::oeffne(QString* grund)
{
    if (m_innen->fenster) {
        m_innen->fenster->setWindowState(
            (m_innen->fenster->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        m_innen->fenster->show();
        m_innen->fenster->raise();
        m_innen->fenster->activateWindow();
        return true;
    }

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
    auto* fenster = new Contestprogramm::MainWindow(*steuerung);
    fenster->setAttribute(Qt::WA_DeleteOnClose);
    fenster->setStyleSheet(Contestprogramm::Style::appStyleSheet());

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

    fenster->show();
    emit geoeffnet();
    return true;
}

void ContestFenster::schliesse()
{
    if (m_innen->fenster) {
        Contestprogramm::MainWindow* f = m_innen->fenster;
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

QWidget* ContestFenster::fensterForTest() const
{
    return m_innen->fenster;
}

} // namespace Longpath
