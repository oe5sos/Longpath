#!/usr/bin/env node
// Pruefstand: welches Feld von `tx_sensors` die Seite als SWR liest.
//
// Longpath schickt (TciSensorManager::formatTxSensors)
//     tx_sensors:<rx>,<micDbm>,<rmsW>,<peakW>,<swr>;
// Bis zum 2026-11-22 las die Seite Feld 1 (Mikrofonpegel) als Leistung und
// Feld 2 (Watt) als SWR. Angezeigt wurde nichts davon, also fiel es nicht auf.
//
//     node pruefe-tx-sensors.mjs

import { TciLink } from './tci.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

globalThis.performance ??= { now: () => Date.now() };
const l = new TciLink();
l._onText('tx_sensors:0,-30.0,12.5,13.0,1.4');
pruefe('SWR ist Feld 4 (1,4)', l.st.txSwr === 1.4, String(l.st.txSwr));
pruefe('Leistung ist Feld 2 (12,5 W), nicht der Mikrofonpegel', l.st.txPower === 12.5, String(l.st.txPower));
pruefe('der Zeitstempel wird gesetzt', l.st.txSensorZeit > 0);
const vorher = l.st.txSensorZeit;
l._onText('tx_sensors:0,-30.0,0.0,0.0,1.0');
pruefe('ein zweiter Rahmen ueberschreibt', l.st.txSwr === 1 && l.st.txSensorZeit >= vorher);

console.log(fehler ? `\n${fehler} FEHLT` : '\nalles ok');
process.exit(fehler ? 1 : 0);
