#!/usr/bin/env node
// Pruefstand fuer die Zeichenbremse.
//
// Zwei Fehler sind hier moeglich, und der zweite ist der schlimmere:
//
//   1. Nicht bremsen, wenn der Ton knapp wird -> es knackt weiter. Der
//      sichtbare Fehler, der gemeldet wird.
//   2. Zu viel bremsen -> der Wasserfall steht. Ein stehender Wasserfall
//      ist von einem toten Empfang nicht zu unterscheiden, und DAS meldet
//      niemand als Zeichenfehler, sondern als "das Funkgeraet geht nicht".
//
//     node pruefe-ton-vorrang.mjs

import { Zeichenbremse } from './ton-vorrang.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

const lage = (vorrat, ziel = 1000, imHauptfaden = true) => ({ vorrat, ziel, imHauptfaden });

console.log('Wann gebremst wird\n');
{
  const b = new Zeichenbremse();
  pruefe('voller Vorrat -> zeichnen', b.darfZeichnen(lage(1000)) === true);
  pruefe('knapp unter Ziel -> noch zeichnen', b.darfZeichnen(lage(700)) === true);
  pruefe('deutlich zu wenig -> auslassen', b.darfZeichnen(lage(400)) === false);
}

// Hysterese: einmal gebremst, wird erst bei 90 % wieder gezeichnet. Ohne das
// flattert die Entscheidung im Bildtakt.
{
  const b = new Zeichenbremse();
  b.darfZeichnen(lage(400));                       // bremst jetzt
  pruefe('gebremst bleibt gebremst bei 70 %', b.darfZeichnen(lage(700)) === false);
  pruefe('und auch noch bei 85 %', b.darfZeichnen(lage(850)) === false);
  pruefe('ab 90 % wieder zeichnen', b.darfZeichnen(lage(900)) === true);
}

// Gegenprobe zur Hysterese: OHNE sie wuerde bei 70 % wieder gezeichnet.
// Mit obenAnteil = untenAnteil ist die Bremse genau das -- und flattert.
{
  const b = new Zeichenbremse({ untenAnteil: 0.6, obenAnteil: 0.6 });
  b.darfZeichnen(lage(400));
  pruefe('ohne Hysterese flattert es (belegt, nicht behauptet)',
         b.darfZeichnen(lage(700)) === true);
}

console.log('\nDer Wasserfall darf nicht einfrieren');
{
  // Dauerhaft zu wenig Vorrat: nach hoechstens 15 ausgelassenen Bildern
  // MUSS eines durchkommen.
  const b = new Zeichenbremse({ hoechstensAmStueck: 15 });
  let gezeichnet = 0, amStueckAus = 0, schlimmste = 0;
  for (let i = 0; i < 200; i++) {
    if (b.darfZeichnen(lage(100))) { gezeichnet++; schlimmste = Math.max(schlimmste, amStueckAus); amStueckAus = 0; }
    else { amStueckAus++; }
  }
  pruefe('auch bei Dauerflaute wird gezeichnet', gezeichnet > 0, String(gezeichnet));
  pruefe('nie mehr als 15 Bilder am Stueck ausgelassen',
         schlimmste <= 15, String(schlimmste));
  pruefe('und das sind rund 12 Pflichtbilder auf 200',
         gezeichnet >= 10 && gezeichnet <= 15, String(gezeichnet));
}

console.log('\nWann NICHT gebremst wird');
{
  // Worklet: eigener Faden, Zeichnen kostet den Ton nichts.
  const b = new Zeichenbremse();
  let alle = true;
  for (let i = 0; i < 50; i++) { if (!b.darfZeichnen(lage(0, 1000, false))) alle = false; }
  pruefe('im Worklet wird nie gebremst', alle);
  pruefe('und es wird nichts mitgezaehlt', b.ausgelassen === 0, String(b.ausgelassen));
}
{
  // Ohne bekanntes Ziel laesst sich nichts beurteilen. Eine Bremse auf
  // Verdacht haelt das Bild an, ohne dem Ton zu helfen.
  const b = new Zeichenbremse();
  pruefe('ohne Ziel wird nicht gebremst', b.darfZeichnen(lage(0, 0)) === true);
  pruefe('unsinnige Werte bremsen nicht', b.darfZeichnen({}) === true);
  pruefe('gar nichts bremst auch nicht', b.darfZeichnen(null) === true);
}

console.log('\nZaehlwerk');
{
  const b = new Zeichenbremse();
  for (let i = 0; i < 5; i++) b.darfZeichnen(lage(100));
  pruefe('ausgelassene Bilder werden gezaehlt', b.ausgelassen === 5, String(b.ausgelassen));
  b.darfZeichnen(lage(1000));
  pruefe('und beim Erholen nicht zurueckgesetzt', b.ausgelassen === 5, String(b.ausgelassen));
}

console.log(fehler === 0 ? '\nAlles gut.' : `\n${fehler} Punkt(e) offen.`);
process.exit(fehler === 0 ? 0 : 1);
