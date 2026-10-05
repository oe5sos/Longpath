#!/usr/bin/env node
// Pruefstand fuer den Rotor auf der Seite.
//
// Am anderen Ende steht ein Mast. Zwei Fehler sind hier moeglich, und beide
// merkt man erst, wenn die Antenne falsch steht:
//
//   * Der Winkel wird falsch herum oder mit falschem Nullpunkt gerechnet.
//     Nord muss OBEN sein und im Uhrzeigersinn gezaehlt werden -- `atan2(y,x)`
//     waere Ost = 0 und gegen den Uhrzeigersinn, und das sieht auf einer
//     runden Scheibe genauso plausibel aus.
//   * Ein Tipp dreht sofort. Auf einem Telefon in der Hosentasche ist das
//     eine schlechte Idee.
//
//     node pruefe-rotor.mjs

import { standLesen, zustandText, peilungAus, Sicherung,
         GETRENNT, VERBINDET, BEREIT, DREHT, FEHLER } from './rotor.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

console.log('Stand lesen\n');
{
  const s = standLesen(['143.5', '2', '1']);
  pruefe('Grad, Zustand und Frische kommen an',
         s.grad === 143.5 && s.zustand === BEREIT && s.frisch === true);
  pruefe('nicht frisch wird nicht verschluckt',
         standLesen(['10', '2', '0']).frisch === false);
  pruefe('ueber 360 wird eingefangen', standLesen(['370', '2', '1']).grad === 10);
  pruefe('negativ wird eingefangen', standLesen(['-10', '2', '1']).grad === 350);
  pruefe('ohne Zahl -> null', standLesen(['', '2', '1']) === null);
  pruefe('Unfug -> null', standLesen(['abc']) === null);
}

console.log('\nWas darunter steht');
{
  pruefe('bereit und frisch -> nichts',
         zustandText({ grad: 0, zustand: BEREIT, frisch: true }) === '');
  pruefe('bereit, aber alt -> sagt es',
         /VORHIN/.test(zustandText({ grad: 0, zustand: BEREIT, frisch: false })));
  pruefe('dreht -> sagt es', /DREHT/.test(zustandText({ zustand: DREHT })));
  pruefe('getrennt -> sagt es', /NICHT VERBUNDEN/.test(zustandText({ zustand: GETRENNT })));
  pruefe('Fehler -> sagt es', /FEHLER/.test(zustandText({ zustand: FEHLER })));
  pruefe('gar kein Rotor -> sagt es', /KEIN ROTOR/.test(zustandText(null)));
}

console.log('\nDie Peilung unter dem Finger');
{
  // Nord ist OBEN, gezaehlt im Uhrzeigersinn. y zeigt nach unten.
  pruefe('oben = 0 (Nord)',   peilungAus(0, -100) === 0);
  pruefe('rechts = 90 (Ost)', peilungAus(100, 0) === 90);
  pruefe('unten = 180 (Sued)', peilungAus(0, 100) === 180);
  pruefe('links = 270 (West)', peilungAus(-100, 0) === 270);
  pruefe('oben rechts = 45',  peilungAus(70, -70) === 45);
  pruefe('oben links = 315',  peilungAus(-70, -70) === 315);

  // Gegenprobe: mit atan2(y,x) waere rechts = 0 und unten = 90. Genau das
  // darf NICHT herauskommen -- auf einer runden Scheibe sieht es gleich aus.
  pruefe('rechts ist NICHT 0 (der verdrehte Nullpunkt)', peilungAus(100, 0) !== 0);
  pruefe('unten ist NICHT 90 (die verdrehte Zaehlrichtung)', peilungAus(0, 100) !== 90);

  pruefe('zu nah an der Mitte -> keine Peilung', peilungAus(2, -3) === null);
  pruefe('genau in der Mitte -> keine Peilung', peilungAus(0, 0) === null);
  pruefe('immer 0..359', [[1,-1],[-1,1],[0,-1],[-0.001,-100]]
           .every(([x,y]) => { const g = peilungAus(x*100, y*100); return g >= 0 && g < 360; }));
}

console.log('\nDie Sicherung');
{
  const s = new Sicherung();
  pruefe('ohne Wahl dreht nichts', s.bestaetige(1000) === null);

  s.waehle(270, 1000);
  pruefe('gewaehlt gilt', s.gilt(1500) === true);
  pruefe('bestaetigen gibt das Ziel frei', s.bestaetige(1500) === 270);
  pruefe('und danach ist wieder zu', s.bestaetige(1600) === null);

  // Eine Wahl von vorhin gehoert dem Finger von vorhin.
  const s2 = new Sicherung({ haltbarMs: 15000 });
  s2.waehle(90, 0);
  pruefe('nach 15 s gilt sie noch', s2.gilt(15000) === true);
  pruefe('nach 16 s nicht mehr', s2.gilt(16000) === false);
  pruefe('und bestaetigen dreht dann nicht', s2.bestaetige(16000) === null);

  const s3 = new Sicherung();
  pruefe('unsinnige Peilung wird nicht gewaehlt', s3.waehle(NaN, 0) === false);
  pruefe('360 wird nicht gewaehlt (0 ist Nord)', s3.waehle(360, 0) === false);
  pruefe('negativ wird nicht gewaehlt', s3.waehle(-1, 0) === false);
  pruefe('und dann steht auch nichts bereit', s3.bestaetige(0) === null);
  pruefe('0 Grad ist eine gueltige Wahl', s3.waehle(0, 0) === true);
  pruefe('und wird auch so herausgegeben', s3.bestaetige(10) === 0);
}

console.log(fehler === 0 ? '\nAlles gut.' : `\n${fehler} Punkt(e) offen.`);
process.exit(fehler === 0 ? 0 : 1);
