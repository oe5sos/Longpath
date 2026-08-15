#pragma once

// =================================================================
// src/gui/widgets/WorldTexture.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// One decoded copy of the operator's world image, shared by everything
// that draws the Earth.
//
// This exists for a concrete reason rather than tidiness: the large
// Blue Marble is 5400 x 2700, which is 58 MB once decoded to RGB32.
// The globe, the flat map and the QSO map would otherwise hold three
// copies — 175 MB of the same picture. QImage is implicitly shared, so
// handing out copies of one cached instance costs nothing.
//
// It also puts the settings key in one place. Two widgets each spelling
// out "GlobeWorldImagePath" is a typo away from one of them silently
// never finding the image.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QImage>
#include <QObject>
#include <QString>

namespace NereusSDR {
namespace WorldTexture {

// Settings key holding the path the operator chose or downloaded.
QString settingsKey();

// That path, or empty if none has been chosen.
QString currentPath();

// The decoded image, or a null QImage. Cached; re-decoded only when the
// path changes or reload() is called. Always Format_RGB32, converted
// once here, because sampling a non-native format per pixel costs more
// than the whole render.
QImage image();

/// Der Urhebervermerk des GEWAEHLTEN Bildes, wenn seine Beschreibung
/// einen verlangt -- sonst leer. Hier und nicht im Katalog, weil die
/// zeichnenden Widgets nur das aktuelle Bild kennen und nicht den
/// Ordner: sonst muesste jede Ansicht den Katalog durchsuchen, um zu
/// wissen, was sie gerade malt.
QString requiredAttribution();

// Point at a new file. Returns false and keeps the previous image if it
// cannot be read — a bad choice should not blank a working map.
bool setPath(const QString& path);

// Drop the cache, e.g. after the file on disk was replaced.
void reload();

// ── Wer erfaehrt von einem Bildwechsel ──────────────────────────────
//
// Bis 2026-08-15 niemand: GlobeWidget rief setPath() und zeichnete sich
// selbst neu, FlatMapWidget erfuhr davon nichts und zeigte das alte Bild
// bis zum naechsten Neuzeichnen aus anderem Grund.
//
// Der naheliegende Weg waere gewesen, beide Stellen nacheinander
// aufzurufen. Entscheidung des Betreibers: Signalgeber statt
// Doppelaufruf -- "ein Aufruf, den jemand spaeter vergisst, ist genau
// der Fehlertyp von heute Nachmittag". Wer kuenftig eine dritte Ansicht
// baut, verbindet sich hier und muss von den anderen beiden nichts
// wissen.
class Notifier : public QObject {
    Q_OBJECT
public:
    /// Der eine Geber. Lebt so lange wie das Programm.
    static Notifier& instance();
signals:
    /// Das Weltbild hat gewechselt -- neu zeichnen. Wird von setPath()
    /// und reload() ausgeloest, auch wenn der Pfad derselbe blieb: die
    /// Datei dahinter kann sich geaendert haben.
    void changed();
private:
    using QObject::QObject;
};

} // namespace WorldTexture
} // namespace NereusSDR
