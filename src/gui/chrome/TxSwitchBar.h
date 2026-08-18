#pragma once

// no-port-check: NereusSDR-original. Kein Upstream-Port.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/chrome/TxSwitchBar.h  (NereusSDR)
// =================================================================
//
// Die vier Sendeschalter in der unteren Leiste: MOX, VOX, TUNE, PS.
//
// Betreiber-Entscheidung 2026-08-18 (Fussleisten-Entwurf, Zuschnitt A):
// die Schalter kommen nach unten und BLEIBEN ZUGLEICH in der TxApplet.
// Beide Flaechen bedienen dasselbe Modell — MoxController,
// TransmitModel, PureSignal — und keine kennt die andere. Damit koennen
// sie nicht auseinanderlaufen: wer eine umlegt, sieht die andere
// mitgehen, weil beide auf dasselbe Signal hoeren.
//
// Der Preis der Entscheidung, hier vermerkt statt verschwiegen: jede
// spaetere Aenderung an einem der vier Schalter muss an zwei Stellen
// nachgezogen werden. Ein Ort je Handlung waere billiger gewesen; der
// schnelle Griff ohne Blickwechsel war dem Betreiber mehr wert.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QWidget>

class QPushButton;

namespace NereusSDR {

class RadioModel;

/// Ein einzelner Sendeschalter fuer die Chrome-Leiste.
///
/// Je Schalter ein eigenes Widget, weil die Faltleiter je Sprosse EIN
/// Widget kennt und die vier verschieden wichtig sind: PS faellt
/// zuerst, MOX zuletzt. Ein Sender, den man nicht abschalten kann, ist
/// das schlechteste Ende einer Faltung.
class TxSwitch : public QWidget {
    Q_OBJECT

public:
    enum class Kind { Mox, Vox, Tune, Ps };

    TxSwitch(Kind kind, RadioModel* model, QWidget* parent = nullptr);

    /// Faltsprosse. Hoeher heisst: faellt spaeter.
    int rung() const;

    /// Fuer Tests: der Knopf selbst.
    QPushButton* button() const { return m_btn; }
    Kind kind() const { return m_kind; }

private:
    void wire();

    Kind         m_kind;
    RadioModel*  m_model{nullptr};
    QPushButton* m_btn{nullptr};
    bool         m_updatingFromModel{false};
};

} // namespace NereusSDR
