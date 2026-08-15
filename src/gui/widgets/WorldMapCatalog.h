#pragma once

// =================================================================
// src/gui/widgets/WorldMapCatalog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Die Weltbilder, die der Betreiber selbst hinlegt.
//
// ── Warum ausserhalb des Quellbaums ──────────────────────────────────
//
// Dieselbe Trennung wie bei den Theme-Dateien: das Bild gehoert dem
// Betreiber, nicht dem Repo. Wer eine Datei in den Ordner legt, bekommt
// sie in der Auswahl angeboten, ohne dass jemand Code aendert und ohne
// dass ein Weltbild von 58 MB in der Versionsgeschichte landet.
//
//     <Konfigurationsverzeichnis>/maps/
//
// Neben themes/, ueber dasselbe AppSettings::resolveConfigDir().
//
// ── Die Beschreibungsdatei ───────────────────────────────────────────
//
// Je Bild eine gleichnamige .json daneben:
//
//     bluemarble.png
//     bluemarble.json
//     { "name": "Blue Marble",
//       "source": "NASA Visible Earth",
//       "attribution": "NASA Earth Observatory",
//       "attributionRequired": true }
//
// Fehlt sie, wird der Dateiname als Name genommen und kein Vermerk
// angezeigt. Ein Bild ohne Beschreibung ist brauchbar, nur unbeschriftet
// — es deshalb gar nicht anzubieten waere strenger als noetig.
//
// ── Warum das Seitenverhaeltnis geprueft wird ────────────────────────
//
// Erwartet wird eine gleichabstaendige Zylinderprojektion, 2:1, Mitte
// bei 0 Grad Laenge — genau das, was FlatMapWidget::project() rechnet:
//
//     x = (lon + 180) / 360 * Breite
//     y = ( 90 - lat) / 180 * Hoehe
//
// Ein Bild mit anderem Verhaeltnis wird ABGELEHNT statt verzerrt
// gezeichnet. Verzerrt saehe die Karte naemlich weiterhin richtig aus,
// und nur die Stationsmarken saessen daneben — ein Fehler, der sich als
// Messfehler tarnt. Genau die Sorte, die dieses Programm schon zweimal
// erst als Bild bemerkt hat.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QSize>
#include <QString>
#include <QVector>

namespace NereusSDR {
namespace WorldMapCatalog {

/// Ein Eintrag der Auswahl. Auch ein abgelehntes Bild bekommt einen —
/// es erscheint ausgegraut MIT Grund, statt unsichtbar zu bleiben. Ein
/// Bild, das der Betreiber hingelegt hat und das dann nirgends auftaucht,
/// liest sich als Fehler des Programms.
struct Entry {
    QString path;            ///< vollstaendiger Pfad zur Bilddatei
    QString name;            ///< aus der Beschreibung, sonst Dateiname
    QString source;          ///< Herkunft, frei
    QString attribution;     ///< Urhebervermerk
    bool    attributionRequired{false};
    QSize   size;            ///< gemessen, nicht behauptet
    bool    usable{false};   ///< Seitenverhaeltnis stimmt
    QString reason;          ///< warum nicht, wenn !usable
};

/// Der Ordner, aus dem gelesen wird. Wird nicht angelegt — ein
/// Verzeichnis, das entsteht, ohne dass jemand danach gefragt hat, ist
/// ein Nebenwirkung; die Auswahl sagt stattdessen, wohin die Dateien
/// gehoeren.
QString directory();

/// Alles im Ordner, in Namensreihenfolge. Wird beim OEFFNEN der Auswahl
/// gerufen, nicht beim Programmstart: wer eine Datei ablegt, waehrend das
/// Programm laeuft, soll sie ohne Neustart sehen.
QVector<Entry> entries();

/// Die Pruefung selbst, als reine Funktion.
///
/// Oeffentlich und ohne Dateizugriff, damit ein Test die BEDINGUNG
/// pruefen kann statt eines Bildes: 2:1, mit einem Pixel Toleranz fuer
/// krumme Kantenlaengen. Die Toleranz ist absichtlich winzig — 2048x1000
/// ist kein 2:1 und soll auffallen.
bool aspectIsUsable(const QSize& size, QString* reasonOut = nullptr);

/// Beschreibung zu einer Bilddatei lesen. Fehlt oder ist sie unlesbar,
/// bleibt der Eintrag bei Vorgabewerten — eine kaputte Beschreibung darf
/// ein brauchbares Bild nicht aus der Auswahl werfen.
Entry describe(const QString& imagePath);

} // namespace WorldMapCatalog
} // namespace NereusSDR
