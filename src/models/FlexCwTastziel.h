#pragma once

// =================================================================
// src/models/FlexCwTastziel.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Morsetaste an der FlexRadio ──────────────────────────────────
//
// Die Fassung von `CwTastziel` (src/core/cw/CwTastung.h) fuer die Flex.
// Die Morsetaste geht nur an die Flex; der Fusstaster (`fordereSenden`
// und `freigabeSteht`) ist dagegen schlicht MOX und gilt fuer jedes
// Geraet und jede Betriebsart. Die Antworten, jede die engste, die es
// gibt:
//
//   bereit()        eine Flex ist verbunden, die Sendescheibe steht in
//                   CWL oder CWU, und es wird nicht gerade abgestimmt.
//   sendet()        MOX steht UND die Maschine hat ihr `xmit 1` wirklich
//                   bekommen (`RadioModel::flexGetastet`). MOX allein
//                   reicht nicht: `flexTasten` kann nach der Freigabe
//                   noch nein sagen (Stehwelle, Transverter auf nur
//                   Empfang, Verriegelung) -- und ein `cw key`, das dann
//                   trotzdem hinausginge, liesse eine Flex mit
//                   eingeschaltetem Break-in von selbst senden, an allen
//                   vier Toren vorbei.
//   freigabeSteht() `RadioModel::mox()` -- die Frage des Fusstasters.
//   fordereSenden() `RadioModel::setMox` -- der Weg durch MoxController
//                   und dessen BandPlanGuard-Vorpruefung. Nie daran
//                   vorbei.
//   taste()         `cw key immediate 1|0` ueber die Befehlsleitung. Das
//                   ist AetherSDRs Rueckfall ohne NetCW-Strom
//                   (RadioModel::sendNetCwCommand [@d58e2b8a]); der
//                   Strom mit Zeitstempeln ist die naechste Stufe.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/cw/CwTastung.h"

#include <QString>

namespace Longpath {

class RadioModel;

class FlexCwTastziel : public CwTastziel {
public:
    explicit FlexCwTastziel(RadioModel* radio) : m_radio(radio) {}

    bool bereit(QString* warum = nullptr) const override;
    bool sendet() const override;
    bool freigabeSteht() const override;
    void fordereSenden(bool an) override;
    void taste(bool unten) override;

    /// Der Wortlaut auf der Leitung -- rein, fuer den Pruefstand.
    static QString tastBefehl(bool unten);
    /// Die Regel hinter `sendet()`, rein: alle drei, nicht nur MOX.
    static bool darfTasten(bool flexVerbunden, bool mox, bool flexGetastet)
    {
        return flexVerbunden && mox && flexGetastet;
    }

private:
    RadioModel* m_radio;
};

} // namespace Longpath
