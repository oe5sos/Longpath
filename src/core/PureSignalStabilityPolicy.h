#pragma once

// =================================================================
// src/core/PureSignalStabilityPolicy.h  (Longpath)
// =================================================================
//
// Longpath-original, nach einer Idee von Yuri EU2AV.
//
// ── Woher die Idee stammt ───────────────────────────────────────────
//
// Yuri EU2AV — derselbe, der die Anvelina PRO III baut — beschreibt in
// seiner "Thetis Extended Version" (Juli 2026) zwei Zutaten, die es in
// der Thetis-Hauptlinie nicht gibt:
//
//   * einen "Kaltstart-Schutz" gegen IMD-Artefakte waehrend der
//     Erstkalibrierung, und
//   * das Beibehalten der Korrektur bei kurzen Datenaussetzern.
//
// Sein Quelltext ist NICHT veroeffentlicht (nur ein Installer), also
// ist hier nichts abgeschrieben. Uebernommen ist der Gedanke, und der
// ist gut begruendbar:
//
// ── Warum ein Kaltstart schadet ─────────────────────────────────────
//
// PureSignal misst die eigene Aussendung ueber den Rueckweg und
// rechnet daraus eine Vorverzerrung. Solange diese Rechnung noch nicht
// eingeschwungen ist, ist die Korrektur nicht bloss unvollstaendig,
// sondern FALSCH — sie verzerrt in eine Richtung, die mit der
// Kennlinie der Endstufe wenig zu tun hat. In den ersten Sekunden
// einer Kalibrierung kann eingeschaltete Korrektur den Nachbarkanal
// also schlechter machen als gar keine.
//
// Thetis schaltet trotzdem sofort scharf. Wer eine Kalibrierung mit
// gedrueckter Taste startet, sendet diese Sekunden mit.
//
// ── Warum ein Aussetzer schadet ─────────────────────────────────────
//
// Faellt der Rueckweg kurz aus — ein verlorenes Paket, ein
// Pegelsprung —, rechnet die Anpassung auf leeren oder unsinnigen
// Daten weiter und verdirbt eine Korrektur, die vorher gut war.
// Besser ist, sie einzufrieren: die letzte gute Korrektur bleibt
// stehen, bis wieder brauchbare Daten kommen.
//
// ── Warum das hier eine eigene Klasse ist ───────────────────────────
//
// Weil sie sich OHNE SENDER pruefen laesst. Ein Zustandsautomat aus
// drei Zahlen hat keine Endstufe noetig, und ein Fehler darin waere
// sonst nur mit einem Messplatz zu finden. Dieselbe Ueberlegung wie
// bei KiwiSdrTxMutePolicy, das aus demselben Grund fuer sich steht.
//
// ── NICHT gegen Hardware geprueft ───────────────────────────────────
//
// Ich habe keinen Sender. Die Regel folgt einer nachvollziehbaren
// Begruendung, aber ob die Schwellen richtig gewaehlt sind, kann nur
// der Betrieb zeigen. Sie ist darum abschaltbar.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <cstdint>

namespace Longpath {

/// Was mit der Korrektur geschehen soll.
enum class PsCorrectionAction {
    /// Noch nicht anwenden — die Kalibrierung ist nicht eingeschwungen.
    /// Gesendet wird unkorrigiert, also so gut wie die Endstufe eben
    /// von sich aus ist. Das ist besser als falsch korrigiert.
    Withhold,
    /// Normalbetrieb: rechnen und anwenden.
    Run,
    /// Einfrieren: die letzte gute Korrektur weiter anwenden, aber
    /// NICHT nachrechnen. Fuer die Dauer eines Aussetzers.
    Hold,
};

struct PureSignalStabilityPolicy {
    // ── Die drei Schwellen ──────────────────────────────────────────
    //
    // minCalibrations: wie viele abgeschlossene Kalibrierungen noetig
    //   sind, bevor die Korrektur scharf geschaltet wird. Zwei ist die
    //   kleinste Zahl, die ueberhaupt etwas aussagt — nach der ersten
    //   weiss man nur, DASS gerechnet wurde, nach der zweiten, dass es
    //   sich nicht mehr stark aendert.
    //
    // Die Pegelgrenzen stammen aus Thetis: FeedbackLevel <= 256 heisst
    // "in Ordnung" (PSForm.cs:1113), und > 90 heisst "korrigiert
    // gerade" (PSForm.cs:1106). Dazwischen liegt der brauchbare
    // Bereich. Beides ist hier NICHT neu erfunden, sondern uebernommen.
    int minCalibrations{2};
    int feedbackOkMax{256};
    int feedbackOkMin{1};

    // Wie lange ein schlechter Rueckweg anliegen darf, bevor
    // eingefroren wird. Kurze Ausreisser sollen nicht sofort
    // umschalten — das gaebe ein Flattern zwischen Rechnen und
    // Einfrieren, das schlimmer waere als beides.
    int holdAfterBadMs{120};

    // Und wie lange nach der Rueckkehr guter Daten gewartet wird, ehe
    // wieder gerechnet wird. Asymmetrisch mit Absicht: einfrieren
    // schnell, auftauen langsam.
    int resumeAfterGoodMs{400};

    bool enabled{true};

    // ── Zustand ─────────────────────────────────────────────────────
    bool     everSettled{false};   ///< War die Kalibrierung schon einmal fertig?
    bool     holding{false};
    int64_t  badSinceMs{-1};
    int64_t  goodSinceMs{-1};

    /// Bei jedem Sendebeginn aufrufen. Der Kaltstart-Schutz gilt je
    /// Aussendung NICHT neu — eine eingeschwungene Korrektur bleibt
    /// ueber die Pause hinweg gueltig, sonst waere jedes Antippen der
    /// Taste wieder ein Kaltstart.
    void onTransmitStart()
    {
        holding = false;
        badSinceMs = -1;
        goodSinceMs = -1;
    }

    /// Nach einem Zuruecksetzen der Korrekturtabelle: der naechste
    /// Kaltstart gilt wieder.
    void onCorrectionsCleared()
    {
        everSettled = false;
        holding = false;
        badSinceMs = -1;
        goodSinceMs = -1;
    }

    /// Die Entscheidung. nowMs ist eine monoton steigende Zeit in
    /// Millisekunden; die Regel rechnet nur mit Abstaenden.
    PsCorrectionAction decide(int calibrationCount, int feedbackLevel,
                              int64_t nowMs)
    {
        if (!enabled) { return PsCorrectionAction::Run; }

        const bool good = (feedbackLevel >= feedbackOkMin
                           && feedbackLevel <= feedbackOkMax);

        // ── Kaltstart ───────────────────────────────────────────────
        if (!everSettled) {
            if (calibrationCount >= minCalibrations && good) {
                everSettled = true;
            } else {
                return PsCorrectionAction::Withhold;
            }
        }

        // ── Aussetzer ───────────────────────────────────────────────
        if (!good) {
            goodSinceMs = -1;
            if (badSinceMs < 0) { badSinceMs = nowMs; }
            if (nowMs - badSinceMs >= holdAfterBadMs) { holding = true; }
        } else {
            badSinceMs = -1;
            if (holding) {
                if (goodSinceMs < 0) { goodSinceMs = nowMs; }
                if (nowMs - goodSinceMs >= resumeAfterGoodMs) {
                    holding = false;
                    goodSinceMs = -1;
                }
            }
        }

        return holding ? PsCorrectionAction::Hold : PsCorrectionAction::Run;
    }
};

} // namespace Longpath
