// ton-kern.js — Ringpuffer und Hochtastung, unabhaengig davon, WER den Ton
// ausgibt.
//
// Warum diese Datei ueberhaupt getrennt ist (2026-09-30):
//
// Der Ton lief bis dahin ausschliesslich ueber ein AudioWorklet. Am echten
// LAN gemessen faellt das aus — `AudioWorklet` ist [SecureContext], und eine
// Seite von http://192.168.x.x (oder hier http://172.30.30.121:8767) ist
// KEIN sicherer Kontext. Gemessen im Browser an genau dieser Adresse:
//
//     isSecureContext:       false
//     ctx.audioWorklet:      undefined      <- der Ton war tot
//     ctx.createScriptProcessor: da
//
// Nur `http://127.0.0.1` und `http://localhost` gelten als sicher — genau
// dort lief die Pruefung, deshalb ist es nicht aufgefallen. Auf dem Telefon
// waere die Handfunke stumm gewesen, und `tonStarten()` hat den Fehler mit
// einem console.warn verschluckt.
//
// Also braucht es einen Rueckfall auf den (veralteten, aber nicht
// [SecureContext]) ScriptProcessorNode. Damit gibt es zwei Ausgabewege — und
// sobald zwei Wege dieselbe Rechnung doppelt fuehren, laufen sie auseinander.
// Deshalb steht die Rechnung genau einmal hier.
//
// Die Datei laedt in BEIDEN Welten:
//   - Hauptfaden:  <script src="ton-kern.js"> in index.html
//   - Worklet:     ctx.audioWorklet.addModule('ton-kern.js') VOR rx-worklet.js
//                  (mehrere addModule-Aufrufe teilen sich den globalen
//                  Gueltigkeitsbereich des Worklets)
//
// Der Ringpuffer haelt absichtlich wenig: eine Fernbedienung, die eine halbe
// Sekunde hinterherhinkt, ist zum Mithoeren beim Drehen unbrauchbar. Laeuft
// er ueber, fallen die AELTESTEN Proben weg, nicht die neuesten — sonst
// klebt die Verzoegerung fest, statt sich abzubauen.

class TonKern {
  // cap/target sind in QUELLproben gemessen, nicht in Ausgabeproben. Das ist
  // wichtig, seit die Rate ausgehandelt wird: bei 12 kHz sind 2400 Proben
  // 200 ms, nicht 50. Wer hier Zahlen setzt, muss durch die Quellrate teilen.
  constructor(opts) {
    const o = opts || {};
    this.ausgabeRate = o.ausgabeRate || 48000;
    this.srcRate = o.srcRate || this.ausgabeRate;

    // Vorrat und Deckel in MILLISEKUNDEN angeben und hier umrechnen — so
    // stimmen sie bei jeder ausgehandelten Rate. Vorher standen hier feste
    // Probenzahlen, und bei 12 kHz war der dokumentierte "ca. 50 ms"-Vorrat
    // in Wirklichkeit 200 ms (Durchsicht 2026-09-30).
    this.vorratMs = o.vorratMs || 120;
    this.deckelMs = o.deckelMs || 1000;

    this._raten();
    this.l = new Float32Array(this.cap);
    this.r = new Float32Array(this.cap);
    this.w = 0; this.rd = 0; this.have = 0;
    this.pos = 0;              // Leseposition, gebrochen
    this.muted = false;
    this.starved = 0;
  }

  _raten() {
    this.cap = Math.max(1024, Math.ceil(this.srcRate * this.deckelMs / 1000));
    this.target = Math.max(64, Math.ceil(this.srcRate * this.vorratMs / 1000));
    this.schritt = this.srcRate / this.ausgabeRate;
  }

  // Die Quellrate darf sich mitten im Betrieb aendern (der Bediener schaltet
  // auf schmale Leitung). Puffer leeren statt die alten Proben mit dem neuen
  // Schritt zu lesen — das waere ein hoerbarer Sprung.
  setRate(srcRate) {
    this.srcRate = srcRate || this.ausgabeRate;
    const alteKap = this.cap;
    this._raten();
    if (this.cap !== alteKap) {
      this.l = new Float32Array(this.cap);
      this.r = new Float32Array(this.cap);
    }
    this.leeren();
  }

  leeren() { this.w = this.rd = this.have = 0; this.pos = 0; }

  push(vals, channels) {
    const frames = channels === 2 ? (vals.length >> 1) : vals.length;
    if (frames <= 0) { return; }

    // Ein einzelner Block, der groesser ist als der ganze Ring, kann nur
    // teilweise hinein — die letzten cap Proben sind die richtigen.
    const start = frames > this.cap ? frames - this.cap : 0;
    const nehme = frames - start;

    const room = this.cap - this.have;
    if (nehme > room) {
      const drop = nehme - room;
      this.rd = (this.rd + drop) % this.cap;
      this.have -= drop;
    }
    for (let i = 0; i < nehme; i++) {
      const q = start + i;
      const w = (this.w + i) % this.cap;
      if (channels === 2) { this.l[w] = vals[q * 2]; this.r[w] = vals[q * 2 + 1]; }
      else                { this.l[w] = vals[q];     this.r[w] = vals[q]; }
    }
    this.w = (this.w + nehme) % this.cap;
    this.have = Math.min(this.cap, this.have + nehme);
  }

  // Fuellt L und R mit n Ausgabeproben. Gibt false zurueck, wenn nichts da
  // war (dann stehen Nullen drin) — der Aufrufer kann das melden.
  zieh(L, R, n) {
    const gebraucht = Math.ceil(this.pos + n * this.schritt) + 1;

    if (this.muted || this.have < gebraucht) {
      L.fill(0); if (R !== L) { R.fill(0); }
      if (!this.muted) { this.starved++; }
      return false;
    }

    for (let i = 0; i < n; i++) {
      const p  = this.pos + i * this.schritt;
      const i0 = Math.floor(p);
      const f  = p - i0;
      const a  = (this.rd + i0) % this.cap;
      const b  = (this.rd + i0 + 1) % this.cap;
      L[i] = this.l[a] * (1 - f) + this.l[b] * f;
      if (R !== L) { R[i] = this.r[a] * (1 - f) + this.r[b] * f; }
    }
    // Ganze Quellproben freigeben, den Rest der Position mitnehmen — sonst
    // liefe die Phase bei nicht ganzzahligen Verhaeltnissen langsam weg.
    // (Bei 8 kHz gegen 48 kHz Ausgang ist das Verhaeltnis krumm.)
    const p2 = this.pos + n * this.schritt;
    const verbraucht = Math.floor(p2);
    this.pos = p2 - verbraucht;
    this.rd = (this.rd + verbraucht) % this.cap;
    this.have -= verbraucht;

    // Laeuft der Vorrat weit ueber das Ziel, hat das Netz einen Schub
    // geliefert — dann still ein Stueck ueberspringen, statt die Verzoegerung
    // mitzuschleppen.
    if (this.have > this.target * 4) {
      const skip = this.have - this.target;
      this.rd = (this.rd + skip) % this.cap;
      this.have -= skip;
    }
    return true;
  }
}

// In beiden Welten erreichbar machen: im Hauptfaden ist globalThis `window`,
// im Worklet der AudioWorkletGlobalScope.
globalThis.TonKern = TonKern;
