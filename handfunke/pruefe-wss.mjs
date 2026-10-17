#!/usr/bin/env node
// Prueft die ganze Kette ueber TLS: wss -> Bruecke -> TCI-Server.
//
// Der Unterschied zu pruefe-bruecke-tls.py: dort geht ein roher Byteblock
// durch und wieder zurueck. Hier spricht ein echter WebSocket-Client, wie
// die Seite es taete — Handschlag, Aufruestung, Textrahmen. Dazwischen
// liegt alles, was im Betrieb auch dazwischenliegt.
//
// Nicht geprueft ist damit der Browser selbst: ob Safari dem Zertifikat
// traut und ob isSecureContext danach "ja" sagt, entscheidet sich am
// Geraet. Dieser Stand sagt nur, dass die Gegenseite stimmt.
//
//     NODE_EXTRA_CA_CERTS=~/Longpath/werkzeug/handfunke-tls/ca.crt \
//       node pruefe-wss.mjs 192.168.1.10:50001
//
// Ohne NODE_EXTRA_CA_CERTS scheitert es an der eigenen Stelle — und das
// ist richtig so: ein Lauf, der jedes Zertifikat nimmt, prueft nichts.

const adr = process.argv[2];
if (!adr) {
  console.error('Aufruf: node pruefe-wss.mjs <rechner>:<port>');
  process.exit(2);
}

const ws = new WebSocket(`wss://${adr}/`);
const zeilen = [];
const ende = (code, text) => { console.log(text); process.exit(code); };
const uhr = setTimeout(
  () => ende(1, `  FEHLT keine Begruessung in 8 s (${zeilen.length} Zeilen)`),
  8000);

ws.onopen = () => console.log('  ok    wss-Handschlag steht');

ws.onmessage = (e) => {
  if (typeof e.data !== 'string') { return; }
  zeilen.push(...e.data.split(';').filter(Boolean));
  // `ready` ist die letzte Zeile des Begruessungsschwalls — vorher ist der
  // Stand unvollstaendig, und ein Lauf, der zu frueh gruen meldet, haette
  // eine abgeschnittene Kette nicht bemerkt.
  if (!zeilen.some((z) => z.startsWith('ready'))) { return; }
  clearTimeout(uhr);
  console.log(`  ok    TCI-Begruessung kam durch (${zeilen.length} Zeilen)`);
  zeilen.filter((z) => /^(protocol|device|vfo:0,0|modulation)/.test(z))
        .slice(0, 4)
        .forEach((z) => console.log(`        ${z}`));
  ende(0, '\nalles gruen');
};

ws.onerror = (e) => { clearTimeout(uhr); ende(1, `  FEHLT ${e.message || e.type}`); };
ws.onclose = (e) => {
  if (zeilen.length) { return; }
  clearTimeout(uhr);
  ende(1, `  FEHLT Verbindung zu (${e.code}) — vertraut node der eigenen `
        + 'Stelle? NODE_EXTRA_CA_CERTS setzen.');
};
