#pragma once

// =================================================================
// src/gui/applets/BandwidthFilterApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, nach der Vorlage des Betreibers (Zeus Link
// „BANDWIDTH FILTER", 2026-08-20). Rechenregeln aus Thetis, siehe
// SliceModel::widthToEdges und ::defaultFilterCenter.
//
// Die Durchlassflaeche, eine je Empfaenger, plus die Zahlen.
//
// ── Was die Flaeche beantworten muss ────────────────────────────────
//
// „Wo liegt mein Filter im Band, und wie breit ist er." Beides
// gleichzeitig — deshalb eine echte Frequenzachse und nicht ±Hz um
// null. Die Vorlage liest 13.137 MHz, und genau das ist die Frage, die
// man wirklich hat.
//
// ── Eine Flaeche je Empfaenger ──────────────────────────────────────
//
// Nebeneinander und farblich getrennt, wie in der Vorlage. Die zweite
// erscheint nur, wenn es eine zweite Scheibe GIBT — eine leere Haelfte
// ist verschenkter Platz, und sie sieht aus wie ein Fehler.
//
// ── Die Zahlen sind eintippbar ──────────────────────────────────────
//
// Der einzige bewusste Unterschied zur Vorlage: dort sind Low Cut,
// High Cut und Breite Beschriftungen. Hier kann man sie setzen. Wer
// „2400" fuer die Breite eintippt, bekommt die Kanten nach der Regel
// seiner Betriebsart — die Rechnung steckt in
// SliceModel::widthToEdges, nicht hier.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "AppletWidget.h"

#include <QList>
#include <functional>

class QLabel;
class QPushButton;
class QSpinBox;
class QHBoxLayout;

namespace Longpath {

class BandwidthFilterPane;
class SliceModel;
enum class DSPMode : int;

class BandwidthFilterApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit BandwidthFilterApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("bwfilter"); }
    QString appletTitle() const override { return QStringLiteral("Bandwidth Filter"); }
    void    syncFromModel() override;

protected:
    /// Blendet die Wortmarken der Zahlenfelder aus, sobald die Zeile
    /// eng wird — siehe Begruendung an der Umsetzung.
    void resizeEvent(QResizeEvent* event) override;

public:
    /// Sagt Qt ausdruecklich, wie schmal dieses Applet werden darf.
    ///
    /// Ohne die Ueberschreibung rechnet Qt die Untergrenze aus der
    /// BREITEN Anordnung (rund 600 Punkte) — und weil das Fenster dann
    /// gar nicht schmaler werden KANN, kommt der Umbruch in
    /// resizeEvent() nie zum Zug. Henne und Ei; gemessen am
    /// 2026-08-22.
    QSize minimumSizeHint() const override;

    /// Woher der Spektrumausschnitt kommt.
    ///
    /// Das Applet kennt den Panadapter nicht und soll ihn auch nicht
    /// kennen — MainWindow reicht eine Abfrage herein, wie schon beim
    /// Scheiben-Aufloeser der Overlay-Leiste. Rueckgabe: dBm-Werte von
    /// loHz bis hiHz, gleichmaessig auf `points` Stuetzstellen.
    using SpectrumSource =
        std::function<QVector<float>(int sliceIndex, double loHz,
                                     double hiHz, int points)>;
    void setSpectrumSource(SpectrumSource src);

    // Fuer Tests: die Flaechen, in der Reihenfolge der Scheiben.
    QList<BandwidthFilterPane*> panes() const { return m_panes; }

private:
    void buildUI();

    // Flaechen an die vorhandenen Scheiben angleichen. Erzeugt und
    // entfernt, statt alles neu zu bauen — eine Flaeche, die beim
    // Frequenzwechsel kurz verschwindet, flackert.
    void rebuildPanes();

    void wirePane(BandwidthFilterPane* pane, int sliceIndex);
    void refreshPane(int i);
    void refreshNumbers();

    /// Die beiden VAR-Knoepfe nachziehen: leer oder mit Breite.
    void refreshVarButtons();

    SliceModel* sliceAt(int i) const;
    SliceModel* activeSlice() const;

    QHBoxLayout* m_paneRow{nullptr};
    QList<BandwidthFilterPane*> m_panes;
    QList<int>   m_sliceIndices;

    QSpinBox*    m_lowBox{nullptr};
    QSpinBox*    m_widthBox{nullptr};
    QSpinBox*    m_highBox{nullptr};
    // Betreiber 2026-09-03: "ich muss beide Werte frei eingeben koennen" --
    // SliceModel::setFilterWidth() verankert LSB/USB/CW immer am
    // Vorgabewert der Betriebsart (widthToEdges()), egal was zuletzt von
    // Hand in LOW oder HIGH stand. Diese Kennung merkt sich, welche der
    // beiden Kanten der Bedienende zuletzt selbst gesetzt hat, damit der
    // WIDTH-Anschluss GENAU DIESE Kante festhaelt statt sie stillschweigend
    // zu verwerfen. Nicht ueber m_lowBox/m_highBox erschliessbar: waehrend
    // eines Edits zeigen beide Felder schon den neuen Wert, das Modell
    // aber noch den alten -- ohne eigene Kennung liesse sich "zuletzt"
    // nicht mehr feststellen, sobald WIDTH selbst dran ist.
    enum class LastEditedEdge { Low, High };
    /// Welche Kante SliceModel::widthToEdges() fuer diese Betriebsart als
    /// Anker behandelt -- High fuer die LSB-Familie, Low fuer die
    /// USB-Familie. Nur fuer die beiden Betriebsartfamilien
    /// aussagekraeftig, die der WIDTH-Anschluss selbst unterscheidet.
    static LastEditedEdge naturalAnchorEdge(DSPMode mode);
    LastEditedEdge m_lastEditedEdge{LastEditedEdge::High};
    /// Fuer welche (Scheibe, Betriebsart) m_lastEditedEdge zuletzt einen
    /// echten Bedienereingriff gesehen hat. Wechselt eines der beiden,
    /// ist die gemerkte Kante nicht mehr die Auskunft des Bedienenden,
    /// sondern ein Zufallstreffer aus der vorigen Scheibe/Betriebsart --
    /// dann wird auf den von SliceModel::widthToEdges vorgesehenen
    /// natuerlichen Anker (High fuer LSB-Familie, Low fuer USB-Familie)
    /// zurueckgesetzt statt ihn stehen zu lassen. m_lastSyncedModeInt
    /// haelt den rohen DSPMode-Wert (int statt Enum, damit dieser Header
    /// keinen Modellheader ziehen muss); -1 heisst "noch nie
    /// synchronisiert", trifft also beim allerersten Aufruf immer.
    SliceModel*    m_lastSyncedSlice{nullptr};
    int            m_lastSyncedModeInt{-1};
    QList<QPushButton*> m_varBtns;
    QPushButton* m_resetBtn{nullptr};
    QPushButton* m_spanBtn{nullptr};
    /// Die Verbindungen jeder Flaeche zu ihrer Scheibe. Werden bei
    /// jedem Neubau geloest und neu geknuepft — siehe rebuildPanes().
    /// Folgt die Spanne dem gewaehlten Filter? Vorgabe: ja.
    bool           m_spanAuto{true};
    SpectrumSource m_spectrumSource;
    class QTimer*  m_traceTimer{nullptr};
    QList<QMetaObject::Connection> m_paneConns;
    QList<QLabel*> m_shrinkableLabels;
    /// Die Bedienzeile. Eng bricht sie in zwei Reihen um — siehe
    /// resizeEvent().
    class QHBoxLayout* m_ctrlRow{nullptr};
    class QHBoxLayout* m_ctrlRow2{nullptr};
    QList<QWidget*>    m_tier2;
    bool               m_ctrlWrapped{false};
    QLabel*      m_modeLbl{nullptr};

    bool m_updatingFromModel{false};
    int  m_spanHz{10000};
};

} // namespace Longpath
