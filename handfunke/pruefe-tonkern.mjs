#!/usr/bin/env node
// Prüfstand für den Ringpuffer der Handfunke — ohne Browser, ohne Funkgerät.
//
// Geprüft wird die Zusage des sanften Gleichlaufs: läuft die Uhr des Geräts
// gegen die der Tonkarte, muss sich der Vorrat auf das Ziel einpendeln statt
// davonzulaufen.
//
// Anlass: Martins Selbstmeldung vom 2026-10-04, 07:58 — `vorrat=4769` gegen
// `ziel=1440`, also rund 280 ms Verzögerung mehr als gewollt. Die Notbremse
// (über dem Vierfachen überspringen) hatte nicht gegriffen, weil 4769 unter
// 5760 liegt. Zwischen Ziel und Vierfachem passierte vorher nichts.
//
//     node pruefe-tonkern.mjs

import { readFileSync } from 'node:fs';

// ton-kern.js setzt globalThis.TonKern — in Node genauso wie im Worklet.
new Function(readFileSync(new URL('./ton-kern.js', import.meta.url), 'utf8'))();
const { TonKern } = globalThis;

let fehler = 0;
function pruefe(name, bedingung, text) {
  if (bedingung) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}: ${text}`); fehler++; }
}

/** Lässt den Ring laufen: Quelle schreibt `driftPpm` schneller als die Senke
 *  liest, und wir sehen nach, wo der Vorrat landet. */
function lauf({ driftPpm, sekunden, srcRate = 12000, ausgabeRate = 48000,
                maxZugRel }) {
  const k = new TonKern({ srcRate, ausgabeRate, vorratMs: 120,
                          ...(maxZugRel !== undefined ? { maxZugRel } : {}) });
  const block = 128;                       // Ausgabeproben je Runde
  const runden = Math.round(sekunden * ausgabeRate / block);
  const L = new Float32Array(block), R = new Float32Array(block);

  // Quellproben je Runde, inklusive Drift. Gebrochene Reste werden
  // mitgeschleppt, sonst rundet man die Drift weg.
  const proSoll = block * srcRate / ausgabeRate;
  let rest = 0;
  const verlauf = [];
  for (let i = 0; i < runden; i++) {
    const genau = proSoll * (1 + driftPpm / 1e6) + rest;
    const n = Math.floor(genau);
    rest = genau - n;
    if (n > 0) {
      const s = new Float32Array(n * 2);   // verschränkt L/R, Inhalt egal
      k.push(s, 2);
    }
    k.zieh(L, R, block);
    if (i % Math.round(runden / 20) === 0) { verlauf.push(k.have); }
  }
  return { have: k.have, target: k.target, starved: k.starved, verlauf };
}

console.log('TonKern — sanfter Gleichlauf\n');

// 1. Ohne Drift bleibt der Vorrat beim Ziel.
{
  const r = lauf({ driftPpm: 0, sekunden: 30 });
  pruefe('ohne Drift bleibt der Vorrat beim Ziel',
         Math.abs(r.have - r.target) < r.target * 0.5,
         `have=${r.have} target=${r.target}`);
  pruefe('ohne Drift kein Leerlauf', r.starved === 0, `starved=${r.starved}`);
}

// 2. Das eigentliche Stück: die Quelle läuft 50 ppm schneller. Ohne
//    Gleichlauf würde der Vorrat in 30 s um 12000*0.00005*30 = 18 Proben
//    wachsen — das allein wäre harmlos. Der beobachtete Fall war gröber:
//    ein Schub, der liegenblieb. Also beides prüfen.
{
  const r = lauf({ driftPpm: 50, sekunden: 60 });
  pruefe('mit 50 ppm Drift bleibt der Vorrat am Ziel',
         Math.abs(r.have - r.target) < r.target * 0.6,
         `have=${r.have} target=${r.target} verlauf=${r.verlauf.join(',')}`);
}

// 3. Ein Schub (zu viel im Ring) muss sich abbauen — ohne Sprung.
{
  const srcRate = 12000, ausgabeRate = 48000, block = 128;
  const k = new TonKern({ srcRate, ausgabeRate, vorratMs: 120 });
  const L = new Float32Array(block), R = new Float32Array(block);
  // Anlauf abschliessen
  k.push(new Float32Array(k.target * 2), 2);
  k.zieh(L, R, block);
  // Schub: das Dreifache des Ziels hineinlegen (wie am 2026-10-04 gemessen)
  const schub = k.target * 3 - k.have;
  k.push(new Float32Array(Math.max(0, schub) * 2), 2);
  const vorher = k.have;
  pruefe('Schub liegt unter der Notbremse (sonst prueft das den Sprung)',
         vorher < k.target * 4, `have=${vorher} bremse=${k.target * 4}`);

  // 60 s laufen lassen, Quelle genau im Takt.
  const proSoll = block * srcRate / ausgabeRate;
  let rest = 0;
  const runden = Math.round(60 * ausgabeRate / block);
  for (let i = 0; i < runden; i++) {
    const genau = proSoll + rest;
    const n = Math.floor(genau); rest = genau - n;
    if (n > 0) { k.push(new Float32Array(n * 2), 2); }
    k.zieh(L, R, block);
  }
  pruefe('ein Schub baut sich ab', k.have < vorher * 0.6,
         `vorher=${vorher} nachher=${k.have} ziel=${k.target}`);
  pruefe('und ueberschiesst nicht in den Leerlauf', k.starved === 0,
         `starved=${k.starved}`);
}

// 4. Gegenprobe: ohne Gleichlauf (maxZugRel = 0) bleibt der Schub stehen.
//    Ohne diese Haelfte sagt Nummer 3 nichts aus.
{
  const srcRate = 12000, ausgabeRate = 48000, block = 128;
  const k = new TonKern({ srcRate, ausgabeRate, vorratMs: 120, maxZugRel: 0 });
  const L = new Float32Array(block), R = new Float32Array(block);
  k.push(new Float32Array(k.target * 2), 2);
  k.zieh(L, R, block);
  const schub = k.target * 3 - k.have;
  k.push(new Float32Array(Math.max(0, schub) * 2), 2);
  const vorher = k.have;
  const proSoll = block * srcRate / ausgabeRate;
  let rest = 0;
  const runden = Math.round(60 * ausgabeRate / block);
  for (let i = 0; i < runden; i++) {
    const genau = proSoll + rest;
    const n = Math.floor(genau); rest = genau - n;
    if (n > 0) { k.push(new Float32Array(n * 2), 2); }
    k.zieh(L, R, block);
  }
  pruefe('GEGENPROBE: ohne Gleichlauf bleibt der Schub stehen',
         k.have > vorher * 0.9,
         `vorher=${vorher} nachher=${k.have} — dann faengt Nummer 3 nichts`);
}

console.log(fehler === 0 ? '\nalles gruen' : `\n${fehler} Pruefung(en) gescheitert`);
process.exit(fehler === 0 ? 0 : 1);
