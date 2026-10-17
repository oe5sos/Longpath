// Den Rotor von der Seite aus — die reinen Teile.
//
// Kein DOM, kein WebSocket. Hier steht, was auf einem Telefon still falsch
// sein kann, wenn am anderen Ende ein Mast steht:
//
//   1. **Die Peilung unter dem Finger.** Eine Scheibe, auf die man tippt,
//      rechnet einen Winkel aus. Rechnet sie falsch herum oder mit dem
//      falschen Nullpunkt, dreht die Antenne woanders hin als dorthin, wo
//      der Finger war — und das merkt man erst, wenn nichts mehr zu hören
//      ist.
//
//   2. **Der versehentliche Griff.** Ein Tipp, der sofort dreht, ist auf
//      einem Telefon in der Hosentasche eine schlechte Idee. Darum wählt
//      der erste Tipp nur, und erst ein zweiter, bewusster Griff dreht —
//      dieselbe Entscheidung wie bei der Sendetaste (Entwurf 1,
//      2026-10-04).
//
//   3. **Die alte Stellung.** Ein Rotor ist langsame Mechanik am Ende eines
//      Drahtes. Eine Nadel, die eine veraltete Richtung zeigt, ohne das zu
//      sagen, ist schlimmer als eine, die nichts zeigt — so steht es schon
//      im Kopf von RotorController.h, und die Seite hält sich daran.

/** `rotor_ist:`-Argumente -> Stand. Gibt `null`, wenn nichts dasteht. */
export function standLesen(args) {
  const grad = parseFloat(args[0]);
  if (isNaN(grad)) return null;
  // Der Zustand kommt als NAME, nicht als Zahl. Vorher stand hier eine
  // Nummer aus RotorController::State, und damit kannten zwei Stellen
  // dieselbe Zaehlung: die Aufzaehlung im Programm und diese Datei. Wer
  // einen Zustand in die Mitte einfuegt, verschiebt alles darueber -- und
  // dann stand unter der Scheibe "DREHT", wenn der Rotor einen Fehler
  // meldet. Am anderen Ende haengt ein Mast.
  const zustand = (args[1] || '').trim().toLowerCase();
  return {
    grad: ((grad % 360) + 360) % 360,
    zustand: zustand || GETRENNT,
    frisch: (args[2] || '') === '1',
  };
}

/** Zustandsnamen aus `rotor_ist:` (TciServer.cpp, Rotorblock). */
export const GETRENNT = 'getrennt', VERBINDET = 'verbindet',
             BEREIT = 'bereit', DREHT = 'dreht', FEHLER = 'fehler';

/** Was unter der Scheibe steht. Kein Rot: Rot bleibt der Warnung. */
export function zustandText(stand) {
  if (!stand) return 'KEIN ROTOR';
  switch (stand.zustand) {
    case GETRENNT:  return 'NICHT VERBUNDEN';
    case VERBINDET: return 'VERBINDET …';
    case DREHT:     return 'DREHT …';
    case FEHLER:    return 'ROTOR MELDET EINEN FEHLER';
    case BEREIT:    return stand.frisch ? '' : 'STELLUNG VON VORHIN';
    // Ein Name, den diese Seite nicht kennt, wird GESAGT und nicht
    // verschluckt. Vorher fiel jeder unbekannte Zustand in denselben Zweig
    // wie "bereit" und zeigte bei frischer Stellung gar nichts -- also las
    // sich ein unbekannter Zustand wie "alles in Ordnung".
    default:        return 'ZUSTAND UNBEKANNT';
  }
}

/**
 * Was bei einer Ablehnung unter der Scheibe steht.
 *
 * „nicht freigegeben" ist richtig und reicht nicht: auf einem Telefon
 * steht man damit vor einer Scheibe, die sich nicht dreht, und weiss
 * nicht, ob der Rotor kaputt ist, das Netz weg ist oder ein Schalter
 * fehlt. Am 2026-10-01 stand der Betreiber eine Stunde vor einer Seite,
 * die nur „keine Antwort" meldete, waehrend Longpath jede Verbindung
 * still abwies — dieselbe Sorte Auskunft, derselbe verlorene Abend.
 *
 * Seit dem 2026-10-07 gilt eine Verbindung ueber die Bruecke als aus dem
 * Netz, also trifft genau diese Ablehnung jeden, der vom Telefon drehen
 * will, bis der Schalter steht. Dann soll dort stehen, WO er steht.
 */
export function fehlerText(roh) {
  const t = (roh || '').trim();
  if (/nicht freigegeben/i.test(t)) {
    return 'NICHT FREIGEGEBEN \u2014 Setup \u203A CAT & Network \u203A TCI Server';
  }
  if (/peilung unbrauchbar/i.test(t)) {
    // Kommt nur bei einer Zahl, die keine ist. Der Bediener kann hier
    // nichts richten, aber er soll sehen, dass es an der Zahl lag und
    // nicht am Mast.
    return 'PEILUNG UNBRAUCHBAR';
  }
  return t ? t.toUpperCase() : 'ABGELEHNT';
}

/**
 * Welche Peilung liegt unter dem Finger?
 *
 * `x`/`y` sind relativ zur Mitte der Scheibe, in Bildpunkten, mit y nach
 * UNTEN (so wie jede Bildschirmkoordinate). Null Grad ist oben (Nord), und
 * gezählt wird im Uhrzeigersinn — wie auf jedem Kompass.
 *
 * Gibt `null` zurück, wenn der Finger zu nah an der Mitte war: dort ist die
 * Richtung nicht bestimmbar, und ein Tipp auf den Mittelpunkt darf keine
 * willkürliche Peilung ergeben.
 */
export function peilungAus(x, y, mindestRadius = 12) {
  const r = Math.sqrt(x * x + y * y);
  if (!(r >= mindestRadius)) return null;
  // atan2(x, -y): Nord oben, im Uhrzeigersinn. Nicht atan2(y, x) --
  // das waere Ost = 0 und gegen den Uhrzeigersinn.
  let g = Math.atan2(x, -y) * 180 / Math.PI;
  if (g < 0) g += 360;
  // Auf ganze Grad: feiner kann ein Finger nicht zielen, und ein Rotor
  // auch nicht.
  g = Math.round(g) % 360;
  return g;
}

/**
 * Die Sicherung: erst wählen, dann drehen.
 *
 * `waehle(grad)` merkt sich ein Ziel. `bestaetige()` gibt es frei — aber
 * nur, wenn vorher wirklich gewählt wurde und die Wahl nicht zu alt ist.
 * Eine Wahl, die eine Minute alt ist, gehört dem Finger von vorhin.
 */
export class Sicherung {
  constructor({ haltbarMs = 15000 } = {}) {
    this.haltbarMs = haltbarMs;
    this.ziel = null;
    this.gewaehltBei = 0;
  }

  waehle(grad, jetztMs) {
    if (typeof grad !== 'number' || !isFinite(grad) || grad < 0 || grad >= 360) {
      return false;
    }
    this.ziel = grad;
    this.gewaehltBei = jetztMs;
    return true;
  }

  /** Noch gültig? */
  gilt(jetztMs) {
    return this.ziel !== null && (jetztMs - this.gewaehltBei) <= this.haltbarMs;
  }

  /**
   * Gibt das Ziel frei und vergisst es — oder `null`, wenn nichts gewählt
   * war oder die Wahl zu alt ist. Danach ist die Sicherung wieder zu: ein
   * zweiter Druck dreht nicht noch einmal.
   */
  bestaetige(jetztMs) {
    if (!this.gilt(jetztMs)) { this.verwerfen(); return null; }
    const z = this.ziel;
    this.verwerfen();
    return z;
  }

  verwerfen() { this.ziel = null; this.gewaehltBei = 0; }
}
