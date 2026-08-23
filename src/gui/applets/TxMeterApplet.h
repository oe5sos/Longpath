#pragma once

// =================================================================
// src/gui/applets/TxMeterApplet.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── EINE Flaeche, zwei Aufgaben ─────────────────────────────────────
//
// Der Betreiber am 2026-08-23:
//   "weitere idee, ein widget, wo SWR und Stehwelle in einem Diagramm
//    sind. wenn ich tune stellt es auf das diagramm SWR um, beim
//    senden habe ich Stehwelle. dann würde ich mir auch einen platz
//    sparen."
//
// Der Gedanke ist nicht nur Platz, er ist auch sachlich richtig: beim
// ABSTIMMEN sieht man auf die Anpassung — man dreht, bis das SWR
// faellt. Beim SENDEN sieht man auf die Ausgangsleistung; das SWR
// steht dann laengst fest und aendert sich nicht mehr. Zwei Anzeigen
// nebeneinander zeigen also zu jedem Zeitpunkt eine, die gerade
// niemand liest.
//
// ── Eine Lesart, die ich offengelegt habe ───────────────────────────
//
// "Stehwelle" und "SWR" sind dasselbe Wort. Aus dem Zusammenhang —
// zwei verschiedene Anzeigen fuer zwei verschiedene Zustaende — kann
// nur gemeint sein: TUNE zeigt das Stehwellenverhaeltnis, Senden
// zeigt die Leistung. So ist es gebaut, und setBindings() macht die
// Zuordnung umstellbar, falls die Lesart falsch war.
//
// ── Warum das Umschalten haengen bleibt ─────────────────────────────
//
// Faellt der Sender ab, bliebe die Anzeige sonst auf der Leistung
// stehen und zeigte null — oder sie spraenge zurueck aufs SWR und
// zeigte den Wert von vorhin. Beides ist falsch. Sie behaelt darum
// nach dem Senden die zuletzt gezeigte Groesse mitsamt ihrer
// Spitzenhaltung, bis der naechste Sendevorgang beginnt. Wer nach dem
// Loslassen der Taste hinsieht, findet, was er gerade gemacht hat.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AppletWidget.h"
#include "gui/instruments/BarInstrument.h"

class QLabel;

namespace Longpath {

class TxMeterApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit TxMeterApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override    { return QStringLiteral("TxMeter"); }
    QString appletTitle() const override { return QStringLiteral("SWR / Leistung"); }
    void    syncFromModel() override;

    /// Messwerte hereinreichen — dieselbe Rolle wie
    /// InstrumentApplet::onReading.
    void onReading(int bindingId, double value);

    /// Welche Groesse in welchem Zustand steht. Vorgabe: beim
    /// Abstimmen das SWR, beim Senden die Leistung.
    void setBindings(int whileTuning, int whileTransmitting);

    /// Was gerade gezeigt wird. Fuer Pruefungen und fuer die
    /// Titelzeile.
    int currentBinding() const { return m_current; }

    BarInstrument* bar() const { return m_bar; }

    void saveState() const;
    void restoreState();

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    void chooseBinding();
    void applyBinding(int bindingId);

    BarInstrument* m_bar{nullptr};
    QLabel*        m_state{nullptr};

    int  m_tuneBinding{-1};
    int  m_txBinding{-1};
    int  m_current{-1};
    bool m_wasActive{false};
};

} // namespace Longpath
