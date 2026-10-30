#pragma once

// =================================================================
// src/core/safety/SwrRiegel.h  (Longpath)
// =================================================================
//
// „Sollte SWR ueber 2,5 sein, bitte nicht senden!"
//   — Betreiber, 2026-10-30
//
// Ein Riegel, kein Regler. Der vorhandene `SwrProtectionController`
// (Thetis-Port) faehrt bei hoher Stehwelle die LEISTUNG zurueck
// („windback") und sendet weiter. Das ist Thetis' Antwort und eine
// vernuenftige; sie ist nicht die, um die hier gebeten wurde.
//
// ── Warum es ihn am Flex ueberhaupt braucht ─────────────────────────
//
// Am 2026-10-30 nachgesehen: in `MainWindow_Flex.cpp` kommt das Wort
// „SWR" nicht vor. Die Maschine misst die Stehwelle selbst und
// schickt sie im Messwertstrom mit; `RadioModel` rechnet daraus die
// Balken FWD und SWR — und gibt sie an niemanden weiter, der
// eingreifen koennte. `flexTasten()` hat drei Tore (Verbindung,
// Sendescheibe, Verriegelung der Maschine), keines davon sieht die
// Stehwelle an.
//
// ── Zwei Riegel, nicht einer ────────────────────────────────────────
//
// VOR dem Tasten reicht nicht. FT8 tastet 13,2 Sekunden am Stueck;
// eine Antenne, die waehrenddessen abreiszt oder ein Relais, das
// nicht umlegt, wuerde den Sender eine ganze Sendung lang in eine
// Fehlanpassung treiben. Darum auch WAEHREND der Sendung, bei jeder
// Messung.
//
// ── Wann eine Messung nichts bedeutet ───────────────────────────────
//
// Unterhalb weniger Watt ist die Stehwellenmessung Rauschen: der
// Rueckwaertszweig misst dann gegen das eigene Grundrauschen. Eine
// Sperre, die beim Hochfahren der Sendung ausloest, sperrt jede
// Sendung. `kMinLeistungW` ist die Grenze, ab der die Zahl etwas
// heiszt.
//
// Und: eine Messung altert. Sie entsteht nur WAEHREND des Sendens —
// zwischen zwei Sendungen steht die Stehwelle bei 1,0, weil nichts
// flieszt. Der letzte Wert gilt darum fuer eine Weile weiter (das ist
// der Sinn: war die letzte Sendung schlecht, geht die naechste nicht
// los), aber nicht ewig: wer die Antenne wechselt, soll nicht eine
// Stunde gesperrt bleiben.
//
// ── Abwesenheit ist keine Aussage ───────────────────────────────────
//
// Wurde nie etwas gemessen, wird NICHT gesperrt. Dieselbe Regel, an
// der sich `flexTasten` schon einmal die Zaehne ausgebissen hat: dort
// stand `if (!m_flexDarfSenden) return false;`, und der Vorgabewert
// false verschluckte jeden Sendeversuch, weil die Maschine nie
// gesprochen hatte. Ein Riegel, der bei fehlender Messung zufaellt,
// ist kein Schutz, sondern ein Ausfall.
//
// Keine Uhr: die Zeit kommt von auszen herein, wie bei
// `Ft8Warteschlange` und `Ft8Stream`.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QString>

namespace Longpath::safety {

class SwrRiegel
{
public:
    /// Die Zahl des Betreibers (2026-10-30).
    static constexpr float kVorgabeGrenze = 2.5f;

    /// Darunter ist die Stehwellenmessung Rauschen, nicht Messung.
    static constexpr float kMinLeistungW = 1.0f;

    /// So lange gilt die letzte Messung fuer die Frage, ob ueberhaupt
    /// getastet werden darf.
    static constexpr qint64 kHaltbarkeitMs = 120 * 1000;

    void  setzeGrenze(float g);
    float grenze() const { return m_grenze; }

    void setzeAn(bool an) { m_an = an; }
    bool an() const { return m_an; }

    /// Eine Messung einspeisen. Gibt true, wenn JETZT gebrochen werden
    /// muss — der Aufrufer loest dann den Sender.
    bool messung(float swr, float vorwaertsW, qint64 jetztMs);

    /// Darf getastet werden? Ohne Messung immer ja (siehe Kopf).
    bool darfTasten(qint64 jetztMs) const;

    /// Warum nicht — fuer die Protokollzeile und das Fenster. Leer,
    /// wenn getastet werden darf.
    QString grund(qint64 jetztMs) const;

    float   letzteSwr()     const { return m_letzteSwr; }
    qint64  letzteMessung() const { return m_letzteMs; }

    /// Nach dem Loesen: die Messung bleibt stehen (sie soll ja das
    /// naechste Tasten verhindern), nur die Bruchmeldung faellt.
    void zuruecksetzen();

private:
    bool    m_an{true};
    float   m_grenze{kVorgabeGrenze};
    float   m_letzteSwr{0.0f};
    qint64  m_letzteMs{0};
    bool    m_hatGemessen{false};
    bool    m_gebrochen{false};
};

}  // namespace Longpath::safety
