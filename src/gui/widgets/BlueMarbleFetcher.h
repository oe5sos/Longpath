// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/BlueMarbleFetcher.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Die Kugel soll nicht schematisch bleiben, bis jemand „World image…"
// findet und einen Download anstoesst („dieses bild als weltkugel ist
// haesslich", Betreiber 2026-09-22). Ohne eigenes Weltbild setzt sich
// die Kugel ihr Blue Marble selbst zusammen: die Kacheln der Stufe 3
// von NASA GIBS (dieselben, die die flache Karte zeichnet, derselbe
// Plattencache), 10 × 5 Kacheln zu 512 px, zu einem 5120 × 2560 grossen
// Bild in Plattkarte gefuegt, als JPEG in den Kartenordner gelegt —
// mit Sidecar fuer Name und Vermerk, wie jedes andere Weltbild dort —
// und als Weltbild eingetragen. Einmal; beim naechsten Start liegt die
// Datei schon da.
//
// Hat der Betreiber selbst ein Bild gewaehlt, passiert nichts.
// =================================================================
// Modification history (Longpath):
//   2026-09-22 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include "gui/widgets/GibsTileLayer.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace Longpath {

class BlueMarbleFetcher : public QObject {
    Q_OBJECT
public:
    explicit BlueMarbleFetcher(QObject* parent = nullptr);

    static constexpr int kLevel = 3;   // 36° je Kachel → 5120 × 2560

    /// Dateiname im Kartenordner (WorldMapCatalog::directory()).
    static QString fileName();
    static QString targetPath();

    /// Startet den Zusammenbau, wenn kein Weltbild gewaehlt ist und die
    /// Datei nicht schon liegt (dann wird sie nur eingetragen). Ein
    /// zweiter Aufruf waehrend eines laufenden tut nichts. Einmal je
    /// Programmlauf sinnvoll — der Aufrufer merkt sich das nicht, das
    /// tut ensureDefault().
    void start();
    bool isRunning() const { return m_running; }

    /// Bequem fuer die Kugeln: ein Abholer fuer das ganze Programm,
    /// angelegt beim ersten Ruf. Tut nichts, wenn schon ein Weltbild
    /// eingetragen ist.
    static void ensureDefault();

    /// Fuer Pruefstaende: eigener Kachelspeicher (Netz aus, putForTest)
    /// und eigener Zielpfad.
    void setTileLayerForTest(GibsTileLayer* layer) { m_tiles = layer; }
    void setTargetPathForTest(const QString& path) { m_targetOverride = path; }

    /// Die Bildkomposition, rein: alle Kacheln der Stufe in die Plattkarte.
    static QImage compose(GibsTileLayer& tiles, int level, bool* complete);

signals:
    void finished(bool ok, const QString& path);

private:
    void onTileReady();
    QString target() const;

    GibsTileLayer* m_tiles{nullptr};
    QString        m_targetOverride;
    bool           m_running{false};
};

} // namespace Longpath
