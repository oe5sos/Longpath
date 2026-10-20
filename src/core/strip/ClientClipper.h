#pragma once

// =================================================================
// src/core/strip/ClientClipper.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Geregeltes Abschneiden -- die Stufe, die bei SSB wirklich
// Sprechleistung bringt.
//
// ── Warum ein Begrenzer dafür nicht reicht ───────────────────────────
//
// Bei SSB begrenzt die Endstufe die SPITZE, gehört wird der
// EFFEKTIVWERT. Der Abstand zwischen beiden -- der Scheitelfaktor --
// ist verschenkte Leistung. Eine ungefasste Stimme bringt 16 bis
// 20 dB mit.
//
// Ein Kompressor schiebt die lauten Stellen herunter und die leisen
// herauf. Er macht den Pegelverlauf gleichmässiger, aber er lässt
// die einzelne Spitze im Wellenzug stehen -- und genau die legt die
// Endstufe fest. Ein Begrenzer dreht bei der Spitze schnell zurück,
// was dasselbe Problem eine Stufe später wiederholt.
//
// Abschneiden macht etwas anderes: es nimmt die Spitze WEG. Drei
// Dezibel Abschnitt bringen rund drei Dezibel mehr Effektivwert bei
// gleicher Spitze, und das ist auf der Gegenseite ein hörbarer
// Unterschied -- deutlich mehr als jede Entzerrerkurve.
//
// Der Preis ist Verzerrung, und sie ist beabsichtigt. Deswegen:
// wenig, hörbar geprüft, und mit einer Zahl daneben (dem
// Scheitelfaktor auf der Übersichtsseite), statt nach Gefühl.
//
// ── Warum zweifach überabgetastet ────────────────────────────────────
//
// Abschneiden erzeugt Oberwellen bis hinauf ins Unendliche. Bei
// 48 kHz Abtastrate faltet sich alles über 24 kHz zurück IN das
// Sprachband -- ein 3,1-kHz-Ton erzeugt abgeschnitten eine
// fünfzehnte Oberwelle bei 46,5 kHz, und die landet als Störton bei
// 1,5 kHz mitten in der Stimme. Kein Filter dahinter bekommt sie je
// wieder heraus, weil sie im Nutzband liegt.
//
// Also: auf 96 kHz hoch, dort abschneiden, zurück auf 48 kHz. Dann
// braucht es die dreissigste Oberwelle, um überhaupt zurückzufalten,
// und die ist dreissig Mal schwächer. Kostet rund 0,3 ms und ist der
// Unterschied zwischen einem Clipper und einem Verzerrer.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include <array>
#include <atomic>

namespace Longpath {

class ClientClipper {
public:
    ClientClipper();

    void prepare(double sampleRate);

    void setEnabled(bool on) noexcept;
    bool isEnabled() const noexcept;

    // Wieviel in die Schwelle hineingefahren wird. 0 heisst: nichts
    // wird abgeschnitten.
    void  setDriveDb(float db) noexcept;      // 0 .. 18 dB
    float driveDb() const noexcept;

    // 0 = harte Kante, 1 = weicher Übergang. Weich klingt runder und
    // erzeugt weniger Oberwellen, nimmt aber weniger Spitze weg.
    void  setKnee(float k) noexcept;          // 0 .. 1
    float knee() const noexcept;

    // ── Lautstärkeabgleich, und warum er NICHT die Vorgabe ist ──────
    //
    // Erster Entwurf nahm nach dem Abschneiden die Verstärkung wieder
    // heraus (Faktor 1/drive). Der Prüfstand hat das sofort
    // widerlegt: Spitze -5,1 dB, Effektivwert -2,6 dB -- der
    // Scheitelfaktor wurde zwar besser, aber das ganze Signal leiser.
    // Damit war der Gewinn unsichtbar und ungenutzt, denn NIEMAND in
    // der Kette holt ihn sich wieder.
    //
    // Ein Clipper arbeitet andersherum: die Verstärkung hinein IST
    // der Gewinn, und die Kante hält die Spitze auf der Decke. Mehr
    // Effektivwert bei gleicher Spitze -- genau das, wofür es ihn
    // gibt. Also keine Nachregelung.
    //
    // Bleibt das echte Problem: beim Ein- und Ausschalten vergleicht
    // man dann laut gegen leise und hält lauter für besser. Das ist
    // die häufigste Selbsttäuschung beim Einstellen von Sendeton.
    // Dafür ist dieser Schalter da -- er gleicht den GEMESSENEN
    // Effektivwert an, damit man den KLANG des Abschneidens beurteilt
    // und nicht die Lautstärke. Zum Beurteilen an, zum Senden aus.
    void setLevelMatch(bool on) noexcept;
    bool levelMatch() const noexcept;

    // Wieviel Spitze die Stufe im letzten Block weggenommen hat, in
    // dB (>= 0). Gemessen, nicht gerechnet.
    float clippedDb() const noexcept;

    void process(float* samples, int frames, int channels) noexcept;
    void reset() noexcept;

private:
    static constexpr int kTaps = 32;          // Halbbandfilter
    std::array<float, kTaps> m_h{};           // Koeffizienten
    std::array<float, kTaps> m_up{};          // Verzögerung hinauf
    std::array<float, kTaps> m_down{};        // Verzögerung herunter
    int m_upPos{0};
    int m_downPos{0};

    std::atomic<bool>  m_on{false};
    std::atomic<float> m_driveDb{0.0f};
    std::atomic<float> m_knee{0.3f};
    std::atomic<bool>  m_levelMatch{false};

    // ── Warum zwei geglättete Mittel und kein Regelkreis ────────────
    //
    // Erster Entwurf war eine Rückkopplung: Verhältnis von Ein- zu
    // Ausgang messen, damit die eigene Verstärkung nachziehen. Der
    // Prüfstand fand darin eine bleibende Abweichung von 1,30 dB,
    // und zwar KEINE langsame Annäherung -- sie blieb stehen, egal
    // wie lange er lief.
    //
    // Der Grund: die Blöcke sind 240 Werte, die Stimme ist nicht
    // blockweise gleich laut, und das Mittel aus vielen
    // Augenblicksverhältnissen ist nicht das Verhältnis der Mittel.
    // Bei Sprache wäre die Abweichung grösser gewesen als hier.
    //
    // Jetzt werden die beiden Leistungen getrennt geglättet und erst
    // danach geteilt. Kein Kreis, keine Schieflage.
    double m_msIn{0.0};               // mittlere Leistung hinein
    double m_msRaw{0.0};              // dieselbe nach der Kante
    double m_rate{48000.0};
    float  m_matchGain{1.0f};
    std::atomic<float> m_clippedDb{0.0f};
};

} // namespace Longpath
