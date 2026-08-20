#pragma once

// =================================================================
// src/core/audio/AudioTapRing.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Ein Abgriff vom Audio-Faden zum Hauptfaden, ohne Schloss und ohne
// Speicheranforderung.
//
// ── Warum ueberhaupt ────────────────────────────────────────────────
//
// Die QSO-Aufnahme sammelt in QVector<float>. Anhaengen fordert
// Speicher an, und CLAUDE.md sagt es klar: im Audio-Rueckruf wird kein
// Schloss gehalten — und Speicher anfordern ist schlimmer als ein
// Schloss, weil es unvorhersehbar lange dauert.
//
// TxAudioRecorder loest dasselbe Problem anders: es fordert die vollen
// 30 Sekunden im Voraus an und der Audio-Faden schreibt nur hinein. Fuer
// eine Ansage geht das auf (5,5 MB). Fuer ein QSO nicht: 30 Minuten
// Stereo in float32 sind 345 MB je Spur, zweimal also fast 700 MB,
// dauerhaft belegt fuer den Fall, dass jemand aufnehmen koennte.
//
// Also ein kleiner Zwischenspeicher: der Audio-Faden schreibt hinein,
// ein Zeitgeber im Hauptfaden holt ab. Dort darf Speicher angefordert
// werden.
//
// ── Ein Schreiber, ein Leser ────────────────────────────────────────
//
// Genau EIN Faden schreibt und genau EINER liest. Das ist keine
// Bequemlichkeit, sondern die Bedingung, unter der die beiden Zaehler
// ohne Schloss auskommen: jeder hat seinen eigenen Schreiber, und
// keiner muss den anderen aendern.
//
// ── Was bei Ueberlauf passiert ──────────────────────────────────────
//
// Neues wird VERWORFEN, nicht Altes ueberschrieben. Ein Zwischen-
// speicher, der ueberlaeuft, bedeutet, dass niemand mehr abholt; dann
// ist die Aufnahme ohnehin verloren. Sie am Anfang statt am Ende
// kaputtzumachen macht es nur schwerer zu bemerken. Die Zahl der
// verworfenen Werte steht in dropped() — eine stille Luecke in einer
// Aufnahme ist das Schlimmste, was hier passieren kann.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <atomic>
#include <cstddef>
#include <vector>

namespace Longpath {

class AudioTapRing
{
public:
    // Der Platz wird EINMAL angefordert, vor dem ersten Schreiben.
    // Danach fordert diese Klasse nie wieder Speicher an.
    explicit AudioTapRing(int capacityFloats = 0);

    // Nur aufrufen, wenn gerade niemand schreibt. Wirft den Inhalt weg.
    void resize(int capacityFloats);

    int capacity() const { return static_cast<int>(m_buf.size()); }

    // ── Audio-Faden ──────────────────────────────────────────────────
    //
    // Schreibt, was hineinpasst, VERWIRFT den Rest und zaehlt ihn als
    // verloren. Gibt zurueck, wie viele Werte angekommen sind.
    //
    // Fuer Schreiber, die nicht warten koennen — der Audio-Faden hat
    // seinen Block jetzt, und beim naechsten Aufruf ist er weg. Was
    // hier nicht hineinpasst, ist wirklich verloren.
    int write(const float* data, int count) noexcept;

    // Dasselbe, aber OHNE als Verlust zu zaehlen.
    //
    // Fuer Schreiber, die es gleich nochmal versuchen. Der Unterschied
    // ist nicht kosmetisch: dropped() traegt die Aussage „in der
    // Aufnahme fehlt etwas", und die Anzeige stellt sie dem Betreiber
    // als Warnung hin. Ein Schreiber, der nach einem Teilschreiben
    // erneut anklopft, hat nichts verloren — er hat gewartet. Wuerde
    // write() auch das zaehlen, stuende die Warnung bei jeder vollen
    // Runde da und waere nach dem dritten Mal nichts mehr wert.
    //
    // Gefunden von tst_audio_tap_ring unter Last: der Test schrieb in
    // einer Wiederholungsschleife und bekam Zehntausende „Verluste"
    // gemeldet, obwohl jeder Wert ankam.
    int tryWrite(const float* data, int count) noexcept;

    // ── Hauptfaden ───────────────────────────────────────────────────
    // Holt bis zu maxCount Werte ab und gibt zurueck, wie viele es
    // waren.
    int read(float* out, int maxCount) noexcept;

    // Wie viel liegt gerade bereit.
    int available() const noexcept;

    // Wie viele Werte seit dem letzten reset() verworfen wurden. Eine
    // Zahl groesser null heisst: in der Aufnahme fehlt etwas.
    long long dropped() const noexcept
    { return m_dropped.load(std::memory_order_relaxed); }

    // Leert den Zwischenspeicher und den Verlustzaehler. Nur aufrufen,
    // wenn gerade niemand schreibt.
    void reset() noexcept;

private:
    std::vector<float>     m_buf;
    std::atomic<size_t>    m_writePos{0};
    std::atomic<size_t>    m_readPos{0};
    std::atomic<long long> m_dropped{0};
};

} // namespace Longpath
