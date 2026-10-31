#pragma once

// =================================================================
// src/core/ProfilHerkunft.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Was fuer eine Profildatei liegt da? Eine Schicht VOR den
// eigentlichen Lesern, die nichts anwendet und nichts aendert — sie
// sagt nur, woher die Datei kommt und wohin sie in Longpath gehoert.
//
// Warum es das gibt: Longpath hatte am 2026-10-31 zwei Importeure an
// zwei Orten mit zwei Zielen, und keiner von beiden konnte die Datei
// des anderen auch nur BENENNEN.
//
//   „Aether-Profil…"  im Kanalzugfenster  -> StripChain
//   „Import Thetis…"  in den TX-Profilen  -> MicProfileManager
//
// Wer seine Thetis-Ausfuhr in den Kanalzug zog, bekam „Das ist kein
// AetherSDR-Kanalzugprofil (Feld „schema" lautet „—")". Richtig, aber
// nutzlos: es stand nicht da, dass die Datei sehr wohl gelesen werden
// kann, nur zwei Fenster weiter.
//
// Das Vorbild ist Zeus, bei dem es ein Knopf ist und die Herkunft am
// INHALT erkannt wird — „Zeus recognizes the Thetis file on its own,
// even if it has been renamed." Dieselbe Regel hier: nie an der
// Endung. Eine umbenannte Datei ist derselbe Inhalt.
//
// Was dieser Erkenner NICHT tut: raten. Er kennt genau die Formate,
// die in Longpath wirklich gelesen werden koennen, und bei allem
// anderen sagt er „kenne ich nicht" und zaehlt auf, was geht. Eine
// Herkunft, die hier steht, ist eine, zu der es einen Leser gibt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QString>

namespace Longpath {

namespace ProfilHerkunft {

/// Woher eine Datei kommt. Nur Arten, zu denen es einen Leser gibt —
/// siehe den Kopf.
enum class Art {
    Unbekannt,
    /// AetherSDRs Kanalzugprofil, JSON mit `"schema":
    /// "AetherSDR ChannelStripPreset v1"`. Ziel: die Kette.
    AetherKanalzug,
    /// Thetis-Einzelausfuhr: `<Data>` mit NUR der TXProfile-Tabelle.
    ThetisProfilausfuhr,
    /// Thetis-Sicherung: `<Data>` mit allen Tabellen.
    ThetisDatenbank,
};

struct Befund {
    Art art{Art::Unbekannt};
    /// Was es ist, in einem Satz fuer den Betreiber.
    QString was;
    /// Wohin es in Longpath gehoert, in Worten („der Kanalzug", „die
    /// TX-Profile"). Leer bei Unbekannt.
    QString wohin;
    /// Gesetzt bei Unbekannt: warum nicht, und was stattdessen geht.
    QString warumNicht;
};

/// Die Datei ansehen. Liest hoechstens den Anfang, aendert nichts und
/// kennt weder AppSettings noch Oberflaeche.
Befund erkenne(const QByteArray& inhalt);

/// Die Filterzeile fuer den Dateidialog — EINE Zeile fuer alles, was
/// gelesen werden kann, plus „alle Dateien". Hier, damit beide Knoepfe
/// dieselbe haben und keiner sie beim naechsten Format vergisst.
QString dateiFilter();

} // namespace ProfilHerkunft
} // namespace Longpath
