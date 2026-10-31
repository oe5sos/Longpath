#pragma once

// =================================================================
// src/gui/widgets/RasterOverlay.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-10-31: „man sollte den raster auch ein und
// ausblenden können."
//
// Das Raster gab es seit dem 2026-09-02, aber nur als Rechnung: die
// Fenster schnappten auf unsichtbare Linien. Wer pruefen wollte, ob
// zwei Kanten wirklich auf derselben liegen, musste Bildpunkte
// zaehlen — genau das hat am 2026-10-31 den Befund „funktioniert fuer
// die ausrichtung auch nicht perfekt" gebraucht.
//
// Darum ein Blatt, das die Linien zeichnet. Es liegt ueber dem
// Hauptfenster, nimmt keine Maus an (`WA_TransparentForMouseEvents`)
// und rechnet in BILDSCHIRMKOORDINATEN: dieselben, in denen
// `snapToGridAfterSettle` rastet. Ein Raster, das bei 0 des
// Hauptfensters anfinge, zeigte woanders hin als das, an dem die
// Fenster haengen.
//
// Zurueckhaltung nach Hausstil: eine Haarlinie im Grau des
// Hintergrunds, keine Farbe. Es ist ein Hilfsmittel beim Einrichten,
// kein Zustand.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QWidget>

namespace Longpath {

class RasterOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit RasterOverlay(QWidget* parent = nullptr);

    /// Die Rasterweite in Punkten. Vorgabe: `kSnapGridPx`.
    void setGrid(int grid);
    int  grid() const { return m_grid; }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    int m_grid;
};

}  // namespace Longpath
