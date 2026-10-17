#!/usr/bin/env node
// Pruefstand fuer das Logbuch in der App — ohne Browser, ohne Funkgeraet.
//
// Geprueft wird, was still falsch sein kann:
//
//   * Eine Liste, die unvollstaendig ist und vollstaendig aussieht. Longpath
//     schickt je Kontakt eine Zeile und erst danach `log_last_ok:<anzahl>`.
//     Ueberlappen zwei Abfragen, mischen sich zwei Listen — und niemand
//     merkt es, weil zehn Zeilen wie zehn Zeilen aussehen.
//
//   * Ein Dupe-Satz, der auf Verdacht warnt. "Schon gearbeitet" bei
//     unbekanntem Band verhindert einen Kontakt, den es noch nicht gibt.
//     Das faellt nie auf, weil nichts passiert.
//
//     node pruefe-logbuch.mjs

import { Sammelstelle, zeileLesen, befundLesen, zeitKurz, dupeSatz }
  from './logbuch.js';
import { feld } from './tci.js';

let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

const z = (nr, ruf, band = '20m', mode = 'CW') =>
  zeileLesen([String(nr), '20261002', '143000', ruf, band, mode, '59', '59']);

console.log('Sammelstelle\n');

// 1. Der gute Fall.
{
  const s = new Sammelstelle();
  s.beginnen();
  s.zeile(z(0, 'OE1AAA')); s.zeile(z(1, 'OE2BBB')); s.zeile(z(2, 'OE3CCC'));
  const l = s.abschluss(3);
  pruefe('drei Zeilen, drei angesagt -> Liste', Array.isArray(l) && l.length === 3);
  pruefe('Reihenfolge bleibt', l && l[0].ruf === 'OE1AAA' && l[2].ruf === 'OE3CCC');
}

// 2. Der Fall, um den es geht: zu wenige Zeilen angekommen.
{
  const s = new Sammelstelle();
  s.beginnen();
  s.zeile(z(0, 'OE1AAA')); s.zeile(z(1, 'OE2BBB'));
  pruefe('zwei Zeilen, drei angesagt -> nichts', s.abschluss(3) === null);
}

// 3. Zwei Abfragen ueberlappen. Die zweite beginnt bei 0 — und was von der
//    ersten noch kam, darf NICHT mitgezaehlt werden.
{
  const s = new Sammelstelle();
  s.beginnen();
  s.zeile(z(0, 'ALT1')); s.zeile(z(1, 'ALT2'));
  // Zweite Abfrage: Nummer 0 wirft die alte Liste weg.
  s.zeile(z(0, 'NEU1')); s.zeile(z(1, 'NEU2'));
  const l = s.abschluss(2);
  pruefe('neue Abfrage wirft die halbe alte weg',
         l && l.length === 2 && l[0].ruf === 'NEU1', JSON.stringify(l));
}

// 4. Gegenprobe zu 3: ohne das Zuruecksetzen bei Nummer 0 waeren vier
//    Zeilen in der Liste und `abschluss(2)` haette null geliefert — die
//    Liste waere also verschwunden, nicht falsch. Beides belegen:
{
  const s = new Sammelstelle();
  s.beginnen();
  s.zeile(z(0, 'A')); s.zeile(z(1, 'B')); s.zeile(z(2, 'C')); s.zeile(z(3, 'D'));
  pruefe('vier Zeilen, zwei angesagt -> nichts', s.abschluss(2) === null);
}

// 5. Eine Antwort ohne gesehenen Anfang (Seite neu geladen).
{
  const s = new Sammelstelle();
  s.zeile(z(0, 'OE1AAA'));
  pruefe('Nummer 0 oeffnet die Sammlung von selbst',
         (s.abschluss(1) || []).length === 1);
}

// 6. Eine Zeile mitten hinein, ohne Anfang: nicht sammeln.
{
  const s = new Sammelstelle();
  s.zeile(z(7, 'OE1AAA'));
  pruefe('Zeile ohne Anfang wird nicht gesammelt', s.abschluss(1) === null);
}

// 7. Muellzeilen.
{
  pruefe('Zeile ohne Rufzeichen -> null',
         zeileLesen(['0', '20261002', '143000', '', '20m', 'CW']) === null);
  pruefe('Zeile ohne Nummer -> null',
         zeileLesen(['x', '20261002', '143000', 'OE1AAA']) === null);
  pruefe('Rufzeichen wird gross', z(0, 'oe1aaa').ruf === 'OE1AAA');
}

console.log('\nZeit');
{
  pruefe('20261002/143000 -> 02.10. 14:30',
         zeitKurz('20261002', '143000') === '02.10. 14:30',
         zeitKurz('20261002', '143000'));
  pruefe('ohne Zeit nur das Datum',
         zeitKurz('20261002', '') === '02.10.', zeitKurz('20261002', ''));
  pruefe('ohne Datum nichts', zeitKurz('', '143000') === '');
  pruefe('Unfug ergibt nichts', zeitKurz('2026', '143000') === '');
}

console.log('\nDupe-Satz');

const bef = (o) => befundLesen([
  o.ruf ?? 'OE2BBB', String(o.anzahl ?? 0), o.datum ?? '', o.zeit ?? '',
  o.band ?? '', o.mode ?? '', o.gleichesBand ? '1' : '0', o.dupe ? '1' : '0',
]);

// 8. Noch nie gearbeitet — das muss sich wie eine Einladung lesen.
{
  const s = dupeSatz(bef({ anzahl: 0 }));
  pruefe('nie gearbeitet -> NEU', s.art === 'neu' && /NEU/.test(s.text), s.text);
}

// 9. DER Fall: bekannt, aber Band/Mode unbekannt -> KEIN Dupe.
{
  const s = dupeSatz(bef({ anzahl: 3, datum: '20261002', zeit: '143000',
                           band: '20m', mode: 'CW',
                           gleichesBand: false, dupe: false }));
  pruefe('bekannt ohne Bandtreffer ist kein Dupe', s.art === 'bekannt', s.art);
  pruefe('und sagt es auch so', !/DUPE/.test(s.text), s.text);
  pruefe('nennt die Anzahl', /3×/.test(s.text), s.text);
  pruefe('nennt, dass dieses Band fehlt', /noch nicht/.test(s.text), s.text);
}

// 10. Echter Dupe.
{
  const s = dupeSatz(bef({ anzahl: 2, datum: '20261002', zeit: '143000',
                           band: '20m', mode: 'CW',
                           gleichesBand: true, dupe: true }));
  pruefe('gleiches Band und Mode -> DUPE',
         s.art === 'dupe' && /DUPE/.test(s.text), s.text);
}

// 11. Gleiches Band, andere Betriebsart — kein Dupe, aber erwaehnenswert.
{
  const s = dupeSatz(bef({ anzahl: 1, band: '20m', mode: 'SSB',
                           gleichesBand: true, dupe: false }));
  pruefe('gleiches Band, anderer Mode ist kein Dupe',
         s.art === 'bekannt' && !/DUPE/.test(s.text), s.text);
  pruefe('sagt, dass die Betriebsart die andere ist',
         /Betriebsart/.test(s.text), s.text);
}

// 12. Leeres oder fehlendes Rufzeichen -> gar kein Satz. Eine Zeile unter
//     dem leeren Feld waere Laerm, und ein "NEU" unter einem leeren Feld
//     eine Behauptung ueber nichts.
{
  pruefe('kein Befund -> kein Satz', dupeSatz(null).text === '');
  pruefe('leeres Rufzeichen -> kein Satz',
         dupeSatz({ ruf: '', anzahl: 0 }).text === '');
  pruefe('befundLesen ohne Rufzeichen -> null', befundLesen(['']) === null);
}

console.log('\nWas in einen Befehl hinausgeht');
{
  // Befehle sind durch `;` getrennt, ihre Felder durch `,`. Steht eines
  // dieser Zeichen in einem Wert, wird aus EINEM Befehl ZWEI -- und der
  // zweite in der Zeile unten ist der Sendebefehl. Am anderen Ende haengt
  // eine Antenne.
  //
  // Erreichbar ist das nicht nur mit der Tastatur: `qsoOeffnenMit()` fuellt
  // das Rufzeichenfeld aus einem SPOT, und Spots kommen aus einem
  // Telnet-Strom von einem fremden Rechner.
  pruefe('ein echtes Rufzeichen bleibt, wie es ist', feld('OE5SOS') === 'OE5SOS');
  pruefe('ein Strich im Rufzeichen bleibt', feld('DL1YCF/P') === 'DL1YCF/P');
  pruefe('RST bleibt, wie es ist', feld('599 TU') === '599 TU');

  pruefe('das Semikolon geht hinaus', !feld('OE5SOS;trx:0,true').includes(';'));
  pruefe('das Komma geht hinaus', !feld('OE5SOS;trx:0,true').includes(','));
  pruefe('und der Sendebefehl ist kein Befehl mehr',
         feld('OE5SOS;trx:0,true') === 'OE5SOStrx:0true');

  pruefe('Zeilenumbruch geht hinaus', feld('OE5\nSOS') === 'OE5SOS');
  pruefe('Rand wird abgeschnitten', feld('  59  ') === '59');
  pruefe('nichts gibt nichts', feld(undefined) === '' && feld(null) === '');
  // Dieselbe Grenze wie TciFeld::kMaxZeichen auf der anderen Seite.
  pruefe('ein Unfall wird gekuerzt', feld('X'.repeat(5000)).length === 64);

  // Und der zusammengebaute Befehl bleibt EIN Befehl.
  const ruf = feld('OE5SOS;trx:0,true');
  const befehl = `log_qso:${ruf},${feld('59')},${feld('59')}`;
  pruefe('der Befehl bleibt einer', !befehl.includes(';'));
  pruefe('mit genau drei Feldern',
         befehl.slice('log_qso:'.length).split(',').length === 3);
}

console.log(fehler === 0 ? '\nAlles gut.' : `\n${fehler} Punkt(e) offen.`);
process.exit(fehler === 0 ? 0 : 1);
