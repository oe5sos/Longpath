#!/usr/bin/env node
// Pruefstand fuer die Sendestrecke — ohne Browser, ohne Mikrofon, ohne jede
// Moeglichkeit zu tasten.
//
// Geprueft wird das, was still falsch sein kann: ein Vorrat, der ueberlaeuft
// und Proben verliert (der Sendeton klaenge zerhackt, und niemand wuesste
// warum), eine Ratenwandlung, die an jeder Blockgrenze springt, ein Kopf,
// den der Server anders liest als wir ihn schreiben.
//
//     node pruefe-mikrofon.mjs

import { Sendestrecke, ZIEL_RATE, BLOCK_MS } from './mikrofon.js';
import { HDR, STROM_TX_AUDIO, TYP_MULAW8, TYP_INT16 } from './tx-ton.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

const kopfVon = (puffer) => {
  const k = new Uint32Array(puffer, 0, 16);
  return { empf: k[0], rate: k[1], typ: k[2], anzahl: k[5],
           strom: k[6], kanaele: k[7] };
};

const sinus = (n, hz, rate, amp = 0.5) => {
  const a = new Float32Array(n);
  for (let i = 0; i < n; i++) { a[i] = amp * Math.sin(2 * Math.PI * hz * i / rate); }
  return a;
};

console.log('Sendestrecke\n');

// 1. Blockgroesse und Kopf — so wie TciServer.cpp sie liest.
{
  const s = new Sendestrecke({ quellRate: ZIEL_RATE });
  const proBlock = ZIEL_RATE * BLOCK_MS / 1000;
  pruefe('20 ms sind 960 Proben bei 48 kHz', s.proBlock === proBlock && proBlock === 960,
         String(s.proBlock));
  const raus = s.schiebe(sinus(proBlock, 1000, ZIEL_RATE));
  pruefe('ein voller Block gibt genau einen Rahmen', raus.length === 1, String(raus.length));
  const k = kopfVon(raus[0]);
  pruefe('Stromart ist TX-Ton', k.strom === STROM_TX_AUDIO, String(k.strom));
  pruefe('Abtastrate steht im Kopf', k.rate === ZIEL_RATE, String(k.rate));
  pruefe('ein Kanal — ein Mikrofon', k.kanaele === 1, String(k.kanaele));
  pruefe('Anzahl ist die FLACHE Zahl der Werte', k.anzahl === proBlock, String(k.anzahl));
  pruefe('mu-law: ein Byte je Wert', raus[0].byteLength === HDR + proBlock,
         String(raus[0].byteLength));
}

// 2. Kein Rest geht verloren. Das ist der Fall, der sich NICHT anhoert wie
//    ein Fehler, sondern wie ein schlechtes Mikrofon.
{
  const s = new Sendestrecke({ quellRate: ZIEL_RATE });
  let werte = 0;
  // Krumme Blockgroessen, wie ein Telefon sie liefert — nie ein Vielfaches
  // von 960.
  for (const n of [128, 2048, 333, 4096, 777, 1024]) {
    for (const r of s.schiebe(sinus(n, 700, ZIEL_RATE))) {
      werte += kopfVon(r).anzahl;
    }
  }
  const herein = 128 + 2048 + 333 + 4096 + 777 + 1024;
  const uebrig = herein - werte;
  pruefe('alles bis auf den letzten Teilblock kommt heraus',
         uebrig >= 0 && uebrig < 960, `${herein} herein, ${werte} heraus, ${uebrig} offen`);
}

// 3. Der Vorrat waechst mit. Ein Block, der groesser ist als der
//    Anfangspuffer, darf nichts verschlucken.
{
  const s = new Sendestrecke({ quellRate: ZIEL_RATE });
  const riesig = sinus(48000, 500, ZIEL_RATE);   // eine ganze Sekunde auf einmal
  const raus = s.schiebe(riesig);
  pruefe('eine Sekunde am Stueck gibt 50 Rahmen', raus.length === 50, String(raus.length));
}

// 4. Ratenwandlung ohne Sprung an der Blockgrenze.
//
//    Ohne den uebertragenen Rest faengt jeder Block wieder bei Probe 0 an,
//    und bei 50 Bloecken je Sekunde hoert man das als Rauen. Nachweis: die
//    groesste Stufe zwischen zwei aufeinanderfolgenden Werten darf nicht
//    groesser sein als das, was ein glatter Sinus ohnehin macht.
{
  // Die Schwelle ist gemessen, nicht geschaetzt. Gegen eine Fassung ohne
  // uebertragenen Rest liegt die groesste Stufe durchweg beim 1,5-fachen —
  // bei 60, 400, 1200 und 2400 Hz gleichermassen, es haengt also nicht an
  // der Frequenz, sondern an der Schwelle. Bei 400 Hz:
  //
  //     ideal 0,0262   gut 0,0262   kaputt 0,0407
  //
  // 1,25 x ideal = 0,0328 trennt beides mit rund einem Viertel Luft nach
  // jeder Seite. Ein erster Versuch mit 1,5 x liess 0,0407 gegen 0,0393
  // stehen — das faengt beim naechsten Mal nichts mehr.
  const quell = 44100;
  const HZ = 400;
  const s = new Sendestrecke({ quellRate: quell, typ: TYP_INT16 });
  const alle = [];
  for (let b = 0; b < 12; b++) {
    // Fortlaufende Phase ueber die Bloecke hinweg — sonst prueft der Stand
    // seinen eigenen Sprung.
    const n = 1024;
    const a = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      a[i] = 0.5 * Math.sin(2 * Math.PI * HZ * (b * n + i) / quell);
    }
    for (const r of s.schiebe(a)) {
      const w = new Int16Array(r, HDR);
      for (let i = 0; i < w.length; i++) { alle.push(w[i] / 32768); }
    }
  }
  let groesste = 0;
  for (let i = 1; i < alle.length; i++) {
    const d = Math.abs(alle[i] - alle[i - 1]);
    if (d > groesste) { groesste = d; }
  }
  const glatt = 2 * Math.PI * HZ / ZIEL_RATE * 0.5;   // ≈0,0262
  const erlaubt = glatt * 1.25;
  pruefe('keine Stufe an der Blockgrenze (44,1 -> 48 kHz)', groesste < erlaubt,
         `groesste Stufe ${groesste.toFixed(4)}, erlaubt ${erlaubt.toFixed(4)}`);
  pruefe('genug Werte zum Urteilen', alle.length > 8000, String(alle.length));
}

// 5. Der Pegel misst, was hereinkommt — nicht, was danach daraus wird.
{
  const s = new Sendestrecke({ quellRate: ZIEL_RATE });
  s.schiebe(sinus(960, 1000, ZIEL_RATE, 0.8));
  const sp = s.spitzeAblesen();
  pruefe('Spitze folgt dem Eingang', Math.abs(sp - 0.8) < 0.02, sp.toFixed(3));
  pruefe('Spitze wird beim Ablesen zurueckgesetzt', s.spitzeAblesen() === 0);
  pruefe('Effektivwert ist der eines Sinus (Spitze/√2)',
         Math.abs(s.effektiv - 0.8 / Math.SQRT2) < 0.02, s.effektiv.toFixed(3));
}

// 6. Loslassen laesst keinen halben Block liegen.
{
  const s = new Sendestrecke({ quellRate: ZIEL_RATE });
  s.schiebe(sinus(500, 1000, ZIEL_RATE));     // weniger als ein Block
  s.leeren();
  const raus = s.schiebe(sinus(460, 1000, ZIEL_RATE));
  pruefe('nach leeren() traegt nichts in die naechste Uebertragung hinueber',
         raus.length === 0, String(raus.length));
}

// 7. Die Stille ist still. Ein Rahmen voller Nullen darf nicht zu
//    mu-law-Muell werden — am 2026-10-02 trug ein Tonstrom mit vollen
//    12 kB/s Stille, und das sah von aussen aus wie Betrieb.
{
  const s = new Sendestrecke({ quellRate: ZIEL_RATE });
  const raus = s.schiebe(new Float32Array(960));
  const bytes = new Uint8Array(raus[0], HDR);
  const alleGleich = bytes.every((b) => b === bytes[0]);
  pruefe('Stille gibt einen gleichfoermigen Rahmen', alleGleich,
         `${new Set(bytes).size} verschiedene Werte`);
  pruefe('und der Pegel meldet sie als still', s.spitzeAblesen() === 0);
}

console.log();
console.log(fehler === 0 ? 'alles gruen' : `${fehler} Punkt(e) offen`);
process.exit(fehler ? 1 : 0);
