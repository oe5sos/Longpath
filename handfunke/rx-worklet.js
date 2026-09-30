// rx-worklet.js — Empfangston aus dem TCI-Strom in die Lautsprecher.
//
// Warum ein Worklet und kein ScriptProcessor: der Ton laeuft hier im
// Audio-Faden, nicht im Hauptfaden. Waehrend das Telefon einen Wasserfall
// zeichnet, darf der Ton nicht stottern.
//
// Der Ringpuffer haelt absichtlich wenig: eine Fernbedienung, die eine halbe
// Sekunde hinterherhinkt, ist zum Mithoeren beim Drehen unbrauchbar. Laeuft
// er ueber, werfen wir die AELTESTEN Proben weg, nicht die neuesten — sonst
// klebt die Verzoegerung fest, statt sich abzubauen.

class RxWorklet extends AudioWorkletProcessor {
  constructor(opts) {
    super();
    const o = (opts && opts.processorOptions) || {};
    this.cap = o.capacity || 48000;        // 1 s bei 48 kHz, je Kanal
    this.l = new Float32Array(this.cap);
    this.r = new Float32Array(this.cap);
    this.w = 0; this.rd = 0; this.have = 0;
    this.target = o.target || 2400;        // ~50 ms Sollvorrat
    this.muted = false;
    this.starved = 0;

    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'pcm') this._push(m.data, m.channels);
      else if (m.type === 'mute') this.muted = !!m.value;
      else if (m.type === 'flush') { this.w = this.rd = this.have = 0; }
    };
  }

  _push(vals, channels) {
    const frames = channels === 2 ? (vals.length >> 1) : vals.length;
    // Ueberlauf: aelteste Proben fallen lassen, damit die Verzoegerung
    // schrumpft statt zu bleiben.
    const room = this.cap - this.have;
    if (frames > room) {
      const drop = frames - room;
      this.rd = (this.rd + drop) % this.cap;
      this.have -= drop;
    }
    for (let i = 0; i < frames; i++) {
      const w = (this.w + i) % this.cap;
      if (channels === 2) { this.l[w] = vals[i * 2]; this.r[w] = vals[i * 2 + 1]; }
      else                { this.l[w] = vals[i];     this.r[w] = vals[i]; }
    }
    this.w = (this.w + frames) % this.cap;
    this.have = Math.min(this.cap, this.have + frames);
  }

  process(_in, outputs) {
    const out = outputs[0];
    const n = out[0].length;
    const L = out[0], R = out[1] || out[0];

    if (this.muted || this.have < n) {
      // Kein Vorrat: Stille statt Knacken. Wir zaehlen es mit, damit die
      // Seite den Puffer nachregeln kann, statt blind zu raten.
      L.fill(0); if (R !== L) R.fill(0);
      if (!this.muted) {
        this.starved++;
        if ((this.starved & 31) === 0) {
          this.port.postMessage({ type: 'starved', have: this.have });
        }
      }
      return true;
    }

    for (let i = 0; i < n; i++) {
      const r = (this.rd + i) % this.cap;
      L[i] = this.l[r]; if (R !== L) R[i] = this.r[r];
    }
    this.rd = (this.rd + n) % this.cap;
    this.have -= n;

    // Laeuft der Vorrat weit ueber das Ziel, hat das Netz einen Schub
    // geliefert — dann still ein Stueck ueberspringen, statt die
    // Verzoegerung mitzuschleppen.
    if (this.have > this.target * 4) {
      const skip = this.have - this.target;
      this.rd = (this.rd + skip) % this.cap;
      this.have -= skip;
    }
    return true;
  }
}

registerProcessor('rx-worklet', RxWorklet);
