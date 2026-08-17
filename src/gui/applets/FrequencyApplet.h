#pragma once

// =================================================================
// src/gui/applets/FrequencyApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Die Fassung um FrequencyInstrument — dieselbe Rolle, die
// InstrumentApplet fuer Zeiger und Balken spielt. Getrennt davon, weil
// die Frequenz keine Messgroesse aus ReadingSource ist, sondern der
// Zustand einer Scheibe: sie hat keine Skala, keine Schwelle und keine
// Spitze, dafuer eine Bedienung.
//
// Der Umschalter der drei Varianten gehoert in den Panelkopf (Punkt 5
// der abgesprochenen Reihenfolge) und ist hier nicht gebaut.
// setForm() ist die Stelle, an der er andocken wird.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AppletWidget.h"
#include "gui/instruments/FrequencyInstrument.h"

namespace NereusSDR {

class SliceModel;

class FrequencyApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit FrequencyApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override    { return QStringLiteral("Frequency"); }
    QString appletTitle() const override { return QStringLiteral("Frequenz"); }
    void    syncFromModel() override;

    void setForm(FrequencyInstrument::Form f) { m_instrument->setForm(f); }
    FrequencyInstrument* instrument() const { return m_instrument; }

    /// Die Scheibe, deren Frequenz gezeigt und gestellt wird.
    void bindSlice(SliceModel* slice);

private:
    FrequencyInstrument* m_instrument{nullptr};
    QPointer<SliceModel> m_slice;
};

} // namespace NereusSDR
