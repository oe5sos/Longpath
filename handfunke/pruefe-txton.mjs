#!/usr/bin/env node
// Prüfstand für den Sendeton-Rahmenbau — ohne Browser, ohne Funkgerät, ohne
// jede Möglichkeit zu tasten.
//
// Die wichtigste Prüfung ist die letzte: mu-law hin und wieder zurück. Die
// Kodierung steht in tx-ton.js, die Dekodierung in tci.js, und sie sind
// voneinander unabhängig geschrieben. Stimmen sie nicht überein, klingt der
// Sendeton verzerrt — und das merkt man erst auf der Luft, also am teuersten
// möglichen Ort.
//
//     node pruefe-txton.mjs

import { readFileSync } from 'node:fs';
import { HDR, STROM_TX_AUDIO, TYP_INT16, TYP_MULAW8,
         mulawAus, wandleRate, baueTxRahmen } from './tx-ton.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

// Die Dekodiertabelle aus tci.js nachbauen — Zeile für Zeile dieselbe
// Rechnung, damit der Vergleich etwas wert ist.
const MULAW_ZURUECK = new Float32Array(256);
for (let u = 0; u < 256; u++) {
  const inv = ~u & 0xFF;
  const vorzeichen = (inv & 0x80) ? -1 : 1;
  const segment = (inv >> 4) & 0x07;
  const mantisse = inv & 0x0F;
  let betrag = ((mantisse << 3) + 0x84) << segment;
  betrag -= 0x84;
  MULAW_ZURUECK[u] = vorzeichen * betrag / 32768;
}

console.log('Sendeton — Rahmenbau\n');

// 1. Der Kopf muss stehen, wie der Server ihn liest.
{
  const proben = new Float32Array(240);
  const buf = baueTxRahmen(proben, 12000, TYP_MULAW8);
  const k = new Uint32Array(buf, 0, 16);
  pruefe('Kopf: Empfaenger 0', k[0] === 0);
  pruefe('Kopf: Abtastrate 12000', k[1] === 12000, `${k[1]}`);
  pruefe('Kopf: Abtastart mu-law (101)', k[2] === TYP_MULAW8, `${k[2]}`);
  pruefe('Kopf: flache Wertezahl', k[5] === 240, `${k[5]}`);
  pruefe('Kopf: Stromart TX-Audio (2)', k[6] === STROM_TX_AUDIO, `${k[6]}`);
  pruefe('Kopf: EIN Kanal', k[7] === 1, `${k[7]}`);
  pruefe('Groesse = 64 + 240 Byte', buf.byteLength === HDR + 240,
         `${buf.byteLength}`);
}

// 2. Int16 belegt zwei Byte je Wert — sonst deutet der Server die Nutzlast
//    falsch und verwirft den Rahmen still.
{
  const buf = baueTxRahmen(new Float32Array(100), 48000, TYP_INT16);
  pruefe('Int16: 64 + 200 Byte', buf.byteLength === HDR + 200,
         `${buf.byteLength}`);
}

// 3. Vollausschlag darf nicht umklappen. Ein Ueberlauf klingt als Knacken.
{
  const proben = Float32Array.from([1.5, -1.5, 1.0, -1.0]);
  const buf = baueTxRahmen(proben, 12000, TYP_INT16);
  const aus = new Int16Array(buf, HDR, 4);
  pruefe('Vollausschlag wird begrenzt, nicht umgeklappt',
         aus[0] === 32767 && aus[1] === -32768 && aus[2] === 32767,
         [...aus].join(','));
}

// 4. Die Ratenwandlung muss die Zahl der Werte treffen und an der
//    Blockgrenze nicht springen.
{
  const z = { pos: 0, letzte: 0 };
  let gesamt = 0;
  for (let i = 0; i < 100; i++) {
    const block = new Float32Array(480);          // 10 ms bei 48 kHz
    gesamt += wandleRate(block, 48000, 12000, z).length;
  }
  // 100 × 10 ms bei 12 kHz = 12000 Werte, ein paar Werte Spiel fuer den Rest.
  pruefe('48 -> 12 kHz: Wertezahl stimmt', Math.abs(gesamt - 12000) <= 4,
         `${gesamt} statt ~12000`);
}

// 5. Das eigentliche Stück: mu-law hin und zurück. Verglichen wird gegen die
//    Dekodierung, die tci.js benutzt — die beiden müssen zusammenpassen,
//    sonst klingt der Sendeton verzerrt.
{
  let schlimmster = 0;
  let summe = 0, n = 0;
  for (let i = -1000; i <= 1000; i++) {
    const f = i / 1000;
    const zurueck = MULAW_ZURUECK[mulawAus(f)];
    const fehlerF = Math.abs(zurueck - f);
    schlimmster = Math.max(schlimmster, fehlerF);
    summe += fehlerF * fehlerF; n++;
  }
  const rms = Math.sqrt(summe / n);
  const stoerabstand = 20 * Math.log10(0.577 / rms);   // RMS eines Sägezahns
  console.log(`        groesster Fehler ${schlimmster.toFixed(4)}, `
              + `Stoerabstand ${stoerabstand.toFixed(1)} dB`);
  // mu-law haelt ueber den ganzen Bereich rund 38 dB; mehr als 0,05
  // Einzelfehler waere eine kaputte Kennlinie, nicht Quantisierung.
  pruefe('mu-law hin und zurueck bleibt dicht', schlimmster < 0.05,
         `groesster Fehler ${schlimmster}`);
  pruefe('mu-law: Stoerabstand ueber 30 dB', stoerabstand > 30,
         `${stoerabstand.toFixed(1)} dB`);
}

// 6. Null bleibt Null. Ein Gleichanteil im Sendeton waere ein Traeger.
{
  const buf = baueTxRahmen(new Float32Array(8), 12000, TYP_MULAW8);
  const aus = new Uint8Array(buf, HDR, 8);
  const zurueck = [...aus].map(u => MULAW_ZURUECK[u]);
  pruefe('Stille bleibt still', zurueck.every(v => Math.abs(v) < 0.001),
         zurueck.join(','));
}

// 7. Die Zusage, auf die es sicherheitshalber ankommt: in dieser Datei gibt
//    es keinen Weg zu tasten.
{
  const quelle = readFileSync(new URL('./tx-ton.js', import.meta.url), 'utf8');
  const taster = /\btrx\s*:|\btune\s*:/.test(quelle.replace(/\/\/[^\n]*/g, ''));
  pruefe('tx-ton.js kann nicht tasten', !taster,
         'es steht ein trx:/tune: im Code');
}

console.log(fehler === 0 ? '\nalles gruen' : `\n${fehler} Pruefung(en) gescheitert`);
process.exit(fehler === 0 ? 0 : 1);
