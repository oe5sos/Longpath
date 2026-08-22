// src/gui/StyleConstants.h

// =================================================================
// src/gui/StyleConstants.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

#pragma once
#include <algorithm>

#include <QColor>
#include <QString>
#include <QWidget>

#include "styles/ThemeQss.h"

#include <QFont>

namespace Longpath::Style {

// ── Entblaut, 2026-08-15 ──────────────────────────────────────────────
//
// „nach zeus sieht das aber nicht aus" — OE5SOS, mit einem Screenshot,
// auf dem dreißig Knöpfe einen leuchtend blauen Rahmen tragen: 5K, 4.4K,
// SQL, RIT, XIT, TUNE, MOX, VOX, MON, LEV, EQ, CFC, 2-Tone, PS-A. Bei
// Zeus ist ein inaktiver Knopf fast unsichtbar; sichtbar ist der eine,
// der an ist.
//
// Die Ursache war kBorder = #205070 — ein kräftiges Blau als Rahmen für
// ALLES. Dazu der durchgehende Blaustich in den Flächen: #0f0f1a,
// #0a0a18, #203040, #1a2a3a haben alle mehr Blau als Rot.
//
// Die alten Werte stehen daneben, weil das der Umbauzustand ist: was
// noch als Hex-Literal in einem Widget steht, folgt hier NICHT mit und
// bleibt alt. Diese Ungleichheit ist beabsichtigt und nützlich — sie
// zeigt genau, welche Stelle noch Arbeit braucht. Siehe
// docs/design/HAUSSTIL.md.
constexpr auto kAppBg           = "#08080a";   // war #0f0f1a
constexpr auto kPanelBg         = "#0c0c0e";   // war #0a0a18
// ── Die Textleiter, 2026-08-20 angehoben ─────────────────────────────
//
// Der Betreiber: „generell ist die schrift sehr dunkel. heller waere
// besser. zahlen und zeiger aufhellen."
//
// Angehoben wurde die GANZE Leiter, nicht einzelne Werte. Die Abstaende
// zwischen den Stufen tragen die Bedeutung (Fliesstext vor Nebentext
// vor Skala vor Inaktiv); haette man nur die dunkelste angehoben, waere
// sie ihrer Nachbarin zu nah gekommen und der Unterschied zwischen
// „leise" und „aus" verloren.
//
//   text          #c4c4c9 -> #dcdce1
//   title-text    #a8a8ae -> #c4c4ca
//   secondary     #8e8e93 -> #a6a6ac
//   tertiary      #76767a -> #909096
//   scale         #5c5c60 -> #7e7e85     Skalen und Gradzahlen
//   inactive      #3d3d41 -> #58585e
//   measured      #c2924f -> #d8a55f     Zeiger und Messwerte
//
// Dieselben Werte stehen ein zweites Mal in themes/oe5sos.json: die
// Palette bedient den Malcode, die Themendatei die Stylesheets. Wer
// eine der beiden aendert, muss die andere mitziehen.
constexpr auto kTextPrimary     = "#dcdce1";   // war #c8d8e8, dann #c4c4c9
constexpr auto kTextSecondary   = "#a6a6ac";
constexpr auto kTextTertiary    = "#909096";
// ── Sechs Schriftstufen ─────────────────────────────────────────────
//
// OE5SOS, 2026-08-18: „sechs Stufen, QFont::setLetterSpacing, benannte
// Konstanten, vorher zaehlen was es schon gibt."
//
// GEZAEHLT am 2026-08-18: 641 Vorkommen in 18 verschiedenen Groessen
// (60 x setPixelSize, 581 x font-size im Stylesheet). Die haeufigsten
// sind 11 (232), 10 (186), 9 (69), 12 (72) — vier Stufen, die kaum
// auseinanderliegen und offensichtlich nicht entschieden, sondern
// gewachsen sind.
//
// Die Leiter unten deckt die drei Instrumente ab. Sie ist NICHT auf
// den ganzen Baum angewandt: 641 Stellen blind zu ersetzen waere
// derselbe Fehler wie die Palettenumstellung ohne Abbildungstabelle.
// Wer sie ausweitet, tut es Datei fuer Datei.
//
// Die Zahlen sind ein Vorschlag mit einer Stelle, an der man ihn
// aendert. Herkunft:
//   Caption  9  — HAUSSTIL.md §Die acht Regeln, Regel 1: 8-9 px
//   Small   11  — die haeufigste Groesse im Baum
//   Body    13  — die VFO-Zeile des Frequenz-Widgets
//   Sub     16  — bisher duenn belegt (8 Vorkommen), fuellt die Luecke
//   Reading 22  — der abgelesene Wert in der Instrumenten-Fusszeile
//   Display 38  — die Frequenz selbst
constexpr int kFontCaption = 9;
constexpr int kFontSmall   = 11;
constexpr int kFontBody    = 13;
constexpr int kFontSub     = 16;
constexpr int kFontReading = 22;
constexpr int kFontDisplay = 38;

/// Laufweite der Versalzeile, als Anteil der Schriftgroesse.
/// HAUSSTIL.md §Die acht Regeln: „.18em".
///
/// Sie MUSS ueber QFont::setLetterSpacing gesetzt werden.
/// Qt-Stylesheets kennen kein letter-spacing und verwerfen es
/// kommentarlos — das steht so in der Uebergabe vom 2026-08-15 und hat
/// dort schon einmal eine Stunde gekostet.
constexpr double kCapsTracking = 0.18;

constexpr auto kTextScale       = "#7e7e85";
constexpr auto kTextInactive    = "#58585e";
// NereusSDR-original — used in 5+ places for AGC-T / pan / similar labels;
// sits between kTextSecondary (#8090a0) and kTextScale (#607080).
constexpr auto kLabelMid        = "#828288";
constexpr auto kAccent          = "#4a7ba8";
constexpr auto kTitleText       = "#c4c4ca";

// Borders & Surfaces
//
// kBorder ist der wichtigste Wert hier: er umrandet jeden Knopf im
// Programm. #205070 machte daraus dreißig blaue Rechtecke. #2c2c31 ist
// gerade noch da und tritt nicht mehr nach vorne — der ausgewählte
// Knopf soll auffallen, nicht alle anderen.
constexpr auto kButtonBg        = "#1a1a1e";   // war #1a2a3a
constexpr auto kButtonHover     = "#26262b";   // war #203040
constexpr auto kButtonAltHover  = "#2f3138";   // war #204060
constexpr auto kBorder          = "#2c2c31";   // war #205070
constexpr auto kBorderSubtle    = "#1f1f23";   // war #203040
// Grundflaeche des Panadapters, unter Bild und Raster.
//
// Grau-Blau statt des fast schwarzen #0a0a12: der Betreiber hat am
// 2026-08-20 gesagt, der Standardhintergrund solle genau so aussehen
// wie auf den Logo-Vorschlaegen. Dort steht der Verlauf
// #1a2733 -> #0e151b; dies ist sein Mittelwert als Flaeche.
//
// Eine gespeicherte eigene Farbe (PanadapterBackgroundFill) hat
// Vorrang — dieser Wert ist nur die Vorgabe fuer alle, die keine
// gewaehlt haben.
constexpr auto kPanadapterBg    = "#141e27";
// ── Eigener Wert, nicht derselbe wie der Seitengrund ────────────────
//
// kInsetBg trug bis zum 2026-08-21 EXAKT denselben Wert wie kAppBg
// (#08080a). Das ist nicht nur Geschmack, es macht die Rolle
// unbrauchbar: die Themen-Umsetzung schluesselt ueber den FARBWERT,
// und wer denselben Wert zweimal vergibt, bekommt nur die erste Rolle
// — app-bg steht frueher in der Tabelle. Ein Thema konnte die
// Eingabefelder deshalb nicht anfassen, ohne den ganzen Seitengrund
// mitzunehmen.
//
// Dieselbe Krankheit wie „#203040 war vier Konstanten auf einmal"
// weiter unten in ThemeQss.cpp, nur unbemerkt geblieben, weil beide
// Werte gleich AUSSEHEN.
//
// Gefunden von tst_theme_form_knobs: die Pruefung verlangte, dass ein
// Thema die Feldfarbe aendert, und das ging nicht.
//
// Der neue Wert ist zwei Stufen tiefer als der Seitengrund — was fuer
// eine VERSENKTE Flaeche ohnehin richtig ist: ein Feld liegt im Grund,
// es liegt nicht darauf.
constexpr auto kInsetBg         = "#050507";   // war #08080a (= kAppBg!)
constexpr auto kInsetBorder     = "#232329";   // war #1e2e3e
constexpr auto kGroove          = "#1f1f23";   // war #203040

// Title Bar Gradient
constexpr auto kTitleGradTop    = "#26262b";
constexpr auto kTitleGradMid    = "#1c1c20";
constexpr auto kTitleGradBot    = "#141417";
constexpr auto kTitleBorder     = "#0d0d0f";

// Active/Checked Button States
constexpr auto kGreenBg         = "#1c3a2a";
constexpr auto kGreenText       = "#6fa384";
constexpr auto kGreenBorder     = "#2c5c44";
// ── Der aktive Zustand ───────────────────────────────────────────────
//
// Der Betreiber, 2026-08-21, zu Zeus: „das design hat teilweise mehr
// stil ... ich glaube die farblichen verlaeufe machen es."
//
// Die Verlaeufe waren es nicht — die stehen seit zwei Tagen drin und
// sind nachgemessen. Gemessen wurde stattdessen die SAETTIGUNG des
// aktiven Knopfes: bei uns 65 Prozent, bei Zeus 100. Das alte #254a72
// hatte 39 Prozent Saettigung im Farbwert selbst; es sagte nicht „das
// hier ist an", sondern „das hier ist irgendwie anders".
//
// Der neue Wert ist gesaettigt und liegt im selben Farbton (rund
// 215 Grad), damit nichts anderes in der Palette dagegen steht.
//
// Fuer helle Themen aendert sich dadurch NICHTS: „Kreide" fuehrt
// sel-bg und sel-border selbst (#e0d6ee / #a889cc). Ein gesaettigtes
// Blau auf hellem Grund waere laut — das ist der Grund, warum die
// Rolle dort ueberschrieben wird und nicht hier.
constexpr auto kBlueBg          = "#3576e0";   // war #254a72 (S 39 %)
constexpr auto kBlueText        = "#ffffff";   // war #cfe2f5
constexpr auto kBlueBorder      = "#2a5fbe";   // war #2f5c86
constexpr auto kBlueHover       = "#2d5885";
constexpr auto kAmberBg         = "#33280f";
constexpr auto kAmberText       = "#d8a55f";
// Die gedaempfte Messwertstufe. Sie stand bis 2026-08-17 dreimal als
// nacktes #6b5630 im Baum -- dateilokal in HGauge.cpp:13, noch einmal
// in InstrumentPainter.cpp und als Literal in VfoWidget. Eine Farbe mit
// drei Wohnorten und keinem Namen ist genau die, die beim naechsten
// Feinschliff an zwei Stellen mitgezogen wird und an einer nicht.
constexpr auto kAmberDim        = "#6b5630";
constexpr auto kAmberBorder     = "#6b5426";
constexpr auto kAmberWarn       = "#a8853f";
// ── Abzeichen: Paare aus Grund und Text ─────────────────────────────
//
// OE5SOS, 2026-08-17: „keine errechneten Deckkraftwerte, sondern
// benannte Paare aus Grund und Text — wie danger-bg mit Rot-Text."
//
// StatusBadge legte bis dahin eine Deckkraft von 10-20 % ueber den
// Untergrund: rgba(255,96,96,51) und Geschwister. Zwei Gruende, warum
// das weg ist:
//
//   Eine Deckkraft ueber einem UNBEKANNTEN Grund ergibt je nach
//   Untergrund etwas anderes. Dasselbe Abzeichen sah in der
//   Statuszeile anders aus als im RxDashboard, ohne dass irgendwo
//   stand, welcher Ton herauskommen sollte.
//
//   Ein Paar haelt Grund und Text zusammen. Genau das war beim
//   Abzeichen auseinandergelaufen (siehe StatusBadge::applyStyle).
//
// ── Woher die Werte kommen ──────────────────────────────────────────
//
// Ausgerechnet, nicht geschaetzt: fuer jede Variante der sichtbare
// Abstand ihrer bisherigen Wasche vom Panelgrund (#0c0c0e), gemessen
// in CIE-Lab wie in tools/colour_audit.py, und dann die Deckkraft
// gesucht, die mit der HEUTIGEN Schriftfarbe denselben Abstand ergibt.
//
// Der Unterschied ist noetig, weil die alten Waschen aus den alten
// HELLEN Farben gerechnet waren -- Gold #ffd700 unter einer Schrift,
// die seit dem Hausstil Bernstein #c2924f ist, und Signalrot #ff6060
// unter #c25a5c. Dieselbe Deckkraft mit einer gedaempften Farbe
// verschwindet; derselbe ABSTAND bleibt sichtbar.
//
//   Info  #141c27 -> #161e27   (dE 10,0)
//   On    #14251b -> #212b27   (dE 14,9)
//   Off   #15171b -> #18181a   (dE  5,0)
//   Warn  #29240c -> #372c1d   (dE 19,7)
//   Tx    #3d1d1e -> #3f2224   (dE 20,5)
//
// Eigene Abstufung neben kGreenBg/kAmberBg/kRedBg: die sind die
// Fuellung eines Knopfes (TgxlAdvancedPage:648-664), ein Abzeichen ist
// eine Andeutung. Zwei Staerken, zwei Namensfamilien.
constexpr auto kBadgeInfoBg     = "#161e27";
constexpr auto kBadgeOkBg       = "#212b27";
constexpr auto kBadgeOffBg      = "#18181a";
constexpr auto kBadgeWarnBg     = "#372c1d";
constexpr auto kBadgeTxBg       = "#3f2224";

// Das kraeftigere Sende-Rot. HAUSSTIL.md §Bedeutung: „Sendet / Gefahr
// #a86b6d — nur MOX/TX darf kraeftiger, #c25a5c". Stand als Literal in
// StatusBadge und hatte keinen Namen.
constexpr auto kTxRed           = "#c25a5c";

constexpr auto kRedBg           = "#7a2c2e";
constexpr auto kRedText         = "#f0dcdc";
constexpr auto kRedBorder       = "#a86b6d";

// ── Instrumente ───────────────────────────────────────────────────────
//
// S-Meter, SWR-Zifferblatt, Gain Reduction. Ein Instrument bekommt seine
// Wirkung aus Licht, nicht aus Farbe.
//
// Vorher war der S-Meter-Bogen zur Hälfte blau und zur Hälfte rot, und
// die Teilung kodierte damit nichts — es war eine Skala, die zufällig
// zweifarbig war. Rot bedeutet in diesem Programm „Achtung"; es bedeutete
// oben rechts im Bild dauerhaft „Skalenteil".
//
// Jetzt: ein cremeweißer Bogen über die ganze Länge, darunter ein
// entsättigtes Glimmen, und die Grenze als kurze Marke statt als halber
// Bogen. Die Information bleibt, die Fläche verschwindet.
//
// Der Glimmton ist absichtlich keiner, den man benennen würde. Sobald
// man ihn „olivgrün" nennen kann, ist er zu kräftig.
// ── Heller, auf Wunsch des Betreibers (2026-08-22, +40 %) ───────────
//
// "mir kommt auch vor, dass beim testen die swr und stehellenanzeigen
// mehr bleuctet sind, als dann später in der app" — und nach einem
// Vergleichsblatt mit vier Stufen: "plus 40 % probieren wir mal bei
// swr und stehwelle".
//
// Gerechnet wurde auf der Helligkeit (HSL), nicht multiplikativ auf
// RGB — dieselbe Lehre wie bei den Flaechenverlaeufen: auf dunklem
// Grund tut lighter() fast nichts.
//
// EHRLICH ZUM BOGEN: #c8c8c0 lag schon bei 76 % Helligkeit, +40 %
// laeuft also in reines Weiss. Das ist genau das, was im
// Vergleichsblatt zu sehen war und ausgewaehlt wurde — aber es nimmt
// dem Bogen den warmen Stich. Deshalb steht er hier auf #f2f2ec statt
// #ffffff: praktisch die volle Aufhellung, mit einem Rest Farbe. Wer
// das reine Weiss will, setzt die Zeile auf #ffffff.
//
// Das GLIMMEN traegt den groesseren Teil des Eindrucks; dort greift
// die Aufhellung voll.
constexpr auto kInstrumentFace    = "#f2f2ec";  // Bogen, Zeiger, Teilung
constexpr auto kInstrumentGlowHi  = "#817b5c";  // Mitte des Glimmens
constexpr auto kInstrumentGlowLo  = "#47463b";  // Rand des Glimmens
constexpr auto kInstrumentLimit   = "#a86b6d";  // wo es zu viel wird

// ── Spektrum und Gitter ───────────────────────────────────────────────
//
// Die zwei größten Flächen im Fenster. Solange die Kurve neonzyan und
// die Frequenzskala knallgelb ist, nützt eine dezente Palette drumherum
// nichts — zwei Drittel des Bildes schreien weiter.
//
// Die Kurve gehört zur Akzentfamilie (kAccent #4a7ba8), ist aber eine
// Spur heller: sie ist das, worauf man den ganzen Abend schaut, und
// muss vor dem Rauschteppich stehen, ohne zu leuchten.
//
// Das Gitter kommt ohne Alpha aus dieser Datei — die Deckung setzt der
// Malcode, weil fein und grob dieselbe Farbe bei anderer Deckung sind.
// Eine Farbe hier zu verdoppeln, nur um zwei Alphawerte zu tragen, wäre
// ein Wert zu viel in der Theme-Datei.
//
// Die Bandkante bleibt rot und wird hier NICHT gedämpft: sie sagt, wo
// das Senden aufhört, erlaubt zu sein. Eine Sicherheitsmarke leiser zu
// drehen, weil sie auffällt, hieße das Merkmal abzuschaffen.
// Warm, nicht blau. Der Hausstil trennt nach Aufgabe, nicht nach
// Geschmack: „Blau = anfassbar. Warm = gemessen." Die Spektrumkurve ist
// das Gemessene schlechthin, und sie stand als einziger Messwert im
// Programm auf einem Blauton. HAUSSTIL.md führt beide in einer Zeile:
// „Messwert / Kurve #c2924f".
//
// Die Rolle heißt weiter „trace" und nicht „measured", damit eine
// Theme-Datei die Kurve getrennt vom übrigen Bernstein setzen kann —
// nur ihr Ausgangston ist jetzt der Messwert-Ton.
constexpr auto kSpectrumTrace     = "#c2924f";  // Kurve und Füllung
constexpr auto kSpectrumGrid      = "#8a8f96";  // Gitterlinien (Alpha im Code)
constexpr auto kSpectrumGridText  = "#9aa0a8";  // Frequenz- und dBm-Skala

// Gauge Fill Zones
// ── Telling one equaliser band from another ──────────────────────────
//
// Eight hues so that overlapping bands on one curve can be told apart.
// This is not decoration: with eight filters on top of each other, hue
// is the only thing that says which handle belongs to which curve.
//
// Grey at both ends on purpose — those slots hold the high-pass and
// low-pass slopes, which are shapes rather than bumps and should not
// compete for attention with the shaping bands in the middle.
//
// Four of the eight are NereusSDR's existing accents. Three are new
// (2026-08-11): the palette needs six distinguishable hues between the
// greys and NereusSDR only had four, so a coral, a blue and a violet
// were added, spaced to stay apart on #0a0a18 and chosen to sit with
// the accents rather than beside them.
constexpr auto kEqBand0 = "#8090a0";   // grey — kTextSecondary, HP slot
constexpr auto kEqBand1 = "#ff8850";   // coral, new
constexpr auto kEqBand2 = "#ffb800";   // amber — kAmberText
constexpr auto kEqBand3 = "#00ff88";   // green — kGreenText
constexpr auto kEqBand4 = "#4a7ba8";   // teal  — kAccent
constexpr auto kEqBand5 = "#4a80ff";   // blue, new
constexpr auto kEqBand6 = "#a070e0";   // violet, new
constexpr auto kEqBand7 = "#8090a0";   // grey — kTextSecondary, LP slot

constexpr auto kGaugeNormal     = "#4a7ba8";
constexpr auto kGaugeWarning    = "#a8853f";
constexpr auto kGaugeDanger     = "#a86b6d";
constexpr auto kGaugePeak       = "#c4c4c9";

// Disabled
constexpr auto kDisabledBg      = "#141417";
constexpr auto kDisabledText    = "#4e4e53";
constexpr auto kDisabledBorder  = "#232327";

// Overlay
constexpr auto kOverlayBtnBg    = "rgba(20, 30, 45, 240)";
constexpr auto kOverlayPanelBg  = "rgba(15, 15, 26, 220)";
constexpr auto kOverlayBtnHover = "rgba(0, 112, 192, 180)";
constexpr auto kOverlayBorder   = "#2c2c31";

// Sizes
constexpr int kTitleBarH        = 16;
constexpr int kButtonH          = 22;
constexpr int kOverlayBtnW      = 68;
constexpr int kOverlayBtnH      = 22;
constexpr int kSliderGrooveH    = 4;
constexpr int kSliderHandleW    = 10;
constexpr int kSliderHandleH    = 10;
constexpr int kAppletPanelW     = 260;

// Status Bar
constexpr int kStatusBarH       = 46;
constexpr auto kStatusBarBg     = "#0a0a0c";
constexpr auto kStatusBarBorder = "#1f1f23";
constexpr auto kStatusSep       = "#2c2c31";

// Shared Stylesheet Fragments
// ── Flaechen mit Verlauf statt flacher Fuellung ──────────────────────
//
// Der Betreiber, 2026-08-21, mit einem Bildschirmfoto aus Zeus: „die
// uebergaenge im hintergrund bei den widget wirken sehr gut" — und
// nach dem Entwurfsblatt: „sehe keinen unterschied beim den pdf.
// mache es wie bei zeus."
//
// Der Unterschied ist benennbar: bei Zeus ist keine Flaeche eine
// Farbe, sondern ein Verlauf von oben nach unten, mit einer helleren
// Oberkante. Das Auge liest das als Licht von oben und damit als
// Flaeche mit DICKE — ein Stueck Blech statt eines Rechtecks. Bei uns
// war alles flach.
//
// Zwei Regeln, mehr steckt nicht dahinter:
//
//   AUFGESETZT (Platte, Kopfleiste, Knopf): oben heller, unten dunkler,
//   eine helle Linie an der Oberkante.
//
//   VERSENKT (Mulde, Eingabefeld, Rille): genau ANDERSHERUM — oben
//   dunkel, unten heller. Dasselbe Mittel, umgedreht, und die Flaeche
//   liegt drin statt drauf.
//
// Kein neuer Farbton: alle Stufen kommen aus der Familie, die die
// Palette ohnehin fuehrt. Und alles an EINER Stelle — wer hier etwas
// aendert, aendert das ganze Programm mit.
//
// Ein senkrechter Verlauf in Qt-Stylesheets:
//   qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 …, stop:1 …)
// Die Stufen werden aus der Grundfarbe GERECHNET, nicht getippt.
// Grund: die Themenverwaltung tauscht Farben ueber ihren Hexwert aus
// (ThemeQss). Ein von Hand gesetztes "#22222a" kennt sie nicht — im
// hellen Thema „Kreide" waere der Knopf dunkel geblieben. Also: die
// Grundfarbe durch hexRole() durchs Thema schicken und die helle und
// dunkle Stufe daraus ableiten. Dann stimmt jedes Thema von selbst,
// auch die, die es noch nicht gibt.
// Die Stufen werden AUS der Grundfarbe gerechnet, nicht getippt.
//
// Zwei Gruende. Erstens die Themenverwaltung: sie tauscht Farben ueber
// ihren Hexwert aus (ThemeQss). Ein von Hand gesetztes "#22222a" kennt
// sie nicht — im hellen Thema „Kreide" waere der Knopf dunkel
// geblieben. Also geht die Grundfarbe durch hexRole() und die Stufen
// werden daraus abgeleitet; dann stimmt jedes Thema von selbst, auch
// die, die es noch nicht gibt.
//
// Zweitens die Rechenart. Der erste Versuch nahm QColor::lighter(118),
// und das ist eine MULTIPLIKATION der Helligkeit. Auf unserem fast
// schwarzen Grund (#1a1a1e, Helligkeit 26) sind 18 Prozent davon
// gerade fuenf Stufen — der Testknopf kam auf einen Abstand von 6,
// und genau das war der Fehler, den der Betreiber am Entwurfsblatt
// gesehen hat: „sehe keinen unterschied". Auf Dunkel muss ADDIERT
// werden. shiftL() legt einen festen Betrag drauf, unabhaengig davon,
// wie dunkel der Grund ist.
inline QString shiftL(const QColor& base, int deltaL)
{
    QColor c = base.toHsl();
    const int l = std::clamp(c.lightness() + deltaL, 0, 255);
    c.setHsl(c.hslHue(), c.hslSaturation(), l, c.alpha());
    return c.name();
}

/// AUFGESETZT: oben heller, unten dunkler — Licht von oben, die
/// Flaeche bekommt Dicke. Fuer Platten, Kopfleisten, Knoepfe.
inline QString raisedFill(const char* baseHex, int lift = -1, int drop = -1)
{
    // Tiefe aus dem Thema, sofern es eine Meinung hat. Die Aufrufer
    // duerfen weiter eigene Werte setzen — der aktive Knopf etwa ist
    // absichtlich etwas kraeftiger als eine ruhende Platte.
    if (lift < 0) { lift = formInt("relief", 16); }
    if (drop < 0) { drop = lift * 3 / 4; }
    const QColor base(hexRole(baseHex));
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:0.55 %2, stop:1 %3)")
        .arg(shiftL(base, lift), base.name(), shiftL(base, -drop));
}

/// VERSENKT: genau andersherum — oben dunkel, unten heller. Dasselbe
/// Mittel, umgedreht, und die Flaeche liegt drin statt drauf.
inline QString sunkenFill(const char* baseHex, int deepen = -1, int lift = -1)
{
    if (deepen < 0) { deepen = formInt("mulde", 10); }
    if (lift < 0)   { lift   = deepen * 6 / 5; }
    const QColor base(hexRole(baseHex));
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:1 %2)")
        .arg(shiftL(base, -deepen), shiftL(base, lift));
}

// ── Griffleisten zwischen den Feldern ────────────────────────────────
//
// Der Betreiber, 2026-08-21: „der pandapter muss jetzt endlich mal
// funktionieren, das ist das kernstueck" — und davor: er loese ihn nur
// ab, weil das „der einzige Weg" sei, die Groesse zu aendern.
//
// Es war nie der einzige Weg. Die Splitter waren immer da, ihre Griffe
// aber DREI PIXEL breit (MainWindow.cpp: setHandleWidth(3)), und die
// Splitter im Panadapter-Stapel setzten gar nichts, blieben also auf
// Qts Vorgabe. Ein Ziehgriff von drei Pixeln ist auf einem Retina-
// Schirm mit der Maus nicht zu treffen. Vorhanden und unerreichbar —
// dieselbe Fehlerklasse wie beim Erreichbarkeits-Durchgang, nur in
// Pixeln statt in Signalen.
//
// Sechs Pixel sind greifbar, und ein sichtbarer Strich in der Mitte
// sagt, DASS man ziehen kann. Ohne den weiss es niemand.
constexpr int kSplitterHandlePx = 6;

inline QString splitterStyle()
{
    return QStringLiteral(
        "QSplitter::handle { background: %1; }"
        "QSplitter::handle:horizontal { width: %2px; "
        "  image: none; border-left: 1px solid %3; }"
        "QSplitter::handle:vertical { height: %2px; "
        "  image: none; border-top: 1px solid %3; }"
        "QSplitter::handle:hover { background: %4; }")
        .arg(hexRole(kPanelBg))
        .arg(kSplitterHandlePx)
        .arg(hexRole(kBorder), hexRole(kAccent));
}

inline QString buttonBaseStyle()
{
    return QStringLiteral(
        "QPushButton {"
        // Polsterung 4/10 statt 2/4 und 7 statt 6 Punkt Eckenradius.
        // Klingt nach nichts und ist der halbe Eindruck von Ruhe: die
        // Knopfreihen bei Zeus wirken nicht ruhiger, weil dort weniger
        // steht, sondern weil um jedes Ding mehr Luft ist.
        "  background: %1; border: 1px solid %2; border-radius: %6px;"
        "  color: %3; font-size: 11px; font-weight: bold;"
        "  padding: %7px %8px;"
        "}"
        "QPushButton:hover { background: %4; }"
        // Gedrueckt: der Verlauf kippt. Ein Knopf, der sich beim
        // Druecken nicht bewegt, fuehlt sich tot an — und das kostet
        // hier kein einziges Pixel Verschiebung.
        "QPushButton:pressed { background: %5; }"
    ).arg(raisedFill(kButtonBg),
          QLatin1String(kBorder),
          QLatin1String(kTextPrimary),
          raisedFill(kButtonAltHover, 20, 10),
          sunkenFill(kButtonBg))
     .arg(formInt("radius", 7))
     .arg(formInt("luft-v", 4))
     .arg(formInt("luft-h", 10));
}

inline QString greenCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kGreenBg, kGreenText, kGreenBorder);
}

inline QString blueCheckedStyle()
{
    // raisedFill statt einer flachen Fuellung: der aktive Knopf ist
    // dieselbe aufgesetzte Flaeche wie jeder andere, nur in der
    // Akzentfarbe. Zwei verschiedene Bauarten fuer denselben Knopf
    // waeren genau die Art Unstimmigkeit, die man als „unfertig" liest.
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2;"
        " border: 1px solid %3; }"
    ).arg(raisedFill(kBlueBg, 18, 14),
          QLatin1String(kBlueText),
          QLatin1String(kBlueBorder));
}

inline QString amberCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kAmberBg, kAmberText, kAmberBorder);
}

inline QString redCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kRedBg, kRedText, kRedBorder);
}

// DSP toggle — brighter green than the kGreenBg action buttons.
// Used by VfoWidget DSP tab and SpectrumOverlayPanel DSP toggles.
// Source: NereusSDR-original. Distinct semantic from action-button
// "checked" state because DSP toggles communicate "feature on" not
// "action engaged."
constexpr auto kDspToggleBg     = "#1b3527";
constexpr auto kDspToggleBorder = "#33684c";
constexpr auto kDspToggleText   = "#7fae91";

inline QString dspToggleStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kDspToggleBg, kDspToggleText, kDspToggleBorder);
}

inline QString sliderHStyle()
{
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: %1; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 10px; height: 10px; margin: -3px 0;"
        "  background: %2; border-radius: 5px;"
        "}"
    ).arg(kGroove, kAccent);
}

inline QString sliderVStyle()
{
    return QStringLiteral(
        "QSlider::groove:vertical {"
        "  width: 4px; background: %1; border-radius: 2px;"
        "}"
        "QSlider::handle:vertical {"
        "  height: 10px; width: 16px; margin: 0 -6px;"
        "  background: %2; border-radius: 5px;"
        "}"
    ).arg(kGroove, kAccent);
}

inline QString insetValueStyle()
{
    // VERSENKT: oben dunkel, unten heller — siehe die Notiz bei
    // buttonBaseStyle. Dasselbe Mittel wie bei der Platte, nur
    // umgedreht, und das Feld liegt drin statt drauf.
    return QStringLiteral(
        "QLabel {"
        "  font-size: 11px; background: %1; border: 1px solid %2;"
        "  border-radius: 6px; padding: 1px 2px; color: %3;"
        "}"
    ).arg(sunkenFill(kInsetBg),
          QLatin1String(kInsetBorder),
          QLatin1String(kTextPrimary));
}

inline QString titleBarStyle()
{
    // Die Kopfleiste soll AUFLIEGEN, nicht eingelassen sein: etwas
    // kraeftiger als die Platte darunter, und eine dunkle Unterkante,
    // die sie von ihr abhebt. Die Toene stehen weiter in der Palette
    // (kTitleGrad*) — hier ist nur der Schluss dunkler geworden.
    return QStringLiteral(
        "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 %1, stop:0.5 %2, stop:1 %3);"
        " border-bottom: 1px solid %4;"
    ).arg(kTitleGradTop, kTitleGradMid, kTitleGradBot, kTitleBorder);
}

// ── Setup Dialog Page Stylesheets ─────────────────────────────────────────────
// Flat string constants used by SetupPage subclasses that build their own
// layouts directly (CatNetwork, Keyboard, Diagnostics pages).

constexpr auto kPageStyle =
    "QWidget { background: #0f0f1a; color: #c8d8e8; }";

constexpr auto kGroupBoxStyle =
    "QGroupBox { border: 1px solid #304050; border-radius: 6px;"
    " margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

constexpr auto kSecondaryLabelStyle =
    "QLabel { color: #8090a0; font-size: 11px; }";

constexpr auto kComboStyle =
    "QComboBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 6px; color: #c8d8e8; font-size: 13px; padding: 2px 4px; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
    " selection-background-color: #4a7ba8; }";

constexpr auto kCheckBoxStyle =
    "QCheckBox { color: #c8d8e8; font-size: 13px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
    " border: 1px solid #304050; border-radius: 2px; }"
    "QCheckBox::indicator:checked { background: #4a7ba8; border-color: #4a7ba8; }";

constexpr auto kRadioButtonStyle =
    "QRadioButton { color: #c8d8e8; font-size: 13px; }"
    "QRadioButton::indicator { width: 14px; height: 14px; background: #08080a;"
    " border: 1px solid #2c2c31; border-radius: 7px; }"
    "QRadioButton::indicator:checked { background: #4a7ba8; border-color: #2f5c86; }";

// ── Eingabefelder sind Mulden, keine Flaechen ────────────────────────
//
// Gemeldet 2026-08-16 auf Startup & Preferences: Callsign, Grid Square,
// Process Priority und FFTW Wisdom waren "fast unsichtbar -- man sieht
// nicht mehr, dass das bedienbare Felder sind".
//
// Ursache war nicht zu viel Daempfung, sondern die falsche RICHTUNG.
// Die Werte #1a2a3a (Grund) und #304050 (Rand) wurden von themed()
// korrekt auf #1a1a1e und #2c2c31 gezogen -- zwei fast gleiche
// Grautoene. Vorher trennte der Rand durch ein kraeftiges Blau von
// selbst; ohne das liegt ein Feld auf dem Panel wie eine Flaeche.
//
// HAUSSTIL beschreibt es als VERSENKT: "Versenkt (Glas) #000000 +
// inset 0 2px 8px #000". Ein Feld gehoert DUNKLER als seine Umgebung,
// nicht heller -- dann traegt der hellere Rand von selbst.
//
// Qt-Stylesheets koennen keinen inneren Schatten. Der Grund liegt
// deshalb auf kInsetBg (#08080a, dunkler als das Panel #0c0c0e) und der
// Rand auf kBorder (#2c2c31), der gegen fast Schwarz deutlich steht.
//
// Ein leeres Feld ist nicht inaktiv. Es wartet auf eine Eingabe.
// ── Eingabefelder sind VERSENKT ──────────────────────────────────────
//
// Der Betreiber, 2026-08-21, auf ein Bildschirmfoto zeigend: „hier
// siehst du gut die uebergaenge: filter, customs. im feld von 150 bis
// 2850." Gemeint sind die Eingabefelder und Rillen — weiche
// Verlaeufe, die eine Flaeche als eingelassen lesen lassen.
//
// Bei uns waren genau die flach: „background: #08080a", fest
// eingetippt. Und schlimmer — dieser Wert stand in KEINER
// Themen-Tabelle, die Felder folgten also gar keinem Thema. Im hellen
// Thema „Kreide" blieben sie schwarz.
//
// Jetzt sind es Funktionen: sunkenFill() ueber die Themenfarbe, Tiefe
// und Eckenradius aus den Form-Reglern. Die alten Konstanten sind
// ENTFERNT, nicht danebengestellt — sonst haette der Uebersetzer die
// vergessenen Aufrufstellen nicht gefunden, und die Haelfte der
// Felder waere flach geblieben.
inline QString lineEditStyle()
{
    return QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        " border-radius: %3px; color: %4; font-size: 13px;"
        " padding: 2px 4px; }")
        .arg(sunkenFill(kInsetBg),
             hexRole(kInsetBorder))
        .arg(formInt("radius", 7))
        .arg(hexRole(kTextPrimary));
}

inline QString spinBoxStyle()
{
    return QStringLiteral(
        "QSpinBox { background: %1; border: 1px solid %2;"
        " border-radius: %3px; color: %4; font-size: 13px;"
        " padding: 2px 4px; }")
        .arg(sunkenFill(kInsetBg),
             hexRole(kInsetBorder))
        .arg(formInt("radius", 7))
        .arg(hexRole(kTextPrimary));
}

inline QString doubleSpinBoxStyle()
{
    return QStringLiteral(
        "QDoubleSpinBox { background: %1; border: 1px solid %2;"
        " border-radius: %3px; color: %4; font-size: 13px;"
        " padding: 2px 4px; }")
        .arg(sunkenFill(kInsetBg),
             hexRole(kInsetBorder))
        .arg(formInt("radius", 7))
        .arg(hexRole(kTextPrimary));
}

constexpr auto kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #4a7ba8; width: 12px;"
    " height: 12px; border-radius: 6px; margin: -4px 0; }";

constexpr auto kButtonStyle =
    "QPushButton { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 6px; color: #c8d8e8; font-size: 13px; padding: 3px 10px; }"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:pressed { background: #4a7ba8; color: #0f0f1a; }";

// Apply the canonical "dark page" stylesheet to a Setup page that lays
// itself out manually (i.e. doesn't inherit the SetupPage::addLabeledX
// helper-based widgets). Replaces the 4 byte-for-byte copies of
// applyDarkStyle() that previously lived in TransmitSetupPages,
// DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage.
//
// Per docs/architecture/ui-audit-polish-plan.md §A3.
//
// Two intentional drift corrections vs the local copies:
// - QGroupBox border uses kBorder (#205070), not the locals' #203040.
// - QGroupBox padding-top is 12 px, not the locals' 4 px (an 8 px
//   header-position shift, matching the rest of the app).
// All other rules match the locals byte-for-byte so callers see no
// other visual change.
//
// Issue #175 follow-up (2026-05-04): added QRadioButton rules.  Without
// these, calling applyDarkPageStyle() on a SetupPage subclass overwrote
// the QRadioButton::indicator style installed in SetupPage::init() via
// kRadioButtonStyle, leaving radio-button text + indicators in system
// default (black-on-black on dark theme).  Caught on PowerPage's new
// "Tune" group radios.  Mirrors kRadioButtonStyle (lines 241-245 above).
inline void applyDarkPageStyle(QWidget* w)
{
    if (!w) { return; }
    w->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QGroupBox { color: %7; font-size: 11px;"
        "  border: 1px solid %3; border-radius: 6px;"
        "  margin-top: 8px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: %2; }"
        "QComboBox { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %4; color: %2;"
        "  selection-background-color: %5; }"
        "QSlider::groove:horizontal { background: %4; height: 4px;"
        "  border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %5; width: 12px;"
        "  margin: -4px 0; border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }"
        "QSpinBox, QDoubleSpinBox { background: %4; color: %2;"
        "  border: 1px solid %3; border-radius: 6px; padding: 1px 4px; }"
        "QCheckBox { color: %2; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: %4;"
        "  border: 1px solid %3; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: %5; border-color: %5; }"
        "QRadioButton { color: %2; }"
        "QRadioButton::indicator { width: 14px; height: 14px; background: %4;"
        "  border: 1px solid %3; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background: %5; border-color: %5; }"
        "QLineEdit { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 2px 6px; }"
        "QPushButton { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 3px 12px; }"
        "QPushButton:hover { background: %6; }"
        "QPushButton:pressed { background: %5; color: %1; }"
    ).arg(kAppBg, kTextPrimary, kBorder, kButtonBg, kAccent, kButtonHover, kTextSecondary));
}

// ── TX / RX filter overlay palette ────────────────────────────────────────────
// Plan 4 D9 (Cluster E).  Used by SpectrumWidget::drawTxFilterOverlay() and
// SpectrumWidget::drawTxFilterWaterfallColumn().
// Colours are NereusSDR-original; no Thetis upstream equivalent (Thetis uses
// hard-coded GDI+ brushes without named constants).

// TX filter overlay — translucent orange.
// Customisable via ColorSwatchButton on Setup → Display → TX Display per
// docs/architecture/ui-audit-polish-plan.md §E1.D9b.
constexpr auto kTxFilterOverlayFill   = "rgba(255, 120, 60, 46)";
constexpr auto kTxFilterOverlayBorder = "#a8724a";
constexpr auto kTxFilterOverlayLabel  = "#c2924f";

// RX filter overlay — translucent cyan.
// Border reuses kAccent (#00b4d8); only the fill variant is new.
// Customisable on Setup → Display → Spectrum Defaults.
constexpr auto kRxFilterOverlayFill   = "rgba(0, 180, 216, 80)";


/// Eine Versalzeile: Groesse, Halbfett, Grossbuchstaben und die
/// Laufweite aus kCapsTracking — an einer Stelle, damit sie nicht je
/// Aufrufstelle neu gerechnet wird und dabei auseinanderlaeuft.
inline QFont capsFont(const QFont& base, int px = kFontCaption)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::DemiBold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, px * kCapsTracking);
    return f;
}

/// Eine Zahl, die sich aendert: Monospace, damit Stellen untereinander
/// stehen. HAUSSTIL.md §Die acht Regeln, Regel 2.
inline QFont monoFont(const QFont& base, int px, QFont::Weight w = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(w);
    f.setFamily(QStringLiteral("Menlo"));
    return f;
}

} // namespace Longpath::Style
