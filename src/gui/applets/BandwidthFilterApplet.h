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

class QLabel;
class QPushButton;
class QSpinBox;
class QHBoxLayout;

namespace NereusSDR {

class BandwidthFilterPane;
class SliceModel;

class BandwidthFilterApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit BandwidthFilterApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("bwfilter"); }
    QString appletTitle() const override { return QStringLiteral("Bandwidth Filter"); }
    void    syncFromModel() override;

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

    SliceModel* sliceAt(int i) const;
    SliceModel* activeSlice() const;

    QHBoxLayout* m_paneRow{nullptr};
    QList<BandwidthFilterPane*> m_panes;
    QList<int>   m_sliceIndices;

    QSpinBox*    m_lowBox{nullptr};
    QSpinBox*    m_widthBox{nullptr};
    QSpinBox*    m_highBox{nullptr};
    QPushButton* m_resetBtn{nullptr};
    QPushButton* m_spanBtn{nullptr};
    QLabel*      m_modeLbl{nullptr};

    bool m_updatingFromModel{false};
    int  m_spanHz{10000};
};

} // namespace NereusSDR
