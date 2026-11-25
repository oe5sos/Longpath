#!/usr/bin/env node
// Pruefstand: liest die Seite RIT/XIT so, wie Longpath sie schickt?
//   rit_enable:<rx>,<true|false>;  rit_offset:<rx>,<hz>;  (xit ebenso)
// (TciProtocol buildRitEnableLine/buildRitOffsetLine, Init-Burst und Gegenmeldung)
//
//     node pruefe-versatz.mjs

import { TciLink } from './tci.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

globalThis.performance ??= { now: () => Date.now() };
const l = new TciLink();
pruefe('vor dem Init-Burst unbekannt (null), nicht "aus"', l.st.rit[0] === null && l.st.ritHz[0] === null);
l._onText('rit_enable:0,true');
l._onText('rit_offset:0,-250');
l._onText('xit_enable:1,false');
l._onText('xit_offset:1,1200');
pruefe('RIT ein auf Empfaenger 0', l.st.rit[0] === true);
pruefe('RIT-Versatz negativ', l.st.ritHz[0] === -250, String(l.st.ritHz[0]));
pruefe('XIT aus auf Empfaenger 1', l.st.xit[1] === false);
pruefe('XIT-Versatz', l.st.xitHz[1] === 1200, String(l.st.xitHz[1]));
pruefe('Empfaenger 0 bleibt vom XIT unberuehrt', l.st.xit[0] === null);
l._onText('rit_offset:5,99');
pruefe('fremde Empfaengernummer wird nicht eingetragen', l.st.ritHz.length === 2 && l.st.ritHz[5] === undefined);

console.log(fehler ? `\n${fehler} FEHLT` : '\nalles ok');
process.exit(fehler ? 1 : 0);
