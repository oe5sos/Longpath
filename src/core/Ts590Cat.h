#pragma once

// no-port-check: Longpath-original. Befehle aus Kenwoods "PC Control
// Command Reference for the TS-590S Transceiver" (2010).

// =================================================================
// src/core/Ts590Cat.h  (Longpath)
// =================================================================
//
// Was Longpath vom TS-590 liest und was es dort verstellen darf, an
// einer Stelle und ohne Socket pruefbar.
//
// ── Was hier bewusst fehlt ──────────────────────────────────────
//
// Nichts, was sendet oder das Geraet umbaut: kein TX, kein KY, kein PB,
// kein PS, kein EX, kein PC. Die Liste ist geschlossen, es gibt keinen
// Weg, hier einen freien Befehl durchzureichen. Longpath tastet ein
// fremdes Geraet nicht, und ein Rechnerausfall darf den Funkbetrieb nicht
// beruehren (Betreiber, 2026-10-27).
//
// ── Warum kein `l RFPOWER` ──────────────────────────────────────
//
// Hamlib 4.7.x stellt beim TS-590S mit dieser Abfrage die Leistung still
// auf 5 W (Hamlib #2240). Am 2026-11-07 stand das Geraet danach wirklich
// auf PC005. Gelesen wird darum nur mit dem rohen `PC;`.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QByteArray>
#include <QList>

namespace Longpath::Ts590Cat {

enum class Agc { Aus = 0, Langsam = 1, Schnell = 2 };

/// Was das Geraet zuletzt gemeldet hat. -1 heisst: noch keine Antwort.
struct Zustand {
    int  antenne{-1};          // 1 oder 2
    int  rxAnt{-1};            // 0/1, Empfang ueber RX ANT
    int  drive{-1};            // 0/1, Sendesignal auf DRV
    int  vorverstaerker{-1};   // 0/1
    int  daempfung{-1};        // 0/1
    int  rfGain{-1};           // 0..255
    int  agc{-1};              // Agc als Zahl
    int  nb{-1};               // 0, 1, 2
    int  nr{-1};               // 0, 1, 2
    int  leistungW{-1};        // 5..100
    int  meterPunkte{-1};      // 0..30: S-Meter, beim Senden Leistung
    int  swrPunkte{-1};        // 0..30
    int  lowCutHz{-1};
    int  highCutHz{-1};
    int  cwBreiteHz{-1};       // nur in CW, sonst bleibt der letzte Wert
    int  ritAn{-1};            // 0/1
    int  ritHz{0};             // Versatz, -9999..9999
    int  xitAn{-1};            // 0/1, teilt sich den Versatz mit RIT
    int  notch{-1};            // 0 aus, 1 Auto, 2 von Hand
    int  nbPegel{-1};          // 1..10, nur bei NB an
    int  nrPegel{-1};          // 1..10, nur bei NR1
    int  alcPunkte{-1};        // 0..30, beim Senden, wenn das Geraet ALC zeigt
    int  filter{-1};           // 1 = Filter A, 2 = Filter B

    bool operator==(const Zustand&) const = default;
};

/// Alle Abfragen eines Rundlaufs, ohne SM0 (das kommt haeufiger).
QList<QByteArray> abfragen();

/// Das S-Meter, die eine Groesse, die mehrmals je Sekunde gebraucht wird.
QByteArray abfrageMeter();

/// Beim Senden an der Stelle der anderen Abfragen.
QByteArray abfrageSwr();

/// Eine Antwort wie "PA10;" oder "AN100;" in den Zustand schreiben.
/// false, wenn sie nichts Bekanntes ist.
bool leseAntwort(QByteArray antwort, Zustand& z);

/// Punkte des Geraets in dBm: S9 (15 Punkte) = -73 dBm, darunter 3,6 dB
/// je Punkt, darueber 4 dB bis S9+60.
double punkteZuDbm(int punkte);

/// Grenzen der SSB-Filter des Geraets. Tabelle aus der Referenz; die
/// Menge ist fest, ein Wunsch dazwischen faellt auf den naechsten Wert.
int naechsterLowCut(int hz);
int naechsterHighCut(int hz);
int naechsteCwBreite(int hz);

// ── Die Zeilen fuer rigctld ─────────────────────────────────────
QByteArray stelleVorverstaerker(bool an);
QByteArray stelleDaempfung(bool an);
QByteArray stelleRfGain(int wert);        // 0..255
QByteArray stelleAgc(Agc agc);
QByteArray stelleNb(bool an);
QByteArray stelleNr(bool an);
QByteArray stelleLowCut(int hz);
QByteArray stelleHighCut(int hz);
QByteArray stelleCwBreite(int hz);
QByteArray stelleFilter(int filter);      // 1 = A, 2 = B
QByteArray stelleRit(bool an);
QByteArray stelleRitSchritt(int hz);      // + hoch, - runter
QByteArray stelleRitNull();

// XIT am TS-590 (2026-11-13). Das Geraet fuehrt EIN Versatzregister fuer
// beide: `RU`/`RD`/`RC` verschieben und loeschen es, `RT` schaltet es auf
// den Empfang, `XT` auf das Senden. Darum gibt es hier kein
// stelleXitSchritt() und kein stelleXitNull() -- das waeren dieselben
// Befehle unter zweitem Namen, und der Anschein zweier getrennter
// Versaetze waere schlimmer als die fehlende Funktion.
QByteArray stelleXit(bool an);
QByteArray stelleNotch(bool an);          // Auto-Notch
QByteArray stelleNbPegel(int pegel);      // 1..10
QByteArray stelleNrPegel(int pegel);      // 1..10

} // namespace Longpath::Ts590Cat
