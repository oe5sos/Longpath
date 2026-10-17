// Der Sendeton vom Telefon: vom Mikrofon bis zum fertigen TCI-Rahmen.
//
// Stand 2026-10-04: **auch dieser Teil tastet nicht.** Es gibt hier keinen
// Weg, `trx:` oder `tune:` zu schicken, und das ist Absicht — dieselbe
// Reihenfolge wie bei tx-ton.js: erst alles bauen, was nicht tasten kann,
// dann das Tasten, und das erst nach ausdruecklicher Freigabe und erst an
// der Dummy-Last.
//
// Dass das trotzdem pruefbar ist, liegt am Server: Longpath nimmt TX-Ton nur
// vom Client an, der GERADE TASTET (`m_txAudioActiveClient`), und zaehlt
// alles andere als verworfen (`session->txFramesDropped`). Dieser Zaehler ist
// der Beleg, dass die ganze Kette steht, OHNE dass ein Watt die Antenne
// sieht.
//
// Zwei Teile in einer Datei, und die Trennung ist der Punkt:
//
//   `Sendestrecke` — reine Rechnung. Proben hinein, Rahmen heraus. Kein
//     Browser, kein Mikrofon, kein Netz. Darum ohne Geraet pruefbar
//     (pruefe-mikrofon.mjs), und genau dort sitzt alles, was falsch
//     rechnen koennte.
//   `Mikrofon` — die Anbindung an den Browser. Duenn gehalten, weil sie
//     sich nicht pruefen laesst.

import { TYP_MULAW8, wandleRate, baueTxRahmen } from './tx-ton.js';

/** Rate, in der Longpath den Sendeton am liebsten bekommt.
 *
 *  WDSPs TXA-Eingang laeuft mit 48 kHz. Wer etwas anderes schickt, zwingt
 *  den Server, einen Umsetzer anzulegen (TxChannel::feedTxAudioFromTci) —
 *  das geht, kostet aber Rechnung und eine Fehlerquelle. Das Telefon
 *  liefert ohnehin meist 48 kHz, dann ist die Wandlung hier ein Nullschritt.
 */
export const ZIEL_RATE = 48000;

/** Blocklaenge. 20 ms sind 50 Rahmen je Sekunde — fein genug, dass die
 *  Verzoegerung nicht auffaellt, grob genug, dass der Kopf (64 Byte) nicht
 *  ins Gewicht faellt: bei mu-law traegt ein Block 960 Nutzbytes. */
export const BLOCK_MS = 20;

/** Proben hinein, fertige TCI-Rahmen heraus — und nebenbei der Pegel.
 *
 *  Der Pegel ist kein Beiwerk: ohne ihn weiss am Telefon niemand, ob das
 *  Mikrofon ueberhaupt etwas hoert, und das ist die erste Frage, bevor
 *  irgendwann einmal getastet wird.
 */
export class Sendestrecke {
  constructor({ quellRate, zielRate = ZIEL_RATE, blockMs = BLOCK_MS,
                typ = TYP_MULAW8 } = {}) {
    this.quellRate = quellRate || zielRate;
    this.zielRate = zielRate;
    this.typ = typ;
    this.proBlock = Math.max(1, Math.round(zielRate * blockMs / 1000));
    this._vorrat = new Float32Array(this.proBlock * 4);
    this._haben = 0;
    this._wandel = { pos: 0, letzte: 0 };   // traegt den Rest ueber Blockgrenzen
    this.spitze = 0;      // groesster Betrag seit dem letzten Ablesen
    this.effektiv = 0;    // Effektivwert des letzten Blocks
    this.rahmen = 0;      // gebaute Rahmen, fuer die Selbstmeldung
  }

  /** Einen Block Mikrofonproben (-1..+1) hereingeben.
   *  Liefert die Rahmen, die dadurch voll geworden sind — oft keinen,
   *  manchmal zwei. */
  schiebe(proben) {
    if (!proben || !proben.length) { return []; }

    // Pegel VOR der Ratenwandlung: was das Mikrofon liefert, nicht was die
    // Wandlung daraus macht. Eine Uebersteuerung soll man dort sehen, wo sie
    // entsteht.
    for (let i = 0; i < proben.length; i++) {
      const a = Math.abs(proben[i]);
      if (a > this.spitze) { this.spitze = a; }
    }

    const um = wandleRate(proben, this.quellRate, this.zielRate, this._wandel);
    if (um.length) {
      // Der Vorrat muss mitwachsen koennen: ein Telefon liefert nicht immer
      // gleich grosse Bloecke, und ein zu kleiner Puffer wuerde still Proben
      // verlieren — der Sendeton klaenge dann zerhackt, und niemand wuesste
      // warum.
      if (this._haben + um.length > this._vorrat.length) {
        const groesser = new Float32Array(
          Math.max(this._vorrat.length * 2, this._haben + um.length));
        groesser.set(this._vorrat.subarray(0, this._haben));
        this._vorrat = groesser;
      }
      this._vorrat.set(um, this._haben);
      this._haben += um.length;
    }

    const raus = [];
    let gelesen = 0;
    while (this._haben - gelesen >= this.proBlock) {
      const block = this._vorrat.subarray(gelesen, gelesen + this.proBlock);
      let summe = 0;
      for (let i = 0; i < block.length; i++) { summe += block[i] * block[i]; }
      this.effektiv = Math.sqrt(summe / block.length);
      raus.push(baueTxRahmen(block, this.zielRate, this.typ));
      this.rahmen++;
      gelesen += this.proBlock;
    }
    if (gelesen) {
      this._vorrat.copyWithin(0, gelesen, this._haben);
      this._haben -= gelesen;
    }
    return raus;
  }

  /** Spitzenwert ablesen UND zuruecksetzen — so zeigt der Balken die
   *  Spitze seit dem letzten Bild, nicht die seit dem Einschalten. */
  spitzeAblesen() {
    const s = this.spitze;
    this.spitze = 0;
    return s;
  }

  /** Beim Loslassen: kein halber Block darf in die naechste Uebertragung
   *  hinuebergetragen werden. Der Rest ist aelter als das Loslassen. */
  leeren() {
    this._haben = 0;
    this._wandel = { pos: 0, letzte: 0 };
    this.spitze = 0;
    this.effektiv = 0;
  }
}

// ── Die Anbindung an den Browser ───────────────────────────────────────────

/** Gibt es hier ueberhaupt ein Mikrofon?
 *
 *  `getUserMedia` ist [SecureContext]. Ueber `http` auf einer LAN-Adresse —
 *  also genau dort, wo das Telefon die Seite holt — ist
 *  `navigator.mediaDevices` nicht leer, sondern `undefined`. Einen
 *  Ersatzweg wie beim ScriptProcessor gibt es nicht; dagegen hilft nur ein
 *  Zertifikat (handfunke/tls-einrichten.sh).
 *
 *  Die Seite muss das VORHER wissen, sonst bietet sie eine Taste an, die
 *  beim Druecken nichts tut und nichts erklaert.
 */
export function mikrofonMoeglich() {
  return !!(globalThis.navigator && navigator.mediaDevices
            && navigator.mediaDevices.getUserMedia);
}

export class Mikrofon {
  /** @param aufRahmen  wird je fertigem TCI-Rahmen gerufen */
  constructor(aufRahmen) {
    this.aufRahmen = aufRahmen;
    this.strecke = null;
    this.laeuft = false;
    this._ctx = null;
    this._strom = null;
    this._knoten = null;
    this.fehler = '';
  }

  async start() {
    if (this.laeuft) { return true; }
    if (!mikrofonMoeglich()) {
      this.fehler = 'kein sicherer Kontext — Zertifikat fehlt';
      return false;
    }
    try {
      // Alle drei Aufbereitungen AUS. Sie sind fuer Telefonie gebaut und
      // auf einer Funkstrecke schaedlich: die Echounterdrueckung schneidet
      // Silben weg, sobald der Empfaenger mithoert, die Rauschsperre frisst
      // leise Sprache, und die Verstaerkungsregelung pumpt gegen die
      // Sprachaufbereitung im Funkgeraet. Was geregelt wird, wird dort
      // geregelt.
      this._strom = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: false,
          channelCount: 1,
        },
      });
    } catch (e) {
      this.fehler = (e && e.name === 'NotAllowedError')
        ? 'Mikrofon abgelehnt'
        : ('Mikrofon nicht verfuegbar: ' + (e && e.name ? e.name : e));
      return false;
    }

    this._ctx = new (globalThis.AudioContext || globalThis.webkitAudioContext)();
    if (this._ctx.state === 'suspended') { await this._ctx.resume(); }
    this.strecke = new Sendestrecke({ quellRate: this._ctx.sampleRate });

    const quelle = this._ctx.createMediaStreamSource(this._strom);

    // Derselbe Rueckfall wie beim Empfangston: `AudioWorklet` ist
    // [SecureContext] — hier zwar immer gegeben (ohne sicheren Kontext kaeme
    // man gar nicht bis hierher), aber der Pfad bleibt, solange aeltere
    // Browser ihn brauchen. Die Rechnung steht in beiden Faellen in
    // `Sendestrecke`, nicht hier.
    const n = 2048;   // ~43 ms bei 48 kHz; die Strecke schneidet es auf 20 ms
    this._knoten = this._ctx.createScriptProcessor(n, 1, 1);
    this._knoten.onaudioprocess = (e) => {
      if (!this.laeuft) { return; }
      const rahmen = this.strecke.schiebe(e.inputBuffer.getChannelData(0));
      for (const r of rahmen) { this.aufRahmen(r); }
    };
    quelle.connect(this._knoten);
    // Ein ScriptProcessor ohne Ziel wird von Safari nicht getaktet — dieselbe
    // Falle wie beim Empfangston. Ein stummer Verstaerker reicht als Ziel und
    // bringt nichts auf die Lautsprecher.
    const stumm = this._ctx.createGain();
    stumm.gain.value = 0;
    this._knoten.connect(stumm);
    stumm.connect(this._ctx.destination);

    this.fehler = '';
    this.laeuft = true;
    return true;
  }

  stop() {
    this.laeuft = false;
    if (this.strecke) { this.strecke.leeren(); }
    if (this._knoten) { this._knoten.onaudioprocess = null; }
    try { this._knoten && this._knoten.disconnect(); } catch (e) {}
    if (this._strom) {
      for (const s of this._strom.getTracks()) { s.stop(); }
      this._strom = null;
    }
    // Der AudioContext bleibt stehen: ihn je Uebertragung neu aufzubauen
    // kostet am Telefon spuerbar Zeit, und die faellt genau zwischen
    // Tastendruck und erstem Ton.
    this._knoten = null;
  }
}

globalThis.MikrofonTeile = { Sendestrecke, Mikrofon, mikrofonMoeglich,
                             ZIEL_RATE, BLOCK_MS };
