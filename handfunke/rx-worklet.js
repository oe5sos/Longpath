// rx-worklet.js — Empfangston aus dem TCI-Strom in die Lautsprecher.
//
// Warum ein Worklet und kein ScriptProcessor: der Ton laeuft hier im
// Audio-Faden, nicht im Hauptfaden. Waehrend das Telefon einen Wasserfall
// zeichnet, darf der Ton nicht stottern.
//
// Aber: `AudioWorklet` ist [SecureContext] und faellt auf einer LAN-Adresse
// ueber http komplett aus — am 2026-09-30 gemessen. Deshalb gibt es in
// app.js einen Rueckfall auf ScriptProcessorNode, der DIESELBE Rechnung
// benutzt: sie steht in ton-kern.js, nicht hier. Diese Datei ist nur noch
// die Huelle, die den Kern an den Audio-Faden anschliesst.
//
// ton-kern.js muss VOR dieser Datei per addModule geladen werden — mehrere
// addModule-Aufrufe teilen sich den globalen Gueltigkeitsbereich des
// Worklets, also steht `TonKern` dann hier bereit.

class RxWorklet extends AudioWorkletProcessor {
  constructor(opts) {
    super();
    const o = (opts && opts.processorOptions) || {};
    this.kern = new TonKern({
      ausgabeRate: sampleRate,            // die echte Rate des Ausgangs
      srcRate: o.srcRate || sampleRate,   // die mit dem Server ausgehandelte
      vorratMs: o.vorratMs,
      deckelMs: o.deckelMs,
    });

    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'pcm') { this.kern.push(m.data, m.channels); }
      else if (m.type === 'mute') { this.kern.muted = !!m.value; }
      else if (m.type === 'flush') { this.kern.leeren(); }
      else if (m.type === 'rate') { this.kern.setRate(m.value); }
    };
  }

  process(_in, outputs) {
    const out = outputs[0];
    const L = out[0], R = out[1] || out[0];
    const voll = this.kern.zieh(L, R, L.length);
    // Leerlauf melden, damit die Seite den Vorrat nachregeln kann statt blind
    // zu raten — aber nicht bei jedem Block, das waere ein Nachrichtensturm.
    if (!voll && !this.kern.muted && (this.kern.starved & 31) === 0) {
      this.port.postMessage({ type: 'starved', have: this.kern.have });
    }
    return true;
  }
}

registerProcessor('rx-worklet', RxWorklet);
