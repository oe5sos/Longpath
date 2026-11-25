#!/usr/bin/env node
// Pruefstand: liest die Seite AGC und NB/NR/ANF/SQL so, wie Longpath sie schickt?
// (TciProtocol Init-Burst; TciServer-Gegenmeldungen, z. B. activeNrChanged
//  -> rx_nr_enable + rx_nr_enable_ex mit Platznummer 0 NR1, 1 NR2 ...)
//
//     node pruefe-aufbereitung.mjs

import { TciLink } from './tci.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

globalThis.performance ??= { now: () => Date.now() };
const l = new TciLink();
pruefe('vor dem Init-Burst unbekannt', l.st.nr[0] === null && l.st.agcMode[0] === null);
l._onText('agc_mode:0,normal');
l._onText('agc_gain:0,80');
l._onText('rx_nb_enable:0,true');
l._onText('rx_anf_enable:0,false');
l._onText('sql_enable:0,true');
l._onText('sql_level:0,-30');
l._onText('rx_nr_enable:0,true');
l._onText('rx_nr_enable_ex:0,true,4');
pruefe('AGC-Stufe wie gesendet (normal = Med)', l.st.agcMode[0] === 'normal');
pruefe('AGC-T', l.st.agcGain[0] === 80);
pruefe('NB ein, ANF aus', l.st.nb[0] === true && l.st.anf[0] === false);
pruefe('SQL ein mit Schwelle', l.st.sql[0] === true && l.st.sqlDb[0] === -30);
pruefe('NR ein, Art 4 (DFNR)', l.st.nr[0] === true && l.st.nrIdx[0] === 4);
l._onText('rx_nr_enable:0,false');
l._onText('rx_nr_enable_ex:0,false,0');
pruefe('NR aus', l.st.nr[0] === false);
pruefe('beim Ausschalten bleibt die letzte Art gemerkt (Server meldet dann 0)', l.st.nrIdx[0] === 4, String(l.st.nrIdx[0]));
l._onText('agc_mode:1,LONG');
pruefe('AGC-Stufe klein geschrieben, Empfaenger 1 getrennt', l.st.agcMode[1] === 'long' && l.st.agcMode[0] === 'normal');

console.log(fehler ? `\n${fehler} FEHLT` : '\nalles ok');
process.exit(fehler ? 1 : 0);
