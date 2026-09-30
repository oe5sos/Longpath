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

    // ── Aufwaerts tasten ────────────────────────────────────────────────────
    //
    // Der Ton kommt in der Rate, die der Client mit dem Server ausgehandelt
    // hat — fuer ein Telefon 12 kHz statt 48, das allein ist Faktor vier.
    // Der Audio-Ausgang laeuft aber immer mit der Rate des Geraets. Also hier
    // hochtasten.
    //
    // Linear interpoliert, bewusst: bei 12 kHz liegt die Bandbreite bei 6 kHz
    // und SSB-Sprache endet bei 3 — die Spiegelungen, die eine lineare
    // Interpolation stehen laesst, liegen darueber und sind bei Sprache nicht
    // zu hoeren. Ein richtiger Filter waere schoener und ist die Sache wert,
    // sobald der Ton komprimiert ankommt; vorher nicht.
    this.srcRate = o.srcRate || sampleRate;
    this.schritt = this.srcRate / sampleRate;
    this.pos = 0;                          // Leseposition, gebrochen

    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'pcm') this._push(m.data, m.channels);
      else if (m.type === 'mute') this.muted = !!m.value;
      else if (m.type === 'flush') { this.w = this.rd = this.have = 0; this.pos = 0; }
      else if (m.type === 'rate') {
        // Die Rate darf sich mitten im Betrieb aendern (der Bediener schaltet
        // auf schmale Leitung). Puffer leeren statt die alten Proben mit dem
        // neuen Schritt zu lesen — das waere ein hoerbarer Sprung.
        this.srcRate = m.value || sampleRate;
        this.schritt = this.srcRate / sampleRate;
        this.w = this.rd = this.have = 0;
        this.pos = 0;
      }
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

    // Wie viele Quellproben braucht dieser Ausgabeblock? Bei 12 kHz Quelle
    // und 48 kHz Ausgang ein Viertel — plus eine fuer die Interpolation.
    const gebraucht = Math.ceil(this.pos + n * this.schritt) + 1;

    if (this.muted || this.have < gebraucht) {
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
    const p2 = this.pos + n * this.schritt;
    const verbraucht = Math.floor(p2);
    this.pos = p2 - verbraucht;
    this.rd = (this.rd + verbraucht) % this.cap;
    this.have -= verbraucht;

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
