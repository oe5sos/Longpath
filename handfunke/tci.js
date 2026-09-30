// tci.js — Longpath Handfunke: der Draht zum TCI-Server.
//
// Kein Bauwerkzeug, keine Abhaengigkeiten. Laeuft in Safari auf dem iPhone
// genauso wie im Pruefbrowser.
//
// Der Binaerrahmen ist byte-genau der aus TciBinaryFrame.h:
//   64-Byte-Kopf, 16 x uint32 little-endian
//   Offset  0 receiver · 4 sampleRate · 8 sampleType · 20 length · 24 streamType
//   sampleType 0=Int16 1=Int24 2=Int32 3=Float32
//   streamType 0=IQ 1=RX-Ton 2=TX-Ton 3=TX_CHRONO 4=Lineout
//   length zaehlt ALLE Werte des Rahmens, nicht je Kanal.
// Laut Kopfkommentar dort sind sampleRate und channels bei FREMDEN Servern
// unbrauchbar; Longpath fuellt sie korrekt. Wir verlassen uns trotzdem nicht
// darauf und nehmen die Raten aus dem Textkanal — dann laeuft die Seite auch
// gegen Thetis und ExpertSDR.

export const HDR = 64;

export class TciLink extends EventTarget {
  constructor() {
    super();
    this.ws = null;
    this.url = '';
    this.wanted = false;         // will der Bediener verbunden sein?
    this.retryMs = 1000;
    this.retryTimer = null;
    this.ready = false;          // ready; aus dem Init-Burst gesehen

    // Zustand, wie ihn der Server meldet. Nichts hiervon wird geraten:
    // jeder Wert kommt aus einer Zeile des Servers.
    this.st = {
      device: '', protocol: '',
      vfo: [[0, 0], [0, 0]],     // [trx][kanal]
      mode: ['', ''],
      filter: [[0, 0], [0, 0]],  // [trx][lo,hi]
      mox: false, tune: false, split: false,
      drive: null, tuneDrive: null,
      volume: null,              // dB, global
      rxVolume: [null, null],
      smeter: [null, null],
      txPower: null, txSwr: null,
      iqRate: null, audioRate: null,
      modes: [],
    };

    // Datenzaehler fuer die Fusszeile — was die Leitung wirklich kostet.
    this.bytes = { iq: 0, audio: 0, text: 0 };
    this._lastTick = 0;
    this.rate = { iq: 0, audio: 0, text: 0 };
  }

  // ── Verbindung ────────────────────────────────────────────────────────────
  connect(url) {
    this.url = url;
    this.wanted = true;
    this._open();
  }

  disconnect() {
    this.wanted = false;
    clearTimeout(this.retryTimer);
    if (this.ws) { try { this.ws.close(); } catch (e) {} }
    this.ws = null;
    this.ready = false;
    this._emit('state');
  }

  _open() {
    if (this.ws) { try { this.ws.close(); } catch (e) {} }
    let ws;
    try { ws = new WebSocket(this.url); }
    catch (e) { this._retry(); return; }
    ws.binaryType = 'arraybuffer';
    this.ws = ws;

    ws.onopen = () => {
      this.retryMs = 1000;
      this._emit('open');
    };
    ws.onclose = () => {
      this.ready = false;
      this._emit('state');
      this._retry();
    };
    ws.onerror = () => { /* onclose folgt ohnehin */ };
    ws.onmessage = (ev) => {
      if (typeof ev.data === 'string') {
        this.bytes.text += ev.data.length;
        // Ein Rahmen kann mehrere Befehle tragen, mit ';' getrennt.
        for (const part of ev.data.split(';')) {
          const line = part.trim();
          if (line) this._onText(line);
        }
      } else {
        this._onBinary(ev.data);
      }
    };
  }

  _retry() {
    if (!this.wanted) return;
    clearTimeout(this.retryTimer);
    this.retryTimer = setTimeout(() => this._open(), this.retryMs);
    // Sanft ansteigen, aber nie laenger als 10 s — ein Handy kommt aus dem
    // Ruhezustand zurueck und soll dann schnell wieder da sein.
    this.retryMs = Math.min(this.retryMs * 1.7, 10000);
  }

  send(cmd) {
    if (this.ws && this.ws.readyState === 1) {
      this.ws.send(cmd.endsWith(';') ? cmd : cmd + ';');
    }
  }

  // ── Textkanal ─────────────────────────────────────────────────────────────
  _onText(line) {
    const i = line.indexOf(':');
    const name = (i < 0 ? line : line.slice(0, i)).toLowerCase();
    const args = i < 0 ? [] : line.slice(i + 1).split(',').map(s => s.trim());
    const num = (k) => { const v = parseFloat(args[k]); return isNaN(v) ? null : v; };
    const int = (k) => { const v = parseInt(args[k], 10); return isNaN(v) ? null : v; };
    const bool = (k) => (args[k] || '').toLowerCase() === 'true';
    const s = this.st;

    switch (name) {
      case 'device':          s.device = args[0] || ''; break;
      case 'protocol':        s.protocol = args.join(','); break;
      case 'modulations_list': s.modes = args.filter(Boolean); break;
      case 'ready':           this.ready = true; this._emit('ready'); break;

      case 'vfo': {
        const t = int(0), ch = int(1), hz = int(2);
        if (t !== null && ch !== null && hz !== null && s.vfo[t]) s.vfo[t][ch] = hz;
        break;
      }
      case 'modulation': { const t = int(0); if (t !== null) s.mode[t] = args[1] || ''; break; }
      case 'rx_filter_band': {
        const t = int(0);
        if (t !== null && s.filter[t]) s.filter[t] = [int(1), int(2)];
        break;
      }
      case 'trx':   s.mox   = bool(1); break;
      case 'tune':  s.tune  = bool(1); break;
      case 'split_enable': s.split = bool(1); break;

      // Die drei, die bis 2026-09-30 fehlten (PR #145).
      case 'drive':       s.drive     = int(1); break;
      case 'tune_drive':  s.tuneDrive = int(1); break;

      case 'volume':      s.volume = num(0); break;
      case 'rx_volume':   { const t = int(0); if (t !== null) s.rxVolume[t] = num(2); break; }

      case 'iq_samplerate':     s.iqRate    = int(0); break;
      case 'audio_samplerate':  s.audioRate = int(0); break;

      // Messwerte. Longpath sendet sie laufend (Vorgabe alle 200 ms).
      case 'rx_sensors': { const t = int(0); if (t !== null) s.smeter[t] = num(1); break; }
      case 'tx_sensors': s.txPower = num(1); s.txSwr = num(2); break;
    }
    this._emit('state');
  }

  // ── Binaerkanal ───────────────────────────────────────────────────────────
  _onBinary(buf) {
    if (!buf || buf.byteLength < HDR) return;
    const h = new Uint32Array(buf, 0, 16);   // little-endian auf allen Zielen
    const receiver   = h[0];
    const sampleType = h[2];
    const length     = h[5];
    const streamType = h[6];

    const bps = sampleType === 0 ? 2 : sampleType === 1 ? 3 : 4;
    const payload = buf.byteLength - HDR;
    // Lieber einen Rahmen verwerfen als falsch deuten: sagt der Kopf mehr
    // Werte an, als Bytes da sind, stimmt etwas nicht.
    if (length * bps > payload) return;

    const vals = this._values(buf, sampleType, length);
    if (!vals) return;

    if (streamType === 0)      { this.bytes.iq += buf.byteLength;
                                 this._emit('iq', { receiver, vals }); }
    else if (streamType === 1) { this.bytes.audio += buf.byteLength;
                                 this._emit('audio', { receiver, vals }); }
    // 2/3/4 gehen uns als Empfaenger nichts an.
  }

  _values(buf, sampleType, length) {
    switch (sampleType) {
      case 3: return new Float32Array(buf, HDR, length);
      case 0: {
        const src = new Int16Array(buf, HDR, length);
        const out = new Float32Array(length);
        for (let i = 0; i < length; i++) out[i] = src[i] / 32768;
        return out;
      }
      case 2: {
        const src = new Int32Array(buf, HDR, length);
        const out = new Float32Array(length);
        for (let i = 0; i < length; i++) out[i] = src[i] / 2147483648;
        return out;
      }
      case 1: {
        // Int24, dicht gepackt, little-endian mit Vorzeichen.
        const b = new Uint8Array(buf, HDR, length * 3);
        const out = new Float32Array(length);
        for (let i = 0; i < length; i++) {
          const o = i * 3;
          let v = b[o] | (b[o + 1] << 8) | (b[o + 2] << 16);
          if (v & 0x800000) v -= 0x1000000;
          out[i] = v / 8388608;
        }
        return out;
      }
      default: return null;
    }
  }

  // ── Datenraten je Sekunde ─────────────────────────────────────────────────
  tickRates(now) {
    if (!this._lastTick) { this._lastTick = now; return this.rate; }
    const dt = (now - this._lastTick) / 1000;
    if (dt < 1) return this.rate;
    this.rate = {
      iq:    Math.round(this.bytes.iq    / dt / 1024),
      audio: Math.round(this.bytes.audio / dt / 1024),
      text:  Math.round(this.bytes.text  / dt / 1024),
    };
    this.bytes = { iq: 0, audio: 0, text: 0 };
    this._lastTick = now;
    return this.rate;
  }

  _emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }
}

// ── FFT, radix-2, in-place ───────────────────────────────────────────────────
// Das Telefon rechnet das nur, solange der Server kein fertiges Spektrum
// liefert. Sobald der schmale Spektrumrahmen steht, faellt das hier weg.
export class Fft {
  constructor(n) {
    this.n = n;
    this.cos = new Float32Array(n / 2);
    this.sin = new Float32Array(n / 2);
    for (let i = 0; i < n / 2; i++) {
      this.cos[i] = Math.cos(-2 * Math.PI * i / n);
      this.sin[i] = Math.sin(-2 * Math.PI * i / n);
    }
    // Blackman-Harris: schwache Traeger neben starken bleiben sichtbar.
    this.win = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const t = 2 * Math.PI * i / (n - 1);
      this.win[i] = 0.35875 - 0.48829 * Math.cos(t)
                  + 0.14128 * Math.cos(2 * t) - 0.01168 * Math.cos(3 * t);
    }
    this.rev = new Uint32Array(n);
    let bits = 0; while ((1 << bits) < n) bits++;
    for (let i = 0; i < n; i++) {
      let r = 0;
      for (let b = 0; b < bits; b++) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
      this.rev[i] = r;
    }
  }

  // iq: verschraenkt I,Q,I,Q… → Betrag in dB, fftshift (0 Hz in der Mitte)
  spectrum(iq, out) {
    const n = this.n, re = new Float32Array(n), im = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const j = this.rev[i], w = this.win[j];
      re[i] = iq[j * 2] * w;
      im[i] = iq[j * 2 + 1] * w;
    }
    for (let size = 2; size <= n; size <<= 1) {
      const half = size >> 1, step = n / size;
      for (let i = 0; i < n; i += size) {
        for (let j = i, k = 0; j < i + half; j++, k += step) {
          const c = this.cos[k], s = this.sin[k];
          const tr = re[j + half] * c - im[j + half] * s;
          const ti = re[j + half] * s + im[j + half] * c;
          re[j + half] = re[j] - tr; im[j + half] = im[j] - ti;
          re[j] += tr; im[j] += ti;
        }
      }
    }
    const half = n >> 1;
    for (let i = 0; i < n; i++) {
      const k = (i + half) % n;               // fftshift
      const p = re[k] * re[k] + im[k] * im[k];
      out[i] = 10 * Math.log10(p + 1e-20);
    }
    return out;
  }
}
