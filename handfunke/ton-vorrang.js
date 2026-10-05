// Der Ton hat Vorrang vor dem Bild — die Entscheidung, rein.
//
// WARUM ES DAS BRAUCHT: am Telefon ist die Seite kein sicherer Kontext
// (http von einer LAN-Adresse). Dann gibt es kein AudioWorklet, und der Ton
// laeuft ueber einen ScriptProcessorNode — im HAUPTFADEN, demselben Faden,
// der Spektrum und Wasserfall zeichnet. Wer dort 20 ms lang malt, waehrend
// der Tonblock faellig ist, bekommt eine Luecke. Auf Deutsch: es knackt.
//
// Der Tonkern kann dagegen nichts tun. Er hat Anlaufsperre, sanften
// Gleichlauf und eine Notbremse, aber alle drei setzen voraus, dass er
// ueberhaupt aufgerufen WIRD. Gegen einen blockierten Faden hilft nur,
// weniger zu zeichnen.
//
// WAS HIER NICHT PASSIERT: dauerhaft aufhoeren zu zeichnen. Ein Wasserfall,
// der stehenbleibt, ist von einem toten Empfang nicht zu unterscheiden --
// und das waere ein schlimmerer Fehler als das Knacken, gegen das er hilft.
// Darum die Obergrenze fuer aufeinanderfolgende Auslassungen.

/**
 * Entscheidet je Bild, ob gezeichnet werden darf.
 *
 * Mit Hysterese: ausgelassen wird ab `untenAnteil` des Zielvorrats, wieder
 * gezeichnet erst ab `obenAnteil`. Ohne das flattert die Entscheidung im
 * Bildtakt, und flackerndes Zeichnen kostet mehr als es spart.
 */
export class Zeichenbremse {
  constructor(o = {}) {
    this.untenAnteil = o.untenAnteil ?? 0.6;   // darunter: auslassen
    this.obenAnteil  = o.obenAnteil  ?? 0.9;   // darueber: wieder zeichnen
    // 15 Bilder sind bei 60 Hz rund eine Viertelsekunde. Danach wird einmal
    // gezeichnet, egal wie es um den Ton steht: lieber ein Knacken als ein
    // Bild, das luegt.
    this.hoechstensAmStueck = o.hoechstensAmStueck ?? 15;
    this.bremst = false;
    this.amStueck = 0;
    this.ausgelassen = 0;   // insgesamt, fuer die Fusszeile
  }

  /**
   * @param {{vorrat:number, ziel:number, imHauptfaden:boolean}} lage
   * @returns {boolean} true = zeichnen
   */
  darfZeichnen(lage) {
    const l = lage || {};
    // Laeuft der Ton im Worklet, hat er einen eigenen Faden. Dann kostet
    // Zeichnen ihn nichts, und Bremsen waere reiner Verlust.
    if (!l.imHauptfaden) { this.bremst = false; this.amStueck = 0; return true; }

    const ziel = l.ziel > 0 ? l.ziel : 0;
    const vorrat = l.vorrat >= 0 ? l.vorrat : 0;
    // Ohne bekanntes Ziel laesst sich nichts beurteilen. Nicht bremsen:
    // eine Bremse auf Verdacht haelt das Bild an, ohne dass dem Ton
    // geholfen waere.
    if (ziel <= 0) { this.bremst = false; this.amStueck = 0; return true; }

    const anteil = vorrat / ziel;
    if (this.bremst) {
      if (anteil >= this.obenAnteil) { this.bremst = false; }
    } else if (anteil < this.untenAnteil) {
      this.bremst = true;
    }

    if (!this.bremst) { this.amStueck = 0; return true; }

    if (this.amStueck >= this.hoechstensAmStueck) {
      // Pflichtbild: der Wasserfall darf nicht einfrieren.
      this.amStueck = 0;
      return true;
    }
    this.amStueck++;
    this.ausgelassen++;
    return false;
  }
}
