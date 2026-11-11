// no-port-check: Longpath-original, no Thetis logic (Thetis has no rotor).
//
// =================================================================
// src/core/RotorBereich.h  (Longpath)
// =================================================================
//
// ── Wohin der Rotor darf -- und auf welchem Weg ─────────────────────
//
// 2026-11-08, Betreiber: „zu hause will ich das nicht probieren, weil der
// rotor nicht 360 drehen soll". Ein erlaubter Bogen, im Uhrzeigersinn von
// `vonGrad` bis `bisGrad`, der fuer JEDEN Drehbefehl aus Longpath gilt.
//
// Es reicht nicht, das Ziel zu pruefen. Ein Rotor faehrt nicht den kurzen
// Weg, sondern den zwischen seinen Anschlaegen: ein Yaesu mit Anschlag im
// Norden kommt von 280 nach 80 Grad ueber SUEDEN, auch wenn beide Enden im
// erlaubten Bogen 270-90 liegen. Darum kennt die Einstellung den
// mechanischen Anschlag und prueft den ganzen Weg:
//
//   Rotorlage p(az) = (az - Anschlag) mod 360, linear zwischen 0 und 360.
//   (Ohne Anschlag, "turns freely": der kurze Weg.)
//   Der Weg von a nach b ist das Intervall [p(a), p(b)]; er ist erlaubt,
//   wenn es ganz im erlaubten Bogen liegt.
//
// Liegt der Anschlag IM erlaubten Bogen, zerfaellt der Bogen fuer den
// Rotor in zwei Stuecke, zwischen denen er nicht wechseln kann, ohne den
// gesperrten Teil zu ueberstreichen. Das ist keine Schwaeche der Rechnung,
// sondern die Mechanik: dann gibt es diesen Weg wirklich nicht.
//
// Rotoren mit 450-Grad-Ueberlappung werden wie 360-Grad-Rotoren gerechnet
// -- vorsichtig: der Ueberlapp wird nie benutzt, um eine Sperre zu umgehen.

#pragma once

#include <QString>

namespace Longpath {

struct RotorBereich {
    bool   an{false};          ///< aus: alles erlaubt
    double vonGrad{0.0};       ///< erlaubt im Uhrzeigersinn von ...
    double bisGrad{360.0};     ///< ... bis
    /// Mechanischer Anschlag -- dieselbe Einstellung wie "End stop" im
    /// Rotor-Fenster (RotorEndStopDeg, seit 2026-08-10). < 0 heisst: der
    /// Rotor dreht frei und nimmt den kurzen Weg.
    double anschlagGrad{-1.0};

    /// Toleranz an den Raendern: so genau melden Rotoren ihre Lage.
    static constexpr double kRandGrad = 1.0;

    /// Bezug der Rotorlage: der Anschlag, oder bei frei drehendem Rotor
    /// der Anfang des Bogens.
    double bezug() const;

    /// Liegt diese Richtung im erlaubten Bogen?
    bool erlaubt(double az) const;
    /// Ist der ganze Weg von `von` nach `nach` erlaubt (Rotor faehrt
    /// zwischen seinen Anschlaegen, nicht den kurzen Weg)?
    bool wegErlaubt(double von, double nach) const;
    /// Das zusammenhaengende Stueck, das der Rotor von `az` aus ohne
    /// Gesperrtes erreicht: Anfang (Kompass) und Laenge im Uhrzeigersinn.
    /// false, wenn `az` selbst nicht erlaubt ist.
    bool reichweite(double az, double* anfangGrad, double* spanneGrad) const;

    /// "270° → 90° clockwise (stop at north)" -- fuer die Oberflaeche.
    QString beschreibung() const;

    static RotorBereich load();
    void save() const;
};

} // namespace Longpath
