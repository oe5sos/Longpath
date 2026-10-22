#pragma once

// =================================================================
// src/core/Ft8Decoder.h  (Longpath)
// =================================================================
//
// Longpath-original. Die Rechenarbeit macht ft8_lib (Kārlis Goba,
// MIT) — siehe third_party/ft8_lib/FT8LIB-PROVENANCE.md fuer
// Herkunft, Umfang und den Grund.
//
// ── Warum nativ und nicht ueber WSJT-X ──────────────────────────────
//
// Betreiber, 2026-10-15: „ft8 und ft4 sollte auch angedacht werden.
// zeus, aether usw. haben da schon arbeit geleistet."
//
// Nachgesehen: Thetis und AetherSDR dekodieren NICHT selbst — beide
// schicken den Ton ueber ein virtuelles Kabel an WSJT-X. Zeus schon,
// und zwar mit derselben Bibliothek, die hier liegt. Longpath hatte
// bis heute den WSJT-X-Weg (WsjtxClient sammelt fremde Decodes per
// UDP ein). Auf einem Berg heisst das: ein zweites Programm, zwei
// virtuelle Tonkabel, eine zweite Einrichtung und mehr Strom.
//
// ── Was diese Klasse ist, und was sie nicht ist ─────────────────────
//
// Sie ist eine REINE Funktion: ein Block Tonproben hinein, eine Liste
// Decodes heraus. Kein Faden, kein Taktgeber, keine Uhr. Genau das
// macht sie pruefbar — der Pruefstand erzeugt mit ft8_libs eigenem
// Sender eine Nachricht, moduliert sie, schickt sie hier hinein und
// vergleicht, was herauskommt. Eine geschlossene Schleife ohne
// Tondatei und ohne Funkgeraet.
//
// Das Zuhoeren am laufenden Ton (Periodengrenzen, Uhr, Faden) und das
// SENDEN sind eigene Schritte und stehen bewusst noch nicht hier.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QString>
#include <QVector>

namespace Longpath {

/// Eine dekodierte Nachricht, so wie sie in der Liste steht.
struct Ft8Decode {
    float   snrDb{0.0f};    ///< geschaetzt aus dem Sync-Wert, wie im Vorbild
    float   dtSeconds{0.0f};///< Zeitversatz im Zeitfenster
    float   freqHz{0.0f};   ///< Tonlage im Durchlassbereich
    int     score{0};       ///< Costas-Sync-Wert, roh
    QString text;           ///< „CQ DX OE5SOS JN78"
};

class Ft8Decoder {
public:
    enum class Protocol { Ft8, Ft4 };

    /// Abtastrate, mit der ft8_lib arbeitet. Wer anders abtastet,
    /// rechnet vorher um — hier wird NICHT stillschweigend
    /// umgerechnet, weil ein falsch abgetastetes FT8 nicht falsch
    /// aussieht, sondern einfach nichts dekodiert.
    static constexpr int kSampleRate = 12000;

    /// Ein ganzes Zeitfenster dekodieren.
    ///
    /// `samples` sind Monoproben bei kSampleRate, ueblicherweise ein
    /// volles Fenster (15 s FT8, 7,5 s FT4). Kuerzer geht auch — dann
    /// faellt entsprechend weniger an.
    /// `durchgaenge` ist die Zahl der Runden ueber dasselbe Fenster.
    /// Nach jeder Runde werden die gefundenen Signale aus einer KOPIE
    /// der Aufnahme herausgerechnet, damit schwaechere darunter
    /// auffindbar werden (siehe core/Ft8Subtract.h). 1 = wie frueher,
    /// ohne Abzug. Fuer FT4 gilt immer 1 -- die Subtraktion rechnet
    /// mit FT8-Massen.
    static QVector<Ft8Decode> decodeBlock(const float* samples,
                                          int count,
                                          Protocol protocol = Protocol::Ft8,
                                          int durchgaenge = 2);

    /// Dieselbe Rechnung rueckwaerts, fuer den Pruefstand und spaeter
    /// fuer das Senden: Nachricht -> Tonproben.
    ///
    /// Gibt false, wenn ft8_lib die Nachricht nicht packen kann (zu
    /// lang, unbekanntes Format).
    static bool encodeBlock(const QString& text,
                            float toneHz,
                            Protocol protocol,
                            QVector<float>* samples);
};

} // namespace Longpath
