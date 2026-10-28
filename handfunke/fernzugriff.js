/**
 * fernzugriff.js — die App von draussen, ohne das eigene Netz.
 *
 * Im eigenen WLAN spricht die App Longpaths TCI-Server direkt an. Von
 * unterwegs geht das nicht: die Adresse ist privat, und daran aendert kein
 * Wunsch etwas. Dieser Weg fuehrt ueber einen Rendezvous -- einen kleinen
 * Vermittlungsdienst, bei dem sich die Station anmeldet und unter dem ein
 * Telefon sie findet. Die Sitzung selbst laeuft NICHT ueber ihn: er
 * stellt nur vor, danach sucht sich WebRTC den Weg (im eigenen Netz
 * direkt, sonst ueber STUN, sonst ueber TURN).
 *
 * Massgeblich ist `docs/architecture/2026-09-23-rendezvous-v1.md` im
 * Upstream, Abschnitte 4.1, 4.4, 5 und 6.3. Wo diese Datei und das
 * Dokument sich widersprechen, hat das Dokument recht.
 *
 * **Was hier NICHT passiert: raten.** Die Gegenstelle laesst vor der
 * Anmeldung kein einziges TCI-Byte durch, und die Unterschrift dafuer
 * kommt aus einem Schluessel, den dieses Telefon nie herausgibt --
 * `extractable: false`, er lebt in IndexedDB und laesst sich auch von
 * einem Skript auf dieser Seite nicht auslesen.
 *
 * Nach aussen sieht `FernLeitung` aus wie ein WebSocket: `onopen`,
 * `onmessage`, `onclose`, `onerror`, `send`, `close`, `binaryType`,
 * `readyState`. Darum musste an `tci.js` fast nichts geaendert werden --
 * nur, woher die Leitung kommt.
 */

// ── Kleinkram ───────────────────────────────────────────────────────────────

export function b64url(roh) {
  const b = new Uint8Array(roh);
  let s = '';
  for (let i = 0; i < b.length; i++) { s += String.fromCharCode(b[i]); }
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

export function unb64url(text) {
  const s = text.replace(/-/g, '+').replace(/_/g, '/');
  const roh = atob(s + '='.repeat((4 - (s.length % 4)) % 4));
  const b = new Uint8Array(roh.length);
  for (let i = 0; i < roh.length; i++) { b[i] = roh.charCodeAt(i); }
  return b;
}

function verkette(...teile) {
  const n = teile.reduce((s, t) => s + t.length, 0);
  const aus = new Uint8Array(n);
  let o = 0;
  for (const t of teile) { aus.set(t, o); o += t.length; }
  return aus;
}

const TEXT = new TextEncoder();

// Die Praefixe sind Teil des Protokolls. Ein Tippfehler hier faellt erst
// auf, wenn die Station schweigt -- und Schweigen ist genau das, was sie
// auch einem Fremden gegenueber tut.
const PRAEFIX_INTRODUCE = TEXT.encode('NereusSDR introduce v1\n');
const PRAEFIX_ANMELDUNG = TEXT.encode('Longpath fernzugriff anmeldung v1\n');

const KANAL = 'longpath-tci';

// ── Der Geraeteschluessel ───────────────────────────────────────────────────

const DB = 'longpath-fernzugriff';
const LADEN = 'schluessel';

function db() {
  return new Promise((ok, weg) => {
    const a = indexedDB.open(DB, 1);
    a.onupgradeneeded = () => a.result.createObjectStore(LADEN);
    a.onsuccess = () => ok(a.result);
    a.onerror = () => weg(a.error);
  });
}

function ausLaden(d, schluessel) {
  return new Promise((ok, weg) => {
    const a = d.transaction(LADEN, 'readonly').objectStore(LADEN).get(schluessel);
    a.onsuccess = () => ok(a.result);
    a.onerror = () => weg(a.error);
  });
}

function inLaden(d, schluessel, wert) {
  return new Promise((ok, weg) => {
    const a = d.transaction(LADEN, 'readwrite').objectStore(LADEN).put(wert, schluessel);
    a.onsuccess = () => ok();
    a.onerror = () => weg(a.error);
  });
}

/**
 * Den Schluessel dieses Telefons holen, und beim ersten Mal erzeugen.
 *
 * `extractable: false` ist hier kein Zierrat: der Schluessel IST das
 * Telefon. Ein auslesbarer Schluessel in localStorage waere eine Kopie,
 * die jedes Skript auf dieser Seite mitnehmen kann.
 */
export async function geraeteschluessel() {
  const d = await db();
  let paar = await ausLaden(d, 'paar');
  if (!paar) {
    paar = await crypto.subtle.generateKey(
      { name: 'ECDSA', namedCurve: 'P-256' }, false, ['sign', 'verify']);
    await inLaden(d, 'paar', paar);
  }
  const spki = new Uint8Array(await crypto.subtle.exportKey('spki', paar.publicKey));
  const abdruck = new Uint8Array(await crypto.subtle.digest('SHA-256', spki));
  return {
    privat: paar.privateKey,
    spki,
    schluesselText: b64url(spki),   // was beim Koppeln uebergeben wird
    ident: b64url(abdruck),         // die Geraete-Id, 43 Zeichen
  };
}

/**
 * Unterschreiben. WebCrypto liefert ECDSA schon als rohes r||s (64 Byte)
 * -- genau das Format des Protokolls, kein DER-Umweg noetig.
 */
async function unterschreibe(privat, daten) {
  const sig = await crypto.subtle.sign(
    { name: 'ECDSA', hash: 'SHA-256' }, privat, daten);
  if (sig.byteLength !== 64) {
    throw new Error(`Unterschrift ist ${sig.byteLength} Byte lang, erwartet sind 64`);
  }
  return b64url(sig);
}

// ── Die Leitung ─────────────────────────────────────────────────────────────

/**
 * Sieht aus wie ein WebSocket, ist aber eine WebRTC-Verbindung durch den
 * Rendezvous.
 *
 * `url` ist der Dienst (`wss://…`), `stationId` die 26 Zeichen der
 * Station. Beides steht im Kopplungsblatt.
 */
export class FernLeitung extends EventTarget {
  constructor(dienstUrl, stationId) {
    super();
    this.dienstUrl = dienstUrl;
    this.stationId = stationId;
    this.binaryType = 'arraybuffer';
    this.readyState = 0;            // CONNECTING
    this.onopen = this.onmessage = this.onclose = this.onerror = null;
    this.weg = '';                  // 'direkt', 'stun' oder 'relay' -- fuer die Anzeige
    this._pc = null;
    this._kanal = null;
    this._ws = null;
    this._angemeldet = false;
    this._los();
  }

  async _los() {
    try {
      await this._verbinde();
    } catch (e) {
      this._fehler(e);
    }
  }

  async _verbinde() {
    const ich = await geraeteschluessel();
    const ws = new WebSocket(this.dienstUrl);
    this._ws = ws;
    ws.binaryType = 'arraybuffer';

    const hallo = await new Promise((ok, weg) => {
      ws.onmessage = (ev) => ok(JSON.parse(ev.data));
      ws.onerror = () => weg(new Error('der Vermittlungsdienst ist nicht erreichbar'));
      ws.onclose = () => weg(new Error('der Vermittlungsdienst hat zugemacht'));
    });
    if (hallo.type !== 'hello') { throw new Error(`erst kam ${hallo.type}, nicht hello`); }

    const stun = (hallo.stun || []).map((u) => ({ urls: u }));
    const pc = new RTCPeerConnection({ iceServers: stun, iceCandidatePoolSize: 0 });
    this._pc = pc;

    const kanal = pc.createDataChannel(KANAL, { ordered: true });
    kanal.binaryType = 'arraybuffer';
    this._kanal = kanal;
    this._verdrahte(kanal, ich);

    // Kandidaten einzeln nachreichen (trickle): ein Angebot, das auf das
    // Sammeln wartet, kostet im Mobilfunk mehrere Sekunden, bevor
    // ueberhaupt etwas losgeht.
    pc.onicecandidate = (ev) => {
      if (ws.readyState !== WebSocket.OPEN) { return; }
      ws.send(JSON.stringify({
        type: 'candidate',
        // Ein leerer Kandidat ist das Ende der Reihe und wird genauso
        // weitergereicht (Abschnitt 5.2).
        candidate: ev.candidate ? ev.candidate.candidate.replace(/^a=/, '') : '',
      }));
    };

    const angebot = await pc.createOffer();
    await pc.setLocalDescription(angebot);

    const transkript = verkette(
      PRAEFIX_INTRODUCE, TEXT.encode(this.stationId), unb64url(hallo.nonce));

    ws.onmessage = (ev) => this._vomDienst(JSON.parse(ev.data));
    ws.onclose = () => {
      // Sobald die Sitzung steht, ist der Dienst entbehrlich. Vorher ist
      // sein Wegfall das Ende des Versuchs.
      if (!this._angemeldet) { this._fehler(new Error('der Dienst ging weg')); }
    };
    ws.send(JSON.stringify({
      type: 'introduce',
      id: this.stationId,
      device: ich.ident,
      deviceSignature: await unterschreibe(ich.privat, transkript),
      offer: pc.localDescription.sdp,
    }));
  }

  async _vomDienst(m) {
    const pc = this._pc;
    if (m.type === 'answer') {
      // Die TURN-Zugangsdaten kommen hier an. Sie jetzt noch zu setzen
      // hat Wirkung: der Browser sammelt danach weiter und findet den
      // Relay-Weg, auch wenn direkt und STUN nichts ergeben haben.
      if (m.turn && m.turn.urls) {
        try {
          const kfg = pc.getConfiguration();
          kfg.iceServers = (kfg.iceServers || []).concat([{
            urls: m.turn.urls,
            username: m.turn.username,
            credential: m.turn.password,
          }]);
          pc.setConfiguration(kfg);
          pc.restartIce?.();
        } catch (e) {
          console.warn('TURN liess sich nicht nachtragen:', e);
        }
      }
      await pc.setRemoteDescription({ type: 'answer', sdp: m.answer });
    } else if (m.type === 'candidate') {
      if (m.candidate) {
        try { await pc.addIceCandidate({ candidate: m.candidate, sdpMLineIndex: 0 }); }
        catch (e) { console.warn('Kandidat verworfen:', e); }
      }
    } else if (m.type === 'error') {
      this._fehler(new Error(`${m.code}: ${m.reason || ''}`));
    } else if (m.type === 'introduction.end') {
      // Das ist der Normalfall, NACHDEM die Sitzung steht. Vorher heisst
      // es: die Station hat nicht geantwortet.
      if (!this._angemeldet) {
        this._fehler(new Error(m.code === 'expired'
          ? 'die Station hat nicht geantwortet — ist dieses Telefon dort gekoppelt?'
          : `die Einfuehrung endete: ${m.code}`));
      }
    }
  }

  _verdrahte(kanal, ich) {
    kanal.onmessage = async (ev) => {
      if (!this._angemeldet) {
        await this._anmelden(kanal, ich, ev.data);
        return;
      }
      this.onmessage?.({ data: ev.data });
    };
    kanal.onclose = () => this._zu(1006, 'der Kanal ging zu');
    kanal.onerror = (e) => this._fehler(e);
  }

  async _anmelden(kanal, ich, daten) {
    let m;
    try { m = JSON.parse(typeof daten === 'string' ? daten : ''); }
    catch (e) { return; }

    if (m.typ === 'anmelden') {
      const transkript = verkette(PRAEFIX_ANMELDUNG, unb64url(m.aufforderung));
      kanal.send(JSON.stringify({
        typ: 'anmeldung',
        signatur: await unterschreibe(ich.privat, transkript),
      }));
    } else if (m.typ === 'offen') {
      this._angemeldet = true;
      this.readyState = 1;          // OPEN
      this.weg = await this._welcherWeg();
      // Der Dienst hat seine Arbeit getan. Ihn jetzt zu schliessen ist
      // ausdruecklich vorgesehen (Abschnitt 6.3) -- die Station bekommt
      // `clientLeft` und laesst die Sitzung stehen.
      try { this._ws.close(); } catch (e) {}
      this.onopen?.({});
    } else if (m.typ === 'fehler') {
      this._fehler(new Error(m.grund || 'die Station sagt ab'));
    }
  }

  /** Welchen Weg ICE genommen hat — fuer die Fusszeile, nicht fuer die Technik. */
  async _welcherWeg() {
    try {
      const bericht = await this._pc.getStats();
      let paar = null;
      bericht.forEach((s) => {
        if (s.type === 'candidate-pair' && s.state === 'succeeded' && s.nominated) { paar = s; }
      });
      if (!paar) { return ''; }
      let art = '';
      bericht.forEach((s) => {
        if (s.id === paar.localCandidateId) { art = s.candidateType; }
      });
      return { host: 'direkt', srflx: 'ueber STUN', prflx: 'ueber STUN', relay: 'ueber Relay' }[art] || art;
    } catch (e) { return ''; }
  }

  // ── Wie ein WebSocket ─────────────────────────────────────────────────────

  send(daten) {
    if (this.readyState !== 1) { throw new Error('die Leitung ist nicht offen'); }
    this._kanal.send(daten);
  }

  close() { this._zu(1000, 'geschlossen'); }

  _fehler(e) {
    console.warn('Fernzugriff:', e && e.message ? e.message : e);
    this.onerror?.(e);
    this._zu(1006, (e && e.message) || 'Fehler');
  }

  _zu(code, grund) {
    if (this.readyState === 3) { return; }
    this.readyState = 3;            // CLOSED
    try { this._kanal?.close(); } catch (e) {}
    try { this._pc?.close(); } catch (e) {}
    try { this._ws?.close(); } catch (e) {}
    this.onclose?.({ code, reason: grund });
  }
}

/**
 * Eine Leitung oeffnen — im eigenen Netz ein WebSocket, von draussen der
 * Weg ueber den Rendezvous.
 *
 * Die Fernadresse sieht so aus:
 *   fern:wss://rv.example.com/#lwhu2kyjnrfdwvpxvdckao3lb7
 * also Dienst und Station-Id, durch `#` getrennt. Mehr steht nicht im
 * Kopplungsblatt, und mehr braucht es nicht: die Station ist unter ihrer
 * Id erreichbar, wo auch immer sie gerade steht.
 */
export function oeffneLeitung(url) {
  if (!url.startsWith('fern:')) { return new WebSocket(url); }
  const rest = url.slice('fern:'.length);
  const teiler = rest.lastIndexOf('#');
  if (teiler < 0) { throw new Error('der Fernadresse fehlt die Stations-Id nach #'); }
  return new FernLeitung(rest.slice(0, teiler), rest.slice(teiler + 1));
}
