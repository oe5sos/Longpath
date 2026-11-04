// =================================================================
// src/contest/ContestFenster.h  (Longpath)
// =================================================================
//
// Longpath-original (2026-11-04). Das Contestprogramm als Fenster in
// Longpath -- derselbe Prozess, dasselbe Log wie eigenstaendig.
//
// Betreiber am 2026-11-03: "vielleicht sollte man das alles in longpath
// automatisch als option zusaetzlich einbauen, sodass man nicht primaer
// beide programme offen haben muss. ... es soll aber trotzdem alles auch
// einzeln funktionieren."
//
// Was hier NICHT passiert: kein zweiter Log-Kern in Longpath, keine
// Kopie des Contestprogramms. Die Bibliothek ContestprogrammCore (Submodul
// third_party/contestprogramm) wird gelinkt, und dieses Stueck tut, was
// dort main.cpp tut -- Datenverzeichnis, Einzelplatz-Wache, Controller,
// Datenbank, Fenster, Stylesheet -- nur im Prozess von Longpath. CAT und
// Wasserfall laufen weiter ueber localhost (rigctld 4532, TCI 50001), wie
// eigenstaendig; darum funktioniert jedes Programm auch allein.
//
// Diese Kopfdatei zieht KEINE Kopfdatei des Contestprogramms herein:
// beide Baeume haben ein src/core mit gleichnamigen Dateien
// (Maidenhead.h, RigctldClient.h, ...). Alles Fremde bleibt in der .cpp,
// die als eigene Bibliothek (LongpathContestBruecke) mit den
// Include-Pfaden des Contestprogramms uebersetzt wird.
// =================================================================
#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QWidget;

namespace Longpath {

class ContestFenster : public QObject {
    Q_OBJECT
public:
    explicit ContestFenster(QObject* parent = nullptr);
    ~ContestFenster() override;

    // Wo das Log liegt -- dasselbe Verzeichnis wie eigenstaendig
    // (CONTESTPROGRAMM_DATA_DIR geht vor).
    static QString datenverzeichnis();

    // Oeffnet das Fenster; beim ersten Mal entstehen Controller und
    // Datenbank. false mit `grund`, wenn das Contestprogramm bereits
    // eigenstaendig laeuft (dessen Fenster wird dann nach vorn geholt)
    // oder die Datenbank nicht aufgeht. Ein zweiter Aufruf holt das
    // offene Fenster nach vorn.
    //
    // `elternFenster` (2026-11-04): Longpaths Hauptfenster. Damit wird das
    // Logbuch ein Fenster DERSELBEN Ebene wie Longpaths schwebende Panels
    // (Qt::Tool, auf macOS ein NSPanel auf der schwebenden Ebene). Ohne das
    // lag es als gewoehnliches Fenster immer UNTER den Panels -- Betreiber
    // 2026-11-04: "wenn ich contestprogramm oeffne, soll das nach anklicken
    // im vordergrund bleiben!" Auf einer Ebene gilt die Klickreihenfolge.
    // Ohne Elternfenster (Pruefstand) bleibt es ein gewoehnliches Fenster.
    bool oeffne(QString* grund = nullptr, QWidget* elternFenster = nullptr);

    // Schliesst das Fenster und raeumt Controller und Datenbank ab.
    void schliesse();

    bool istOffen() const;

    // Das offene Logbuch-Fenster oder nullptr -- fuer den Wirt, der es beim
    // Minimieren mit ins Dock nimmt (MainWindow::legeSchwebendeFensterInsDock).
    QWidget* fenster() const;
    QWidget* fensterForTest() const { return fenster(); }

signals:
    void geoeffnet();
    void geschlossen();

private:
    struct Innen;
    std::unique_ptr<Innen> m_innen;
};

} // namespace Longpath
