// Sendeton vom Telefon: aus Mikrofonproben werden TCI-Binärrahmen.
//
// Stand 2026-10-04: **dieser Teil tastet nicht.** Es gibt hier keinen Weg,
// `trx:` oder `tune:` zu schicken, und das ist Absicht — der Entwurf
// (docs/architecture/2026-10-04-handfunke-senden-und-loggen.md) sieht genau
// diese Reihenfolge vor: erst alles bauen, was nicht tasten kann, dann das
// Tasten, und das erst nach ausdrücklicher Freigabe und erst an der
// Dummy-Last. Die Seite hat am 2026-09-30 einmal `tune:N,true` geschickt —
// ein Dauerträger mit voller Leistung, der nur nicht „senden" hieß.
//
// Wirkung am echten Server hat das hier ohnehin nicht: Longpath nimmt
// TX-Ton nur vom Client an, der GERADE TASTET (`m_txAudioActiveClient`,
// TciServer.cpp), und zusätzlich nur, wenn `TciAllowRemoteTx` freigegeben
// ist. Ohne beides zählt der Server die Rahmen als verworfen und tut sonst
// nichts. Das ist kein Zufall, sondern die Sperre, auf die man sich verlässt.
//
// Die Datei ist bewusst rein rechnend und ohne Browser prüfbar
// (pruefe-txton.mjs). Die Mikrofonaufnahme selbst kommt später und benutzt
// genau diese Bausteine.

/** Kopfgröße eines TCI-Binärrahmens, wie überall in diesem Protokoll. */
export const HDR = 64;

/** Stromarten, so wie der Server sie liest (TciBinaryFrame.h). */
export const STROM_TX_AUDIO = 2;

/** Abtastarten. */
export const TYP_INT16  = 0;
export const TYP_MULAW8 = 101;

// ── mu-law nach ITU-T G.711 ────────────────────────────────────────────────
//
// Die Gegenrichtung (Dekodierung) steht in tci.js. Hier die Kodierung, und
// sie ist die Umkehrung davon — dieselbe 0x84-Verschiebung, dieselben acht
// Segmente. Wer eine der beiden ändert, muss die andere mitändern; der
// Prüfstand fährt beide gegeneinander.
const MU_CLIP = 32635;
const MU_BIAS = 0x84;

/** Eine Abtastung (-1..+1) nach mu-law. */
export function mulawAus(f) {
  let s = Math.round(f * 32768);
  if (s > 32767) { s = 32767; }
  if (s < -32768) { s = -32768; }
  let vorzeichen = 0;
  if (s < 0) { s = -s; vorzeichen = 0x80; }
  if (s > MU_CLIP) { s = MU_CLIP; }
  s += MU_BIAS;
  // Segment = Lage des höchsten gesetzten Bits ab Bit 7.
  let segment = 0;
  for (let w = 0x4000; w >= 0x100 && segment < 7; w >>= 1) {
    if (s >= w) { break; }
    segment++;
  }
  segment = 7 - segment;
  const mantisse = (s >> (segment + 3)) & 0x0F;
  return (~(vorzeichen | (segment << 4) | mantisse)) & 0xFF;
}

/** Lineare Abtastratenwandlung mit gebrochenem Schritt.
 *
 *  Reicht hier: der Sendeton ist Sprache in 12 kHz Bandbreite, und die
 *  Quelle (das Telefon) liefert 48 kHz. Ein Polyphasenfilter wäre sauberer
 *  und ist der Aufwand nicht wert, solange es um Verständlichkeit geht und
 *  nicht um Messtechnik.
 *
 *  `rest` trägt die gebrochene Leseposition in den nächsten Block — ohne das
 *  entsteht an jeder Blockgrenze ein kleiner Sprung, und bei 50 Blöcken je
 *  Sekunde hört man das als Rauen.
 */
export function wandleRate(quelle, vonRate, nachRate, zustand) {
  if (vonRate === nachRate) { return quelle; }
  const schritt = vonRate / nachRate;
  const z = zustand || { pos: 0, letzte: 0 };
  const anzahl = Math.floor((quelle.length - z.pos) / schritt);
  const aus = new Float32Array(Math.max(0, anzahl));
  let p = z.pos;
  for (let i = 0; i < aus.length; i++) {
    const i0 = Math.floor(p);
    const f = p - i0;
    const a = i0 < 0 ? z.letzte : quelle[i0];
    const b = quelle[Math.min(i0 + 1, quelle.length - 1)];
    aus[i] = a * (1 - f) + b * f;
    p += schritt;
  }
  z.letzte = quelle.length ? quelle[quelle.length - 1] : z.letzte;
  z.pos = p - quelle.length;   // Rest für den nächsten Block
  return aus;
}

/** Baut einen TCI-Binärrahmen aus Abtastungen (-1..+1).
 *
 *  `length` im Kopf ist die FLACHE Zahl aller Werte (Abtastungen × Kanäle),
 *  nicht je Kanal — so liest der Server es (TciBinaryFrame.h, Offset 20).
 *  Hier ist der Sendeton immer einkanalig: ein Mikrofon.
 */
export function baueTxRahmen(proben, abtastrate, typ = TYP_MULAW8, empfaenger = 0) {
  const n = proben.length;
  const bps = (typ === TYP_MULAW8) ? 1 : 2;
  const puffer = new ArrayBuffer(HDR + n * bps);
  const kopf = new Uint32Array(puffer, 0, 16);
  kopf[0] = empfaenger >>> 0;      //  0 Empfänger
  kopf[1] = abtastrate >>> 0;      //  4 Abtastrate
  kopf[2] = typ >>> 0;             //  8 Abtastart
  kopf[5] = n >>> 0;               // 20 flache Zahl der Werte
  kopf[6] = STROM_TX_AUDIO >>> 0;  // 24 Stromart
  kopf[7] = 1;                     // 28 Kanäle — ein Mikrofon
  if (typ === TYP_MULAW8) {
    const aus = new Uint8Array(puffer, HDR, n);
    for (let i = 0; i < n; i++) { aus[i] = mulawAus(proben[i]); }
  } else {
    const aus = new Int16Array(puffer, HDR, n);
    for (let i = 0; i < n; i++) {
      let s = Math.round(proben[i] * 32768);
      if (s > 32767) { s = 32767; }
      if (s < -32768) { s = -32768; }
      aus[i] = s;
    }
  }
  return puffer;
}

// In beiden Welten erreichbar machen, wie bei ton-kern.js: im Hauptfaden ist
// globalThis `window`, im Prüfstand der Node-Gültigkeitsbereich.
globalThis.TxTon = { HDR, STROM_TX_AUDIO, TYP_INT16, TYP_MULAW8,
                     mulawAus, wandleRate, baueTxRahmen };
