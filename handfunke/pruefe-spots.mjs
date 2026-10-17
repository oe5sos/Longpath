#!/usr/bin/env node
// Pruefstand fuer die Spots im Bild — ohne Browser, ohne Funkgeraet.
//
// Geprueft wird, was auf einem Telefon still falsch sein kann:
//
//   * Eine Marke, die ein paar Pixel neben der Station sitzt. Wer sie
//     antippt, stimmt daneben ab — und merkt nur, dass "der Spot nicht
//     stimmt". Darum dieselbe Rechnung wie beim Abstimmstrich, nur
//     andersherum, und hier gegengerechnet.
//
//   * Uebereinandergedruckte Rufzeichen. Zwei Spots 300 Hz auseinander sind
//     auf einem Telefon derselbe Pixel, und das ist nicht der Ausnahmefall:
//     ein Pile-up sieht genau so aus. Eine unlesbare Marke ist schlimmer
//     als keine, weil man sie trotzdem antippt.
//
//     node pruefe-spots.mjs

import { spotLesen, marken, trefferBei, trefferAufSchrift, deckung,
         kFrischBis, kVerblasstAb, kMindestDeckung } from './spots.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

const z = (nr, hz, ruf, alter = 0) =>
  spotLesen([String(nr), String(hz), ruf, 'CW', 'CLUSTER', String(alter)]);

const MITTE = 14_100_000, SPANNE = 48_000, BREITE = 360;

console.log('Zeilen lesen\n');
{
  const s = z(0, 14_100_000, 'oe1dx', 90);
  pruefe('Rufzeichen wird gross', s.ruf === 'OE1DX');
  pruefe('Alter kommt mit', s.alterSek === 90);
  pruefe('ohne Rufzeichen -> null',
         spotLesen(['0', '14100000', '', 'CW', 'RBN', '0']) === null);
  pruefe('ohne Frequenz -> null',
         spotLesen(['0', '0', 'OE1DX', 'CW', 'RBN', '0']) === null);
  pruefe('Unfug -> null', spotLesen(['x', 'y', 'OE1DX']) === null);
  pruefe('fehlendes Alter ist 0',
         spotLesen(['0', '14100000', 'OE1DX']).alterSek === 0);
}

console.log('\nDie Stelle');

// 1. Die Mitte ist die Mitte. Gegengerechnet gegen frequenzAnStelle():
//    x/Breite - 0.5 mal Spanne plus Mitte muss die Frequenz zurueckgeben.
{
  const m = marken([z(0, MITTE, 'MITTE')], MITTE, SPANNE, BREITE);
  pruefe('Bildmitte liegt in der Mitte', m.length === 1 && m[0].x === BREITE / 2,
         JSON.stringify(m));
}
{
  // Gegenrechnung ueber mehrere Stellen — so sieht man einen Vorzeichen-
  // oder Faktorfehler, den ein einzelner Punkt in der Mitte verbirgt.
  const zurueck = (x) => (x / BREITE - 0.5) * SPANNE + MITTE;
  let groessterFehler = 0;
  for (const dHz of [-20000, -7000, -100, 0, 100, 7000, 20000]) {
    const m = marken([z(0, MITTE + dHz, 'X')], MITTE, SPANNE, BREITE);
    if (!m.length) { groessterFehler = Infinity; break; }
    groessterFehler = Math.max(groessterFehler, Math.abs(zurueck(m[0].x) - (MITTE + dHz)));
  }
  pruefe('Hin- und Rueckrechnung stimmen ueberein (< 1 Hz)',
         groessterFehler < 1, String(groessterFehler));
}
{
  // Ausserhalb faellt weg — sonst klebte eine Marke am Bildrand und zeigte
  // auf eine Frequenz, die gar nicht zu sehen ist.
  const m = marken([z(0, MITTE + 30000, 'WEG'), z(1, MITTE - 30000, 'AUCHWEG'),
                    z(2, MITTE + 1000, 'DA')], MITTE, SPANNE, BREITE);
  pruefe('ausserhalb des Bildes faellt weg',
         m.length === 1 && m[0].ruf === 'DA', JSON.stringify(m.map(x => x.ruf)));
}
{
  // Die Raender gehoeren dazu.
  const m = marken([z(0, MITTE - SPANNE / 2, 'LINKS'),
                    z(1, MITTE + SPANNE / 2, 'RECHTS')], MITTE, SPANNE, BREITE);
  pruefe('genau am Rand zaehlt noch', m.length === 2);
  pruefe('links ist x=0, rechts x=Breite',
         m[0].x === 0 && m[1].x === BREITE, JSON.stringify(m.map(x => x.x)));
}
{
  pruefe('ohne Spanne keine Marken',
         marken([z(0, MITTE, 'X')], MITTE, 0, BREITE).length === 0);
  pruefe('ohne Breite keine Marken',
         marken([z(0, MITTE, 'X')], MITTE, SPANNE, 0).length === 0);
  pruefe('nichts hinein, nichts heraus', marken(null, MITTE, SPANNE, BREITE).length === 0);
}

console.log('\nBeschriftungen, die sich nicht decken');
{
  // Drei Spots auf 300 Hz Abstand: bei 48 kHz auf 360 px sind das gut
  // 2 Pixel. Alle drei in eine Reihe waere unlesbar.
  const m = marken([z(0, MITTE - 300, 'AAA1'), z(1, MITTE, 'BBB2'),
                    z(2, MITTE + 300, 'CCC3')], MITTE, SPANNE, BREITE);
  pruefe('drei dichte Spots kommen alle durch', m.length === 3);
  pruefe('und stehen in drei Reihen',
         new Set(m.map(x => x.reihe)).size === 3, JSON.stringify(m.map(x => x.reihe)));
}
{
  // Weit auseinander: alle in Reihe 0, sonst huepfte die Anzeige ohne Grund.
  const m = marken([z(0, MITTE - 20000, 'A'), z(1, MITTE, 'B'),
                    z(2, MITTE + 20000, 'C')], MITTE, SPANNE, BREITE);
  pruefe('weit auseinander bleibt alles in Reihe 0',
         m.every(x => x.reihe === 0), JSON.stringify(m.map(x => x.reihe)));
}
{
  // Mehr dichte Spots als Reihen: keiner darf verschwinden. Lieber eine
  // enge Stelle als eine Marke, die es nicht gibt.
  const viele = [];
  for (let i = 0; i < 8; i++) viele.push(z(i, MITTE - 1000 + i * 200, 'S' + i));
  const m = marken(viele, MITTE, SPANNE, BREITE, { reihen: 3 });
  pruefe('acht dichte Spots gehen nicht verloren', m.length === 8, String(m.length));
  pruefe('und bleiben in den erlaubten Reihen',
         m.every(x => x.reihe >= 0 && x.reihe <= 2));
}
{
  // Ausgabe nach x sortiert, egal in welcher Reihenfolge sie ankommen.
  const m = marken([z(0, MITTE + 5000, 'RECHTS'), z(1, MITTE - 5000, 'LINKS')],
                   MITTE, SPANNE, BREITE);
  pruefe('Ausgabe steht nach Stelle',
         m[0].ruf === 'LINKS' && m[1].ruf === 'RECHTS');
}

console.log('\nAlter');
{
  pruefe('frisch ist voll deckend', deckung(0) === 1 && deckung(kFrischBis) === 1);
  pruefe('ganz alt ist am blassesten', deckung(kVerblasstAb) === kMindestDeckung);
  pruefe('noch aelter wird nicht blasser',
         deckung(kVerblasstAb * 5) === kMindestDeckung);
  const mitte = deckung((kFrischBis + kVerblasstAb) / 2);
  pruefe('dazwischen liegt dazwischen',
         mitte < 1 && mitte > kMindestDeckung, String(mitte));
  pruefe('nie ganz unsichtbar', deckung(999999) > 0);
}

console.log('\nTippen');
{
  const m = marken([z(0, MITTE - 5000, 'LINKS'), z(1, MITTE + 5000, 'RECHTS')],
                   MITTE, SPANNE, BREITE);
  pruefe('genau getroffen', trefferBei(m, m[0].x).ruf === 'LINKS');
  pruefe('knapp daneben zaehlt noch', trefferBei(m, m[0].x + 15).ruf === 'LINKS');
  // Nach links, wo keine zweite Marke steht: m[0].x + 60 laege 15 px
  // neben RECHTS (10 kHz sind hier 75 px) und traefe die zu Recht.
  pruefe('weit daneben trifft nichts', trefferBei(m, m[0].x - 60) === null);
  pruefe('zwischen den Marken trifft die naechste, nicht nichts',
         trefferBei(m, m[0].x + 60).ruf === 'RECHTS');
  pruefe('zwischen zweien gewinnt der naehere',
         trefferBei(m, m[1].x - 8, 40).ruf === 'RECHTS');
  pruefe('ohne Marken kein Treffer', trefferBei([], 100) === null);
  pruefe('null vertraegt sich', trefferBei(null, 100) === null);
}

console.log('\nTippen auf die Schrift');
{
  // Der Kasten kommt vom ZEICHNEN, nicht aus einer zweiten Rechnung. Wer
  // ihn nie gesetzt hat, darf auch nicht getroffen werden -- sonst faengt
  // eine Marke Tipper ab, die noch gar nicht auf dem Schirm steht.
  const ohne = { ruf: 'OHNE', x: 100 };
  pruefe('ohne gezeichneten Kasten kein Treffer',
         trefferAufSchrift([ohne], 100, 10) === null);

  const m = { ruf: 'OE5SOS', x: 100, kasten: { x: 80, y: 4, b: 40, h: 11 } };
  pruefe('mitten auf der Schrift trifft', trefferAufSchrift([m], 100, 9).ruf === 'OE5SOS');
  pruefe('linke obere Ecke trifft', trefferAufSchrift([m], 80, 4) !== null);
  pruefe('rechte untere Ecke trifft', trefferAufSchrift([m], 120, 15) !== null);
  pruefe('etwas Luft darum trifft noch', trefferAufSchrift([m], 77, 2) !== null);
  pruefe('weit daneben trifft nicht', trefferAufSchrift([m], 100, 60) === null);
  pruefe('weit rechts trifft nicht', trefferAufSchrift([m], 200, 9) === null);
  pruefe('ohne Marken kein Treffer', trefferAufSchrift([], 100, 9) === null);
  pruefe('null vertraegt sich', trefferAufSchrift(null, 1, 1) === null);

  // DER Punkt der Trennung: tief unten am Strich ist die FREQUENZ gemeint,
  // nicht das Rufzeichen. Dort darf der Schrifttreffer nichts melden,
  // waehrend der Strichtreffer sehr wohl greift.
  const marke = { ruf: 'OE5SOS', x: 100, kasten: { x: 80, y: 4, b: 40, h: 11 } };
  pruefe('unten am Strich: keine Schrift, aber eine Marke',
         trefferAufSchrift([marke], 100, 70) === null
         && trefferBei([marke], 100) !== null);
}

console.log(fehler === 0 ? '\nAlles gut.' : `\n${fehler} Punkt(e) offen.`);
process.exit(fehler === 0 ? 0 : 1);
