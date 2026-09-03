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
// ── Bedient wird über den Rechtsklick ────────────────────────────────
//
// OE5SOS, 2026-08-18: „RX-Quellen und Spitzenhaltung ins
// Rechtsklickmenü." Der Umschalter Zeiger/Balken war für den Panelkopf
// vorgesehen; der Kopf fällt weg, also steht er hier mit.
//
// Das Menü ist damit die EINE Stelle, an der ein Instrument eingestellt
// wird: Quelle, zweite Anzeige, Form, Spitzenhaltung. Die analoge
// S-Meter-Anzeige hatte dafür ihr eigenes Rechtsklickmenü mit einer
// eigenen Quellenauswahl (RxMode) — zwei Wege zu einer Entscheidung.
// Hier steht nur einer, und er bedient sich aus derselben Liste wie
// jedes andere Messwerkzeug (ReadingSource → MeterBinding).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AppletWidget.h"

#include <functional>

#include <QImage>

class QMenu;
class QStackedWidget;

namespace Longpath {

class BarInstrument;
class NeedleInstrument;
class PeakHold;

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

    // ── Balken: durchgehend oder Segmente ────────────────────────────
    //
    // Zeus zeigt das S-Meter als Kette einzelner Felder; der Betreiber
    // hat das am 2026-08-20 mit einem Bildschirmfoto verlangt. Beides
    // bleibt — Segmente liest man als Menge, der Verlauf zeigt kleine
    // Aenderungen genauer.
    /// Das Zifferblatt allein, auf durchsichtigem Grund — fuer die
    /// Einblendung im Panadapter. OHNE Kopf- und Fusszeile: beides
    /// gehoert nicht ueber ein Spektrum.
    QImage renderTransparent(int sidePx, qreal dpr = 2.0);

    void setSegmented(bool on);
    bool isSegmented() const;
    void setTube(bool on);
    bool isTube() const;

    // ── Frequenz zusaetzlich einblenden (2026-09-02) ──────────────────
    //
    // Betreiber: "die Idee ist, dass man sich bei kleinen Bildschirmen
    // vielleicht ein Fenster erspart" -- Frequenz mit im S-Meter/
    // Stehwelle statt in einem eigenen Frequenz-Fenster. Aus dem
    // Rechtsklickmenue schaltbar wie Form/Spitzenhaltung, gemerkt in
    // saveState()/restoreState(). Nur die Zeiger-Form zeigt sie derzeit
    // (die genehmigte Gestaltung war ausdruecklich fuer das Zifferblatt,
    // nicht fuer den Balken).
    void setShowFrequency(bool on);
    bool showFrequency() const;
    void setFrequencyHz(double hz);

    // ── Gemerkt wird es auch ─────────────────────────────────────────
    //
    // Bis 2026-08-20 merkte sich ein Instrument NICHTS: Form,
    // Spitzenhaltung und Quelle standen nach jedem Start wieder auf
    // der Vorgabe. Wer den Zeiger gegen den Balken tauscht, tut das
    // nicht fuer eine Sitzung.
    //
    // Die Quelle bleibt bewusst aussen vor: MainWindow setzt sie nach
    // dem Baukasten (setPrimary(MeterBinding::SignalAvg)), und ein
    // wiederhergestellter Wert wuerde davon gleich wieder
    // ueberschrieben — schlimmer als gar nicht merken, weil es
    // manchmal ginge.
    void saveState() const;
    void restoreState();

    /// Die Spitzenhaltung einer Ansicht. Beide sollen immer gleich
    /// stehen — wer sie einzeln abfragt, prüft genau das.
    const PeakHold& peakHold(Form f) const;

    /// Das Rechtsklickmenü, ohne es zu zeigen — für den Test, damit er
    /// die Einträge lesen kann, ohne ein Menü aufzuklappen.
    QMenu* buildContextMenu(QWidget* parent);

public slots:
    /// Direkt an MeterPoller::readingUpdated zu hängen.
    void onReading(int bindingId, double value);

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    /// Auf beide Ansichten anwenden. Eine Einstellung, die nur die
    /// sichtbare Form erreicht, springt beim Umschalten zurück.
    void forEachInstrument(const std::function<void(PeakHold&)>& fn);
    /// Nur die Seite umschalten, NICHTS merken. setForm() = applyForm()
    /// + saveState(); der Konstruktor darf nur ersteres -- sonst
    /// ueberschreibt er mit den Vorgaben, was restoreState() gleich
    /// darauf lesen will (2026-09-03, Roehre/Segmente ueberlebten den
    /// Neustart nie).
    void applyForm(Form f);
    QString m_id;
    QString m_title;
    Form    m_form{Form::Needle};

    QStackedWidget*   m_stack{nullptr};
    NeedleInstrument* m_needle{nullptr};
    BarInstrument*    m_bar{nullptr};
};

} // namespace Longpath
