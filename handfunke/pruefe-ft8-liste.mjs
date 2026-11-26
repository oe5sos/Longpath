#!/usr/bin/env node
// Pruefstand: FT8-Liste aus den Spotzeilen (spots.js: ft8Liste, alterText).
//
//     node pruefe-ft8-liste.mjs

import { spotLesen, ft8Liste, alterText } from './spots.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

// So, wie Longpath sie schickt: spot_zeile:<nr>,<hz>,<ruf>,<mode>,<quelle>,<alter>
const z = [
  '0,14074350,ea1aaa,FT8,LONGPATH,40',
  '1,14074800,DL2BBB,FT8,LONGPATH,5',
  '2,14075200,IK4EEE,FT8,CLUSTER,2',      // fremd gehoert: nicht in die Liste
  '3,14075600,OE3CCC,CW,LONGPATH,1',      // kein FT8
  '4,14076100,DL2BBB,FT8,LONGPATH,70',    // dasselbe Rufzeichen, aelter
  '5,14074900,F4DDD,FT4,LONGPATH,5',
  '6,14077000,G0EEE,FT8,LONGPATH,130',
].map((s) => spotLesen(s.split(',')));

const l = ft8Liste(z, 14074000);
pruefe('nur eigene FT8/FT4-Funde', l.every((x) => ['FT8', 'FT4'].includes(x.mode)) && !l.some((x) => x.ruf === 'IK4EEE' || x.ruf === 'OE3CCC'));
pruefe('je Rufzeichen einmal', l.filter((x) => x.ruf === 'DL2BBB').length === 1);
pruefe('der frischere Fund gewinnt', l.find((x) => x.ruf === 'DL2BBB').df === 800);
pruefe('neueste oben', l[0].alterSek === 5 && l[l.length - 1].ruf === 'G0EEE', l.map((x) => x.ruf).join(' '));
pruefe('Gleichstand: naeher an der Dial-Frequenz zuerst', l[0].ruf === 'DL2BBB' && l[1].ruf === 'F4DDD', l.map((x) => x.ruf).join(' '));
pruefe('Rufzeichen gross', l.some((x) => x.ruf === 'EA1AAA'));
pruefe('Delta f zur Dial-Frequenz', l.find((x) => x.ruf === 'EA1AAA').df === 350);
pruefe('ohne Dial-Frequenz df null', ft8Liste(z, 0)[0].df === null);
pruefe('leer bleibt leer', ft8Liste([], 14074000).length === 0 && ft8Liste(null, 1).length === 0);
pruefe('Alter in s', alterText(12) === '12 s');
pruefe('Alter in min', alterText(130) === '2 min');

console.log(fehler ? `\n${fehler} FEHLT` : '\nalles ok');
process.exit(fehler ? 1 : 0);
