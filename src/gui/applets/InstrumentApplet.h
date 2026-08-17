#pragma once

// =================================================================
// src/gui/applets/InstrumentApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Die Fassung um die beiden Instrumente ────────────────────────────
//
// OE5SOS, 2026-08-17: Zeiger und Balken sind ZWEI Ansichten EINER
// Sache — „bei den Instrumenten zwei (Zeiger / Balken)" steht als
// Umschalter im Panelkopf, und „die Fusszeile ist in beiden
// Instrumenten gleich aufgebaut, damit der Blick beim Wechsel nicht
// springt".
//
// Also ein Applet mit beiden Formen darin, nicht zwei Applets. Ein
// Umschalter, der zwischen zwei Applets wechselte, müsste eines
// abbauen und das andere aufbauen; ein Umschalter zwischen zwei
// Ansichten desselben Applets tauscht nur die Seite und behält Quelle,
// Spitze und Platz in der Spalte.
//
// ── Noch OHNE Umschalter ─────────────────────────────────────────────
//
// Der Knopf im Panelkopf gehört zu Punkt 5 der abgesprochenen
// Reihenfolge und ist hier bewusst nicht gebaut. setForm() ist die
// Stelle, an der er andocken wird. Bis dahin steht der Zeiger.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AppletWidget.h"

class QStackedWidget;

namespace NereusSDR {

class BarInstrument;
class NeedleInstrument;

class InstrumentApplet : public AppletWidget {
    Q_OBJECT

public:
    enum class Form { Needle, Bar };

    /// `id` und `title` kommen von aussen, weil es mehrere Instrumente
    /// geben soll — der Betreiber wählt je Instrument, was es zeigt.
    InstrumentApplet(const QString& id, const QString& title,
                     RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override    { return m_id; }
    QString appletTitle() const override { return m_title; }
    void    syncFromModel() override     {}

    /// Beide Formen zeigen dieselbe Grösse. Abgelehnt (false), wenn die
    /// Grösse keine belegte Skala hat.
    bool setPrimary(int bindingId);
    bool setSecondary(int bindingId);

    void setForm(Form f);
    Form form() const { return m_form; }

public slots:
    /// Direkt an MeterPoller::readingUpdated zu hängen.
    void onReading(int bindingId, double value);

private:
    QString m_id;
    QString m_title;
    Form    m_form{Form::Needle};

    QStackedWidget*   m_stack{nullptr};
    NeedleInstrument* m_needle{nullptr};
    BarInstrument*    m_bar{nullptr};
};

} // namespace NereusSDR
