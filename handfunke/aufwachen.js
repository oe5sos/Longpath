// Zurück aus dem Hintergrund — die Entscheidung, rein.
//
// Was auf einem Telefon beim Entsperren wirklich passiert, und was die
// Seite bis heute davon unbemerkt lässt:
//
//   * Der WebSocket ist tot, aber `onclose` kommt erst, wenn das System
//     die Seite wieder auftaut. Bis dahin steht der Wiederholungsabstand
//     womöglich schon bei 10 s — der Bediener sieht also sekundenlang
//     nichts, obwohl das Funkgerät bereit ist.
//
//   * Schlimmer: der Socket kann als OFFEN gemeldet werden und trotzdem
//     tot sein. iOS friert die Seite ein, die Gegenseite räumt auf, und
//     `readyState` hinkt hinterher. Wer nur auf den Zustand sieht,
//     wartet auf ein `onclose`, das nie kommt.
//
// Darum entscheidet hier nicht der Zustand allein, sondern der Zustand
// UND die Stille: kommt seit Sekunden nichts mehr an, wird neu verbunden,
// ganz gleich was der Socket behauptet. Eine überflüssige Neuverbindung
// kostet eine halbe Sekunde; eine ausgelassene kostet den Moment, in dem
// man das Telefon in die Hand genommen hat.

/** Ab dieser Stille gilt eine Verbindung als tot, auch wenn sie OFFEN sagt. */
export const kTotStilleMs = 3000;

/** WebSocket-Zustände, hier benannt statt als Zahl verstreut. */
export const VERBINDET = 0, OFFEN = 1, SCHLIESST = 2, ZU = 3;

/**
 * Soll nach dem Aufwachen sofort neu verbunden werden?
 *
 * @param {{gewollt:boolean, zustand:(number|null), stilleMs:number}} lage
 *   `zustand` ist `WebSocket.readyState` oder null, wenn gar keiner da ist.
 *   `stilleMs` ist, wie lange schon nichts mehr ankam.
 */
export function sollNeuVerbinden(lage) {
  const l = lage || {};
  // Wer gar nicht verbunden sein will, will es auch nach dem Aufwachen nicht.
  if (!l.gewollt) { return false; }
  const z = (l.zustand === null || l.zustand === undefined) ? ZU : l.zustand;
  if (z === ZU || z === SCHLIESST) { return true; }
  // VERBINDET: einen laufenden Versuch nicht abwürgen, SOLANGE er jung ist.
  // Hängt er schon länger als die Stillegrenze, ist er mit dem Telefon
  // eingeschlafen und wird von selbst nicht mehr wach.
  if (z === VERBINDET) { return (l.stilleMs || 0) > kTotStilleMs; }
  // OFFEN: nur bei Stille. Sonst würde jeder Blick auf den Bildschirm die
  // laufende Verbindung wegwerfen.
  return (l.stilleMs || 0) > kTotStilleMs;
}

/**
 * Ist das Bild, das gerade dasteht, noch von vor dem Schlafen?
 *
 * Darauf kommt es an: ein Wasserfall, der den Stand von vor zehn Minuten
 * zeigt, ist von einem lebendigen nicht zu unterscheiden. Er behauptet ein
 * leeres Band, und danach wird nicht gerufen.
 */
export function bildIstAlt(stilleMs, grenzeMs = 1500) {
  return (stilleMs || 0) > grenzeMs;
}
