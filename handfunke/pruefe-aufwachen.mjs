#!/usr/bin/env node
// Pruefstand fuer das Zurueckkommen aus dem Hintergrund.
//
// Der Fehler, gegen den das meiste hier steht, ist der LEISE: ein Socket,
// der OFFEN meldet und trotzdem tot ist. Wer nur auf `readyState` sieht,
// wartet auf ein `onclose`, das nie kommt -- und der Bediener steht vor
// einem Bild von vorhin, das wie ein leeres Band aussieht.
//
//     node pruefe-aufwachen.mjs

import { sollNeuVerbinden, bildIstAlt, kTotStilleMs,
         VERBINDET, OFFEN, SCHLIESST, ZU } from './aufwachen.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

console.log('Neu verbinden?\n');
{
  const l = (zustand, stilleMs, gewollt = true) => ({ gewollt, zustand, stilleMs });
  pruefe('geschlossen -> ja', sollNeuVerbinden(l(ZU, 0)) === true);
  pruefe('schliesst gerade -> ja', sollNeuVerbinden(l(SCHLIESST, 0)) === true);
  pruefe('gar kein Socket -> ja', sollNeuVerbinden(l(null, 0)) === true);

  // DER Fall: offen, aber seit Sekunden still.
  pruefe('offen und still -> ja (der tote Socket)',
         sollNeuVerbinden(l(OFFEN, kTotStilleMs + 1)) === true);
  pruefe('offen und lebendig -> nein',
         sollNeuVerbinden(l(OFFEN, 200)) === false);
  pruefe('offen, Stille genau an der Grenze -> nein',
         sollNeuVerbinden(l(OFFEN, kTotStilleMs)) === false);

  // Ein laufender Versuch darf nicht bei jedem Blick abgewuergt werden.
  pruefe('verbindet gerade, frisch -> nein',
         sollNeuVerbinden(l(VERBINDET, 500)) === false);
  pruefe('verbindet gerade, aber eingeschlafen -> ja',
         sollNeuVerbinden(l(VERBINDET, kTotStilleMs + 1)) === true);

  // Wer getrennt sein WILL, bleibt getrennt.
  pruefe('nicht gewollt -> nie', sollNeuVerbinden(l(ZU, 99999, false)) === false);
  pruefe('nicht gewollt, auch nicht bei Stille',
         sollNeuVerbinden(l(OFFEN, 99999, false)) === false);
  pruefe('ohne Angaben wird nichts weggeworfen', sollNeuVerbinden(null) === false);
}

console.log('\nIst das Bild alt?');
{
  pruefe('frisch -> nein', bildIstAlt(100) === false);
  pruefe('zwei Sekunden Stille -> ja', bildIstAlt(2000) === true);
  pruefe('an der Grenze -> nein', bildIstAlt(1500) === false);
  pruefe('nichts gemessen -> nein', bildIstAlt(undefined) === false);
  // Die Grenze liegt UNTER der fuer die Neuverbindung: lieber einmal zu
  // frueh sagen "das Bild ist von vorhin" als einmal zu spaet. Ein falsch
  // beschriftetes altes Bild ist harmlos, ein unbeschriftetes nicht.
  pruefe('Bild gilt frueher als alt als der Socket als tot',
         bildIstAlt(2000) === true && sollNeuVerbinden(
           { gewollt: true, zustand: OFFEN, stilleMs: 2000 }) === false);
}

console.log(fehler === 0 ? '\nAlles gut.' : `\n${fehler} Punkt(e) offen.`);
process.exit(fehler === 0 ? 0 : 1);
