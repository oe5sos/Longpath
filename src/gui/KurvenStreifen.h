// no-port-check: Longpath-original, keine Thetis-Logik.
//
// ── Streifen einer Kurve (2026-10-08) ───────────────────────────────────
//
// Die GPU zeichnet die Spektrumkurve als Band aus zwei Scheitelpunkten je
// Bildspalte: Mitte plus/minus Normale mal halbe Linienbreite. Das ergibt
// an steilen Flanken eine Treppe, weil das Band harte Kanten hat.
//
// Zwei Zutaten der gewaehlten Richtung "Glas & Tiefe" (2026-09-17, dort
// "Kurven mit Hof") brauchen mehr als ein Band:
//
//   * weiche Kante -- der Kern wird um einen halben Geraete-Pixel
//     schmaler, und aussen laeuft je ein Streifen von voller Deckkraft
//     auf 0. Das ist ein Gouraud-Verlauf, also kein neuer Shader;
//     AetherSDR 088b68a7 macht dasselbe im Fragment-Shader.
//   * Hof -- ein breiteres, schwaches Band unter der Linie.
//
// Hier steht nur die Geometrie, als reine Funktion ohne Qt-Fenster und
// ohne GPU: damit laesst sich die eine Zusage pruefen, auf der die
// Vorgabe "beide aus" beruht -- OHNE weiche Kante und OHNE Hof kommt
// GENAU EIN Streifen mit den alten Werten heraus. Siehe
// tests/tst_kurven_streifen.cpp.

#pragma once

#include <algorithm>

namespace Longpath::Kurve {

/// Ein Streifen des Bandes: zwei Kanten, je Abstand von der Linienmitte
/// (in Geraete-Pixeln, positiv in Richtung der Normalen) und Deckkraft.
struct Streifen {
    float o1{0.0f};
    float a1{0.0f};
    float o2{0.0f};
    float a2{0.0f};
};

/// Hoechstzahl: Hof oben, Hof unten, Kante oben, Kern, Kante unten.
inline constexpr int kStreifenHoechstens = 5;

/// Deckkraft des Hofs an der Linienmitte; aussen 0.
///
/// 22 % ist geborgt, nicht gewaehlt: docs/design/HAUSSTIL.md
/// §Weiche Uebergaenge laesst die Fuellung unter der Kurve mit
/// "22 % -> 0" auslaufen. Derselbe Wert, damit nicht zwei
/// Deckkraft-Vokabulare nebeneinander stehen. Der Hausstil
/// schreibt keinen Hof vor -- die Richtung "Glas & Tiefe" nennt ihn
/// (docs/design/2026-09-17-design-durchsicht.md, Blatt 3), ohne eine
/// Zahl dazu.
inline constexpr float kHofDeckkraft = 0.22f;

/// Breite des Auslaufs an jeder Kante, in Geraete-Pixeln.
inline constexpr float kKantePixel = 1.0f;

/// Wie weit der Hofrand ueber die halbe Linienbreite hinausreicht, in
/// logischen Pixeln -- mit dpr skaliert, damit er auf dem Retina-Schirm
/// nicht halb so breit aussieht.
inline constexpr float kHofPixel = 2.5f;

/// Untere Schranke fuer den Kern. Bei Linienbreite 1 und dpr 1 waere er
/// sonst 0 Pixel breit und die Kurve verschwaende.
inline constexpr float kKernMindestens = 0.35f;

/// Baut die Streifen eines Bandes nach `aus` (mindestens
/// `kStreifenHoechstens` Plaetze) und gibt ihre Anzahl zurueck.
///
/// `halbPx`     halbe Linienbreite in Geraete-Pixeln
/// `dpr`        Geraete-Pixel je logischem Pixel
/// `deckkraft`  Deckkraft des Kerns (Kurve 0,9 / Spitzenhaltelinie 0,55)
///
/// Ohne weiche Kante und ohne Hof: genau ein Streifen
/// `{ +halbPx, deckkraft, -halbPx, deckkraft }` -- Wert fuer Wert der
/// Stand vor diesem Entwurf.
inline int baue(bool weicheKante, bool hof, float halbPx, float dpr,
                float deckkraft, Streifen* aus)
{
    int n = 0;
    if (hof) {
        const float hofHalb = halbPx + kHofPixel * dpr;
        // Zwei Streifen, weil der Verlauf in der Mitte sein Maximum hat:
        // ein Dreiecksband von aussen nach innen, einmal je Seite.
        aus[n++] = { hofHalb, 0.0f, 0.0f, kHofDeckkraft };
        aus[n++] = { 0.0f, kHofDeckkraft, -hofHalb, 0.0f };
    }
    if (weicheKante) {
        const float kern =
            std::max(kKernMindestens, halbPx - 0.5f * kKantePixel);
        aus[n++] = { kern + kKantePixel, 0.0f, kern, deckkraft };
        aus[n++] = { kern, deckkraft, -kern, deckkraft };
        aus[n++] = { -kern, deckkraft, -kern - kKantePixel, 0.0f };
    } else {
        aus[n++] = { halbPx, deckkraft, -halbPx, deckkraft };
    }
    return n;
}

}  // namespace Longpath::Kurve
