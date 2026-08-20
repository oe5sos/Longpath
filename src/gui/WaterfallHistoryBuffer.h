#pragma once

// =================================================================
// src/gui/WaterfallHistoryBuffer.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Port of AetherSDR `src/gui/WaterfallHistoryBuffer.{h,cpp}`
//   (AetherSDR 31b29583). AetherSDR is licensed under the GNU General
//   Public License v3. NereusSDR is also GPLv3. Attribution follows
//   GPLv3 §5 requirements.
//
// ── Warum Intensität und nicht Farbe ─────────────────────────────────
//
// NereusSDR hielt die Wasserfall-Historie als fertiges RGB32-Bild. Eine
// RGB-Zeile ist ein Ergebnis: man kann sie anzeigen und sonst nichts.
// Nicht umfärben, nicht umrechnen, nicht befragen.
//
// Das war an einer Stelle direkt zu sehen: ein Wechsel des Farbschemas
// galt erst ab der nächsten Zeile. Was schon im Bild stand, behielt
// seine alten Farben, und quer über den Wasserfall lief ein Streifen
// dort, wo umgeschaltet wurde.
//
// Hier liegt stattdessen ein Byte je Punkt — ein normierter Index
// 0..255 in die Farbtabelle. Gefärbt wird erst beim Zeichnen. Damit ist
// die Historie eine Messung und keine Anzeige, und eine Messung kann
// man später noch einmal anders darstellen.
//
// ── Warum in Blöcken ─────────────────────────────────────────────────
//
// Zwanzig Minuten Historie bei 2000 Punkten Breite sind je nach Rate
// einige Dutzend Megabyte. Sie im Voraus zu belegen, hieße: jeder zahlt
// für zwanzig Minuten, auch wer die App zwei Minuten laufen lässt.
//
// Die Anzahl der Plätze steht deshalb von Anfang an fest, der Speicher
// dahinter entsteht in Blöcken zu 256 Zeilen, sobald die Historie sie
// erreicht. Ein nie beschriebener Block kostet einen leeren QByteArray.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Ported in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QByteArray>
#include <QSize>
#include <QVector>

namespace Longpath {

class WaterfallHistoryBuffer final {
public:
    static constexpr int kRowsPerChunk = 256;

    WaterfallHistoryBuffer() = default;
    WaterfallHistoryBuffer(const WaterfallHistoryBuffer&) = default;
    WaterfallHistoryBuffer& operator=(const WaterfallHistoryBuffer&) = default;
    WaterfallHistoryBuffer(WaterfallHistoryBuffer&& other) noexcept;
    WaterfallHistoryBuffer& operator=(WaterfallHistoryBuffer&& other) noexcept;

    /// Plätze festlegen. Der Speicher dahinter entsteht erst beim
    /// Beschreiben. Ein Aufruf mit denselben Maßen tut nichts — sonst
    /// verlöre ein Fenster bei jedem Zeichnen seine Historie.
    void configure(int width, int capacityRows);

    /// Inhalt weg, Plätze bleiben. Für einen Bandwechsel: die alten
    /// Zeilen gehören zu einer anderen Frequenz.
    void discardRows();

    /// Alles weg, auch die Plätze.
    void reset();

    /// Breite ändern und den Inhalt mitnehmen (nächster Nachbar).
    /// Ein Fenster, das breiter gezogen wird, soll seine Historie
    /// behalten — grob umgerechnet ist besser als schwarz.
    bool resizeWidth(int width);

    bool isConfigured() const { return m_width > 0 && m_capacityRows > 0; }
    int width() const { return m_width; }
    int capacityRows() const { return m_capacityRows; }
    QSize size() const { return QSize(m_width, m_capacityRows); }

    /// Zeile zum Schreiben. Belegt den Block beim ersten Zugriff.
    quint8* writableRow(int row);

    /// Zeile zum Lesen, oder nullptr, wenn dieser Block noch nie
    /// beschrieben wurde. Der nullptr ist die Auskunft „hier war noch
    /// nichts" und nicht ein Fehler — der Aufrufer malt dann nichts.
    const quint8* row(int row) const;

    qsizetype allocatedBytes() const;
    int allocatedChunkCount() const;

private:
    int rowsInChunk(int chunkIndex) const;

    int m_width{0};
    int m_capacityRows{0};
    QVector<QByteArray> m_chunks;
};

} // namespace Longpath
