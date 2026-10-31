#pragma once
// =================================================================
// src/gui/applets/PanelAutomatik.h  (Longpath)
// =================================================================
//
// Ein Panel, das von selbst aufgeht, wenn es etwas zu zeigen hat,
// und wieder zu, wenn nicht.
//
// Betreiber am 2026-10-30, zum Plaetze-Panel: „schoen waere, wenn
// hier die fenster eigens aufgehen und danach wieder zu gehen."
//
// ── Die drei Regeln ─────────────────────────────────────────────────
//
//  1. Kommt Inhalt und das Panel ist zu -> aufmachen.
//  2. Ist der Inhalt weg und WIR haben es aufgemacht -> nach einer
//     Nachlaufzeit wieder zu.
//  3. Hat der Betreiber selbst gegriffen, gehoert es ihm. Dann wird
//     es weder zugemacht noch wieder aufgedraengt.
//
// ── Warum eine Nachlaufzeit ─────────────────────────────────────────
//
// Zwischen zwei QSOs liegt bei FT8 eine halbe Minute, in der nichts
// laeuft. Ohne Nachlauf ginge das Panel nach jedem RR73 zu und beim
// naechsten Anrufer wieder auf -- ein Fenster, das im Takt des
// Bandes blinkt. `kNachlaufMs` ist laenger als diese Luecke.
//
// ── Warum Regel 3 ───────────────────────────────────────────────────
//
// Ein Fenster, das nach dem Zumachen wieder aufgeht, ist keine Hilfe
// mehr, sondern eine Zumutung. Und eines, das sich schlieszt,
// waehrend man hineinsieht, ist schlimmer. Beides faellt weg, sobald
// der Betreiber einmal selbst entschieden hat.
//
// Keine Uhr: die Zeit kommt von auszen herein, wie bei
// `Ft8Warteschlange` und `SwrRiegel`.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtGlobal>

namespace Longpath {

class PanelAutomatik
{
public:
    enum class Tat { Nichts, Aufmachen, Zumachen };

    /// So lange bleibt es nach dem letzten Inhalt noch stehen.
    /// Laenger als die Luecke zwischen zwei FT8-QSOs (30 s), sonst
    /// blinkt das Panel im Takt des Bandes.
    static constexpr qint64 kNachlaufMs = 45 * 1000;

    /// Was jetzt zu tun ist. `sichtbar` ist der IST-Zustand des
    /// Panels, nicht unser Wunsch -- wer selbst zumacht, waehrend
    /// noch Inhalt da ist, wird nicht ueberstimmt.
    Tat pruefe(bool hatInhalt, bool sichtbar, qint64 jetztMs);

    /// Der Betreiber hat das Panel selbst auf- oder zugemacht. Ab
    /// dann gehoert es ihm.
    void betreiberHatGegriffen() { m_betreiberEntschied = true; }

    bool gehoertDemBetreiber() const { return m_betreiberEntschied; }
    bool vonUnsAufgemacht()    const { return m_vonUnsAuf; }

    void zuruecksetzen();

private:
    bool   m_vonUnsAuf{false};
    bool   m_betreiberEntschied{false};
    qint64 m_leerSeit{0};
    bool   m_warLeer{true};
};

}  // namespace Longpath
