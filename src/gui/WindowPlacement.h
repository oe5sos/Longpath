#pragma once

// =================================================================
// src/gui/WindowPlacement.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Ein Fenster auf einen sichtbaren Schirm holen ────────────────────
//
// Der Rumpf dieser Funktion stand seit 2026-04-17 als
// FloatingContainer::ensureVisiblePosition() im Baum und löste den Fall
// „der Monitor ist beim Start nicht mehr da" bereits vollständig. Er
// hing dort nur an einer Klasse fest, die ein Meter-Fenster ist — er
// fasst nichts von ihr an ausser zwei Grössenkonstanten.
//
// Am 2026-08-16 wurde er hierher gehoben, damit die Applet-Fenster
// dieselbe Lösung BENUTZEN, statt eine zweite halbe zu bekommen.
// Verhalten unverändert; FloatingContainer ruft jetzt hierher.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Ursprung als FloatingContainer::ensureVisiblePosition,
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   2026-08-16 — Als freie Funktion herausgezogen, Verhalten
//                 unverändert. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
//   2026-09-25 — coveredFraction(): eingeschaltete Applets nach vorne.
//                 Martin Fischer, AI-assisted via Anthropic Claude.
//   2026-09-27 — fitFrameIntoArea(): Werkzeugfenster nie groesser als
//                 der Schirm. Martin Fischer, AI-assisted via Anthropic
//                 Claude.
// =================================================================

#include <functional>

#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>

class QWidget;

namespace Longpath {

// ── Schwebende Fenster aufs Raster ────────────────────────────────────
//
// Betreiber 2026-09-02: "die schwebenden Fenster sollen zueinander in
// einer Linie sein". Alle vier schwebenden Fensterklassen (ToolWindow,
// PanFloatingWindow, AppletFloatingWindow, FloatingContainer) werden
// per NATIVEM macOS-Fenstergriff gezogen, nicht per eigenem Maus-Code
// -- ein direktes Runden in moveEvent() wuerde also gegen das
// Betriebssystem kaempfen (es zieht dorthin, wo die Maus ist, wir
// zoegen sofort zurueck: sichtbares Ruckeln unter dem Cursor). Der
// gedaempfte Weg schnappt erst, wenn die Bewegung kurz zur Ruhe
// gekommen ist -- ein Rueckstoss durch das Runden selbst loest genau
// eine weitere, dann bereits treffende Runde aus, kein Endlosdreh.
//
// Reiner Punkt-Helfer, keine Fensteroperation selbst.
constexpr int kSnapGridPx = 8;
QPoint snappedTopLeft(const QPoint& pos, int grid = kSnapGridPx);

// ── Kanten statt Ecke (2026-09-26) ────────────────────────────────────
//
// Betreiber: "einen minimalen kleinen Raster ueber die ganze Oberflaeche,
// an das jedes Widget andockt ... alle auf gleicher Linie, Hoehe und
// Breite" -- und zum Bestand: "vorhanden, passt aber nicht zu 100 %".
// Gerastert wurde nur die linke obere Ecke; rechte und untere Kante
// lagen, wo die Groesse sie hinstellte. Jetzt rastet jede KANTE fuer
// sich (links, oben, rechts, unten). Getrennt gerundete Lage und Groesse
// wuerden zwei aneinanderliegende Fenster um bis zu ein Raster
// auseinanderreissen; eine gemeinsame Kante rundet so bei beiden gleich.
// Nie kleiner als minSize: dann wandert die rechte/untere Kante ein
// Raster weiter hinaus.
QRect snappedFrameRect(const QRect& frame, const QSize& minSize = QSize(),
                       int grid = kSnapGridPx);

// Von moveEvent() jeder schwebenden Fensterklasse mit einer Zeile
// aufzurufen. Legt bei Bedarf einen einmaligen, an `w` gebundenen
// Debounce-Timer an (kein Member in der aufrufenden Klasse noetig) und
// startet ihn neu; erst wenn er ablaeuft, ohne dass diese Funktion
// zwischenzeitlich erneut aufgerufen wurde, schnappt er `w` aufs
// Raster -- also nach dem Loslassen, nicht waehrend des Ziehens.
//
// Seit 2026-09-26 mit Groesse: der Fensterrahmen (frameGeometry) rastet
// an allen vier Kanten ein, siehe snappedFrameRect(). snapSize = false
// rastet nur die linke obere Ecke (fuer Fenster, deren Breite ein
// anderer festlegt). Maximierte, Vollbild- und minimierte Fenster
// bleiben unberuehrt.
void snapToGridAfterSettle(QWidget* w, int grid = kSnapGridPx,
                           int delayMs = 180, bool snapSize = true);

/// Dafür sorgen, dass `w` auf einem verbundenen Bildschirm zu sehen
/// ist. Verschoben wird, wenn (a) keine Geometrie gespeichert war,
/// (b) die gespeicherte bei (0,0) liegt, (c) sie ausserhalb jedes
/// verbundenen Schirms liegt, oder (d) sie das `anchor`-Fenster in
/// seiner HEUTIGEN Grösse/Lage um weniger als 80×40 Bildpunkte
/// ueberlappt -- LETZTERES NUR, wenn `anchor` bereits sichtbar ist
/// (Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" --
/// eine Position, gespeichert aus einer breiteren/Vollbild-Sitzung,
/// blieb bislang auch dann unangetastet, wenn das Hauptfenster
/// seither viel kleiner geworden ist; ein erster, grosszuegigerer
/// Anlauf ["beruehrt sich ueberhaupt"] liess noch ein Fenster durch,
/// das nur mit einem schmalen Rand an der Hauptfenster-Kante
/// haengenblieb. Zweiter Fund, selber Abend, "vor allem rotor,
/// bandfilter und panadapter sind immer anders als abgespeichert":
/// Rotor/Bandfilter/Panadapter werden alle waehrend MainWindows
/// eigenem Konstruktor wiederhergestellt, lange vor dem ersten
/// show() -- zu diesem Zeitpunkt hat `anchor` seine Vollbild-/
/// Maximiert-Groesse noch nie erreicht, jede Ueberlapp-Pruefung
/// dagegen verwarf die gespeicherte Position also bei JEDEM Start,
/// nicht nur manchmal). `anchor` gibt den Zielschirm vor (in der
/// Regel das Hauptfenster); das Fenster wird auf dessen Schirm
/// zentriert. Liegt die Geometrie schon richtig, geschieht nichts.
///
/// ── Zwei bekannte Schwächen, bewusst stehen gelassen ────────────────
///
/// Beide stammen aus dem Ursprung und werden hier NICHT behoben, weil
/// eine Verhaltensänderung an dieser Stelle jedes freistehende Fenster
/// betrifft. Wer sie angeht, tut es als eigenen Schritt:
///
///  1. `(0,0)` wird als „keine Geometrie" gelesen. Das ist ein gültiger
///     Wert, der als Sonderfall missbraucht wird: auf einem Aufbau mit
///     einem Schirm links oder über dem Hauptschirm ist (0,0) eine
///     ganz normale Position, und ein Fenster, das dorthin gehört,
///     wird bei JEDEM Start zur Mitte gezogen. Richtig wäre ein
///     eigenes Feld „Geometrie vorhanden ja/nein" im gespeicherten
///     Satz, statt einen Wert doppelt zu belegen.
///
///  2. Der Sichtbarkeitstest ist `intersects()` — ein Bildpunkt
///     Überlappung zählt als sichtbar. Ein Fenster, das zu 99 % über
///     die Kante hängt, gilt damit als in Ordnung. Sinnvoll wäre eine
///     Mindestfläche in der Grössenordnung von 80 × 40 px, also
///     genug, um das Fenster mit der Maus wieder hereinziehen zu
///     können.
void ensureOnVisibleScreen(QWidget* w, QWidget* anchor, QSize minSize);

// ── Liegt etwas darueber? ────────────────────────────────────────────
//
// Betreiber 2026-09-25: "wenn ich rx einblende ist es versteckt ... ich
// muss es dann hinter meinen anderen Fenstern suchen". Ein Applet, das
// in die Spalte des Hauptfensters kommt, liegt unter jedem schwebenden
// Werkzeugfenster (Qt::Tool = NSPanel auf der Palettenebene, immer ueber
// normalen Fenstern) -- und bei ihm stehen Panadapter, TX, Rotor/Log
// genau ueber dieser Spalte.
//
// Anteil (0..1) von `target`, den die Rechtecke in `covers` zusammen
// decken. Ueberlappungen untereinander werden nicht doppelt gezaehlt:
// gerechnet wird auf einem 16x16-Punkteraster ueber `target`, genau
// genug fuer "liegt es zu einem guten Teil unter etwas". Leeres
// `target` gilt als ganz verdeckt (es ist nirgends zu sehen).
double coveredFraction(const QRect& target, const QList<QRect>& covers);

// ── Nicht groesser als der Schirm ────────────────────────────────────
//
// Betreiber 2026-09-27, Bild vom MacBook Air: der Kanalzug stand
// 980 x 1000 auf 849 px nutzbarer Hoehe -- sein unterer Teil lag hinter
// dem Dock, und er liess sich nicht kleiner ziehen. Ein Fenster, das
// aufgeht, soll ganz auf die freie Flaeche passen.
//
// Der Rahmen `frame` (mit Titelleiste) so klein wie noetig, damit er in
// `area` (availableGeometry()) passt -- aber nicht kleiner als
// `minFrame` --, und dann so wenig wie moeglich verschoben, bis er ganz
// darin liegt. Passt er schon, kommt er unveraendert zurueck. Ist
// `minFrame` selbst groesser als `area`, gewinnt die Mindestgroesse und
// die linke obere Ecke liegt auf der von `area` (Titelleiste bleibt
// greifbar).
QRect fitFrameIntoArea(const QRect& frame, const QRect& area,
                       const QSize& minFrame = QSize());

/// fitFrameIntoArea() am Fenster `w`, auf der freien Flaeche seines
/// Schirms. Erst nach show() aufrufen: vorher kennt macOS die
/// Titelleiste nicht, und frameGeometry() waere zu klein.
void fitIntoAvailableScreen(QWidget* w);

// ── Der Schutzrahmen (2026-10-14) ────────────────────────────────────
//
// Der Betreiber: „ich muss immer auf Vollformat klicken, um alle Fenster
// und Icons zu sehen" — und gleich darauf der Vorschlag, der die Loesung
// ist: „es sollte vielleicht einen grossen Rahmen geben, ueber den man
// nichts ziehen kann und keine Fenster anordnen kann."
//
// Der Befund dahinter, in seiner Einstellungsdatei nachgelesen: beide
// Layoutprofile tragen `"mainWindow":{"fullScreen":true}`, und die
// schwebenden Felder stehen mit ABSOLUTEN Bildschirmkoordinaten darin
// (BwFilter y=128, pan-0 y=304, Rotor x=1184 h=704). Im Vollbild beginnt
// das Hauptfenster oben; ohne Vollbild rutscht es um die Titelleiste
// nach unten — die schwebenden Fenster bleiben stehen und liegen dann
// ueber der Knopfreihe der Kommandoleiste. Die Ueberschriften (BAND,
// MODE, FILTER …) schauen noch heraus, die Knoepfe darunter sind
// verdeckt.
//
// Geprueft wurde bis dahin nur gegen den BILDSCHIRM
// (`availableGeometry()`, MainWindow.cpp:1935) — nie gegen das
// Hauptfenster. Ein schwebendes Fenster durfte also jederzeit auf der
// eigenen Bedienleiste liegen.
//
// Diese Funktion ist das Gegenstueck: gesperrte Streifen hinein, ein
// Rechteck heraus, das keinen davon mehr beruehrt. Absichtlich eine
// FREIE Funktion ueber uebergebenen Rechtecken und nicht eine Methode an
// MainWindow: so ist sie am Schreibtisch vollstaendig pruefbar (ein
// `MainWindow w;` startet echte UDP-Suche im Netz).
//
// Verschoben wird nur SENKRECHT. Die gesperrten Streifen laufen ueber
// die ganze Fensterbreite — waagerecht gibt es nichts auszuweichen, und
// ein seitliches Ausweichen wuerde eine Anordnung zerreissen, die der
// Betreiber nebeneinander gestellt hat.
//
// `platz` ist die Flaeche, in der das Ergebnis liegen muss (die freie
// Bildschirmflaeche). Passt ein Fenster nirgends mehr hin — zu hoch fuer
// den Rest —, kommt es UNVERAENDERT zurueck: lieber verdeckt als
// halb vom Schirm geschoben.
QRect ausSperrstreifenSchieben(const QRect& wunsch,
                               const QList<QRect>& sperren,
                               const QRect& platz);

// ── „Ich werde gerade gezogen" (2026-10-14) ──────────────────────────
//
// Der Betreiber zum Schutzrahmen: „es sollte vielleicht einen grossen
// Rahmen geben, ueber den man nichts ziehen kann." Ein Rahmen, den man
// erst merkt, wenn das Fenster zurueckspringt, ist eine Ueberraschung;
// einer, den man beim Ziehen SIEHT, ist eine Regel.
//
// Diese zwei Funktionen sind die Bruecke, und absichtlich eine Bruecke
// und kein Ereignisfilter: ein `qApp->installEventFilter` saehe JEDES
// Ereignis des Programms (Zeichnen, Maus, Zeitgeber) -- in einem
// Programm mit Echtzeit-Tonweg ist das kein Preis, den man fuer einen
// Hinweis zahlt. Stattdessen melden sich die drei schwebenden
// Fensterarten selbst, aus ihrem `moveEvent`, wo sie ohnehin schon
// `snapToGridAfterSettle()` rufen.
//
// WindowPlacement kennt MainWindow nicht und soll es nicht kennen --
// darum ein eingehaengter Melder statt eines Aufrufs.
void setzeZugmelder(std::function<void(QWidget*)> melder);

/// Aus dem `moveEvent` eines schwebenden Fensters. Tut nichts, solange
/// niemand einen Melder eingehaengt hat (Prueffaelle, Werkzeugfenster
/// ohne Hauptfenster).
void meldeZug(QWidget* bewegtes);

} // namespace Longpath
