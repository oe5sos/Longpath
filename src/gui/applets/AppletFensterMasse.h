#pragma once
// =================================================================
// src/gui/applets/AppletFensterMasse.h  (Longpath)
// =================================================================
//
// Die zuletzt von Hand gewaehlte Groesse eines abgeloesten Applets.
//
// ── Warum neben dem Profil ──────────────────────────────────────────
//
// Die Fenstergeometrie steht schon im Layoutprofil, und beim Abloesen
// gilt sie auch (`MainWindow::detachApplet`, der Zweig
// `rect.isValid()`). Nur greift sie erst, wenn im Profil ueberhaupt
// etwas steht -- und das ist beim ERSTEN Abloesen eines Applets nie
// der Fall. Dann rechnet `applyDefaultSize()` aus dem `sizeHint()`
// eine Groesse, und bei einem Applet mit schmalem Hinweis ist die
// winzig.
//
// Betreiber am 2026-10-30: „ich wuerde gerne, wenn ich ft8 klicke,
// dass fenster groesser haben. besser waere vielleicht die groesse
// der letzen veraenderung zu speichern?"
//
// Darum diese zweite, einfachere Ablage: nur Breite und Hoehe, je
// Panelkennung, unabhaengig vom Profil. Sie ueberlebt den
// Profilwechsel -- wer ein Applet einmal auf eine brauchbare Groesse
// gezogen hat, will sie nicht in jedem Profil neu ziehen.
//
// Die LAGE bleibt dem Profil. Sie haengt am Bildschirm und am
// Schutzrahmen; eine zweite Stelle, die darueber mitredet, waere
// genau die Art Dopplung, die auseinanderlaeuft.
//
// ── Warum freie Funktionen ueber AppSettings ────────────────────────
//
// Damit ein Prueflauf sie ohne Fenster fahren kann. Dieselbe
// Ueberlegung wie bei `AppletKeys`: eine Regel, die nur mit einem
// MainWindow zu pruefen ist, wird nicht geprueft.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QSize>
#include <QString>

namespace Longpath::AppletFensterMasse {

/// Kleiner als das nimmt niemand freiwillig; was darunter liegt, ist
/// ein Unfall beim Ziehen und wird nicht gemerkt.
inline constexpr int kMinBreite = 200;
inline constexpr int kMinHoehe  = 120;

/// Die gemerkte Groesse, oder eine ungueltige QSize.
QSize gemerkt(const QString& panelId);

/// Merken. Zu kleine Masze werden NICHT abgelegt -- sonst merkt sich
/// das Fenster den Augenblick, in dem es beim Andocken auf null
/// zusammenfaellt.
void merke(const QString& panelId, const QSize& groesse);

/// Vergessen (fuer „Groesse zuruecksetzen").
void vergiss(const QString& panelId);

}  // namespace Longpath::AppletFensterMasse
