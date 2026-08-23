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
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/widgets/VfoTileRow.h"

namespace Longpath {

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

    // ── Zusatzbalken unter der Frequenz ──────────────────────────────
    //
    // Der Betreiber am 2026-08-23: "optional sollte man auch beim
    // widget frequenz den unteren balken als stehwelle auswählen
    // können, und auch als swr dazu, sprich frequenz widget als option
    // mit stehwelle und auch swr zusätzlich."
    //
    // Der Gedanke dahinter ist Platz: wer am Notebook sitzt, will
    // Frequenz, Stehwelle und SWR sehen, ohne dafuer drei Applets
    // uebereinanderzustapeln — jedes mit eigener Titelleiste und
    // eigenem Rand. Ein Widget mit zwei angehaengten Zeilen kostet
    // deutlich weniger Hoehe als drei Widgets.
    //
    // Beide Zeilen sind EINZELN schaltbar, nicht als Paar: wer nur das
    // SWR will, soll nicht die Leistung mitnehmen muessen.
    /// Die Kachelreihe ueber der Ziffernanzeige, nach der Vorlage des
    /// Betreibers vom 2026-08-23 (Bildschirmfoto Zeus Link).
    void setShowTiles(bool on);
    bool showsTiles() const { return m_showTiles; }

    /// Zustand der KIWI-Kachel. Setzt MainWindow, weil nur es den
    /// Panadapter kennt.
    void setKiwiState(bool on) { if (m_tiles) { m_tiles->setKiwiOn(on); } }

    void setShowPower(bool on);
    void setShowSwr(bool on);
    bool showsPower() const { return m_showPower; }
    bool showsSwr() const   { return m_showSwr; }

    /// Messwerte durchreichen — dieselbe Rolle wie
    /// InstrumentApplet::onReading, damit MainWindow beide gleich
    /// bedienen kann.
    void onReading(int bindingId, double value);

    void saveState() const;
    void restoreState();

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;

signals:
    /// Weitergereicht an MainWindow, das als einziges weiss, welcher
    /// Panadapter zu dieser Scheibe gehoert.
    void kiwiDisplayToggleRequested(SliceModel* slice);

private slots:
    /// Die KIWI-Kachel wurde gedrueckt. Das Applet kennt seine Scheibe
    /// und ueber sie den Panadapter — die Kachelreihe kennt beides
    /// nicht und soll es auch nicht.
    void toggleKiwiDisplay();

private:
    void applyRowVisibility();

    VfoTileRow*          m_tiles{nullptr};
    FrequencyInstrument* m_instrument{nullptr};
    BarInstrument*       m_powerBar{nullptr};
    BarInstrument*       m_swrBar{nullptr};
    bool                 m_showTiles{true};
    bool                 m_showPower{false};
    bool                 m_showSwr{false};
    QPointer<SliceModel> m_slice;
};

} // namespace Longpath
