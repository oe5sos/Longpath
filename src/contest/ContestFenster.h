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

#include <QList>
#include <QObject>
#include <QVariantMap>
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
    // Wie oeffne(), aber OHNE das Logbuch-Fenster zu zeigen: Controller,
    // Datenbank und Fenster entstehen, das Fenster bleibt verborgen. Fuer
    // die Panels einzeln (unten). Ein zweiter Aufruf tut nichts.
    bool lade(QString* grund = nullptr, QWidget* elternFenster = nullptr);

    // ── Die Panels des Contestprogramms einzeln (2026-11-04) ─────────
    //
    // Betreiber: "jedes panel vom contestprogramm sollte in longpath
    // eigens hinzuzufuegen sein, das waere die beste version." Jedes
    // Panel (Log, Chat, Karte, Bandmap, ...) laesst sich als eigenes
    // Fenster holen, ohne das Logbuch-Hauptfenster zu zeigen. Es ist
    // dann ein Qt::Tool-Kind des (verborgenen) Logbuch-Fensters, das
    // seinerseits am Longpath-Hauptfenster haengt -- also auf der Ebene
    // von Longpaths eigenen Panels. Das ✕ am Panel dockt es zurueck.
    // `schlagworte`: Suchworte fuer den Auswaehler hinter dem "+"
    // (deutsch wie das Logbuch selbst; sie liegen HIER und nicht in
    // src/gui, weil die Oberflaeche von Longpath englisch bleibt und
    // tst_oberflaeche_englisch dort deutsche Texte zaehlt).
    struct PanelEintrag { QString id; QString titel; QStringList schlagworte; };
    // Alle Panels in der Reihenfolge des Contestprogramms -- auch, wenn
    // das Logbuch noch nicht geladen ist.
    static QList<PanelEintrag> panels();
    bool istPanelSchwebend(const QString& id) const;
    // Holt das Panel als eigenes Fenster; laedt das Logbuch bei Bedarf
    // (verborgen). Ein Panel aus einem nie gezeigten Logbuch wird neben
    // das Elternfenster gestellt, gestaffelt.
    // `aktivieren`: das Panel nach vorn und aktiv -- beim Herstellen aus
    // dem Profil NICHT (2026-11-05): dann war beim Start ein Panel vor
    // dem Hauptfenster aktiv, und der Mac zeigte eine leere Menueleiste
    // ("Apple · Longpath"), weil ein Werkzeugfenster keine hat.
    bool zeigePanelAlsFenster(const QString& id, QString* grund = nullptr,
                              QWidget* elternFenster = nullptr, bool aktivieren = true);
    void dockePanel(const QString& id);
    // Alle Panels sperren oder freigeben (ihr eigenes Schloss in der
    // Kopfzeile); liefert, wie viele umgestellt wurden. Fuer "Lock every
    // window in place" in Longpath.
    int sperreAllePanels(bool an);
    QWidget* panelFenster(const QString& id) const;

    // ── Die Anordnung gehoert ins Layoutprofil (2026-11-05) ──────────
    //
    // Betreiber, nach einem Neustart: "so war mein letzter zustand
    // nicht!!!!" -- Logbuch-Fenster offen, Rotoren darin, Chat und Log
    // woanders. Seine Anordnung (Karte, Log, Chat als Fenster, Logbuch
    // zu) stand NIRGENDS: das Contestprogramm sichert Fensterlagen nur
    // bei eigenem Zug, Longpaths Profil kannte die Panels nicht, und ein
    // Desktop-Export des Profils (CONTEST.json) enthielt sie deshalb
    // auch nicht. Jetzt liefert die Bruecke die Anordnung als Werte fuer
    // das Profil und stellt sie daraus wieder her.
    //
    // anordnungDerPanels(): id -> {x, y, w, h} (Rahmen auf dem Schirm)
    // fuer jedes Panel, das gerade ein Fenster ist.
    QVariantMap anordnungDerPanels() const;
    bool logbuchSichtbar() const;
    // Laedt das Logbuch bei Bedarf (verborgen), holt genau die genannten
    // Panels als Fenster an ihre Stelle, dockt alle anderen zurueck und
    // zeigt das Logbuch-Fenster nur, wenn `logbuch` es sagt.
    bool stelleAnordnungHer(const QVariantMap& panels, bool logbuch,
                            QString* grund = nullptr, QWidget* elternFenster = nullptr);
    // ── Ein Panel in einem fremden Gehaeuse (2026-11-05) ─────────────
    //
    // Betreiber: "im combined sollten auch alle contest optionen moeglich
    // sein". Longpaths Combined-Feld nimmt das Panel-Widget selbst auf
    // (Elternwechsel, eigene Kopfzeile). panelFuerFremdesGehaeuse() macht
    // das Panel dafuer bereit (geladen, sichtbar, abgeloest, ohne eigene
    // Knoepfe) und gibt es verborgen zurueck -- das Gehaeuse zeigt es.
    // nimmPanelZurueck() holt es aus dem Gehaeuse (das es vorher
    // freigegeben hat, Elternteil null) in die Flaeche des Logbuchs
    // zurueck und, wenn `dort` gilt, als eigenes Fenster dorthin.
    QWidget* panelFuerFremdesGehaeuse(const QString& id, QString* grund = nullptr,
                                      QWidget* elternFenster = nullptr);
    void nimmPanelZurueck(const QString& id, const QRect& dort);
    bool panelInFremdemGehaeuse(const QString& id) const;
    // Die Panel-Kennung eines beliebigen Widgets, leer fuer alles andere.
    static QString panelIdVon(const QWidget* w);

    // Ein Profil ohne Contest: alle Panels zurueck, Logbuch verborgen.
    // Controller und Datenbank bleiben (CAT, Cluster laufen weiter).
    void raeumeAnordnung();

    // Schliesst das Fenster und raeumt Controller und Datenbank ab.
    void schliesse();

    bool istOffen() const;

    /// Der Startcheck des Logbuchs („bereit?"). Sein Menue gibt es in
    /// Longpath nicht. false, wenn das Logbuch nicht offen ist.
    bool zeigeStartcheck();
    QWidget* startcheckForTest() const;

    // Das offene Logbuch-Fenster oder nullptr -- fuer den Wirt, der es beim
    // Minimieren mit ins Dock nimmt (MainWindow::legeSchwebendeFensterInsDock).
    QWidget* fenster() const;
    QWidget* fensterForTest() const { return fenster(); }

signals:
    void geoeffnet();
    void geschlossen();
    // Ein Panel wurde Fenster oder dockte zurueck, bewegte sich, wurde
    // ausgeblendet, oder das Logbuch-Fenster ging auf oder zu -- der
    // Wirt schreibt die Anordnung dann ins Profil.
    void anordnungGeaendert();
    // Der zweite Sicherungsordner (der Stick) ist nicht erreichbar. Die
    // Statuszeile des Logbuchs, die das eigenstaendig zeigt, ist in
    // Longpath nicht zu sehen; der Wirt zeigt es selbst.
    void sicherungsWarnung(const QString& text);
    // Was das Logbuch in seine Statuszeile schreibt -- ohne das, was dort
    // nur dauernd mitlaeuft (QSO-Zaehler, Frequenzecho, Band, Betriebsart).
    // Eingebettet ist diese Zeile nicht zu sehen; der Wirt zeigt es selbst.
    void logbuchMeldung(const QString& text, bool warnung);

public:
    /// Ob eine Zeile aus der Statuszeile des Logbuchs nach Longpath gehoert,
    /// und ob sie eine Warnung ist. Ohne Fenster pruefbar.
    static bool meldungGehoertNachAussen(const QString& text, bool* warnung = nullptr);

private:
    struct Innen;
    std::unique_ptr<Innen> m_innen;
};

} // namespace Longpath
