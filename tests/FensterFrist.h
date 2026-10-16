#pragma once

// =================================================================
// tests/FensterFrist.h  (Longpath)
// =================================================================
//
// Wie lange ein Prueflauf darauf warten darf, dass ein Fenster auf
// dem Schirm steht.
//
// ── Warum es diese Zahl gibt ────────────────────────────────────────
//
// QTest::qWaitForWindowExposed() wartet per Vorgabe genau 5000 ms.
// Ein Longpath-MainWindow braucht allein rund 5 s, bis es sichtbar
// ist — gemessen am 2026-10-16 in tst_window_close_stays_closed:
// 4983, 4951, 5016 und 5431 ms fuer vier aufeinanderfolgende Fenster,
// danach der Fehlschlag bei 5078 ms. Einzeln lief jeder dieser
// Laeufe gruen; im vollen Durchgang (978 Laeufe, -j8) fielen 18
// davon um, und JEDER mit derselben Meldung:
//
//     QTest::qWaitForWindowExposed(...) returned FALSE
//
// Das ist kein Fehler im Programm, sondern eine zu knappe Frist:
// unter Last reicht die Vorgabe nicht fuer ein Fenster, das sie im
// Leerlauf schon fast ausschoepft.
//
// ── Warum sie so grosszuegig ist ────────────────────────────────────
//
// Die Frist laeuft nur im FEHLERFALL ab. Steht das Fenster, kehrt
// qWaitForWindowExposed sofort zurueck — eine hohe Frist kostet im
// gruenen Lauf nichts und macht aus einem Fehlschlag unter Last
// bloss ein paar Sekunden mehr Wartezeit. 20 s liegt deutlich ueber
// dem gemessenen Bedarf und deutlich unter CTests eigener Grenze.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

static constexpr int kFensterFrist = 20000;
