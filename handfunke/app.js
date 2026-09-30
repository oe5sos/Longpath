// app.js — Longpath Handfunke.
//
// Die Seite haelt KEINEN eigenen Zustand: alles, was angezeigt wird, kommt aus
// einer Zeile des Servers. Ein Knopfdruck schickt einen Befehl und wartet auf
// die Gegenmeldung, statt die Anzeige vorher umzustellen. Das kostet einen
// Wimpernschlag und erspart die Klasse von Fehlern, bei der das Telefon etwas
// anderes behauptet als das Geraet tut.

import { TciLink, Fft } from './tci.js';

const $ = (id) => document.getElementById(id);
const link = new TciLink();
// Fuer die Fehlersuche im Pruefbrowser erreichbar machen. Schadet auf dem
// Telefon nicht und erspart beim Nachsehen jedes Mal eine Sonde.
window.__link = link;

// ── Baender: Anfang, Ende, Anzeigename ──────────────────────────────────────
// Nur die Baender, die Longpath auch kennt (IARU R1, Kurzwelle + 6 m).
const BAENDER = [
  { n: '160', von: 1810000,  bis: 2000000,  mitte: 1840000 },
  { n: '80',  von: 3500000,  bis: 3800000,  mitte: 3700000 },
  { n: '40',  von: 7000000,  bis: 7200000,  mitte: 7100000 },
  { n: '30',  von: 10100000, bis: 10150000, mitte: 10120000 },
  { n: '20',  von: 14000000, bis: 14350000, mitte: 14200000 },
  { n: '17',  von: 18068000, bis: 18168000, mitte: 18120000 },
  { n: '15',  von: 21000000, bis: 21450000, mitte: 21250000 },
  { n: '12',  von: 24890000, bis: 24990000, mitte: 24940000 },
  { n: '10',  von: 28000000, bis: 29700000, mitte: 28400000 },
  { n: '6',   von: 50000000, bis: 52000000, mitte: 50150000 },
];

// Filtervorgaben: bewusst NICHT erfunden, sondern dieselben Breiten, die
// Longpath seit #139/#140 aus Thetis uebernommen hat.
const FILTER = {
  ssb:   [[1800, '1.8k'], [2400, '2.4k'], [2700, '2.7k'], [3100, '3.1k'], [3900, '3.9k']],
  cw:    [[100, '100'], [250, '250'], [400, '400'], [800, '800'], [1000, '1.0k']],
  digi:  [[1000, '1.0k'], [1500, '1.5k'], [2400, '2.4k'], [3000, '3.0k']],
  am:    [[5000, '5.0k'], [8000, '8.0k'], [10000, '10k'], [16000, '16k']],
};

const state = {
  trx: 0,
  afPct: 70,
  wfRow: 0,
  spec: null,
  specMin: -130, specMax: -30,
  token: '',                 // nur für den Netzweg nötig
  audioRate: 12000,          // ausgehandelt; 48000 wäre Faktor 16 teurer
  tonTypPruefung: null,      // Nachfassen, falls mulaw8 nicht bestaetigt wird
  tonStartLaeuft: false,     // Riegel gegen doppelten AudioContext
  tonWeg: null,              // 'worklet' | 'scriptprocessor'
  tonFehler: null,           // Text fuer die Fusszeile, wenn kein Ton geht
  hatSpektrumstrom: false,   // Server liefert fertige Bins
  rueckfall: false,          // wir rechnen selbst aus rohem I/Q
  iqRueckfall: null,
  audio: null, node: null,
};

// ── Kopplung ────────────────────────────────────────────────────────────────
const gespeichert = () => { try { return localStorage.getItem('handfunke.adresse') || ''; } catch (e) { return ''; } };
const merken = (v) => { try { localStorage.setItem('handfunke.adresse', v); } catch (e) {} };
// Das Token wird nur gebraucht, wenn Longpath ins Netz gebunden ist; auf
// demselben Rechner (Loopback) verlangt der Server keines.
const tokenLesen  = () => { try { return localStorage.getItem('handfunke.token') || ''; } catch (e) { return ''; } };
const tokenMerken = (v) => { try { localStorage.setItem('handfunke.token', v); } catch (e) {} };

function starten(adresse) {
  const a = adresse.trim().replace(/^wss?:\/\//, '');
  if (!a) { $('fehler').textContent = 'Bitte eine Adresse eingeben.'; return; }
  const mitPort = /:\d+$/.test(a) ? a : a + ':50001';
  merken(mitPort);
  state.token = ($('token').value || '').trim().toUpperCase();
  tokenMerken(state.token);
  $('fehler').textContent = '';
  $('koppeln').classList.remove('an');
  link.connect('ws://' + mitPort);
}

$('verbinden').addEventListener('click', () => starten($('adresse').value));
$('adresse').addEventListener('keydown', (e) => { if (e.key === 'Enter') starten($('adresse').value); });
$('adresse').value = gespeichert();
$('token').value = tokenLesen();

// Beim Start: liegt eine Adresse vor, gleich versuchen — das Kopplungsblatt
// kommt von selbst zurueck, wenn es nicht klappt.
if (gespeichert()) starten(gespeichert());

// ── Anzeige ─────────────────────────────────────────────────────────────────
function hzText(hz) {
  if (!hz) return ['—', '', ''];
  const s = String(Math.round(hz)).padStart(9, '0');
  return [String(parseInt(s.slice(0, 3), 10)), '.' + s.slice(3, 6), '.' + s.slice(6, 9)];
}

function bandFuer(hz) {
  const b = BAENDER.find(b => hz >= b.von && hz <= b.bis);
  return b ? b.n + ' m' : '';
}

function modeArt(m) {
  const u = (m || '').toLowerCase();
  if (u.startsWith('cw')) return 'cw';
  if (u.startsWith('digl') || u.startsWith('digu') || u.startsWith('dig')) return 'digi';
  if (u === 'am' || u === 'sam' || u === 'fm' || u === 'nfm') return 'am';
  return 'ssb';
}

let letzterModus = '', letzteModi = '';

function zeichneBedienung() {
  const s = link.st;

  // Betriebsarten — die Liste kommt vom Server (modulations_list), nicht
  // von mir. Sonst zeigt die Seite Betriebsarten, die das Geraet nicht hat.
  const modi = (s.modes.length ? s.modes : ['lsb','usb','cw','digu','am']).slice(0, 5);
  const modiSchl = modi.join(',') + '|' + s.mode[state.trx];
  if (modiSchl !== letzteModi) {
    letzteModi = modiSchl;
    $('modes').innerHTML = '';
    modi.forEach(m => {
      const el = document.createElement('div');
      el.className = 'chip' + (m.toLowerCase() === (s.mode[state.trx] || '').toLowerCase() ? ' on' : '');
      el.textContent = m.toUpperCase();
      el.onclick = () => link.send(`modulation:${state.trx},${m.toLowerCase()}`);
      $('modes').appendChild(el);
    });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('modes').appendChild(mehr);
  }

  // Baender — fuenf um das aktuelle herum, damit der Daumen nicht sucht.
  const hz = s.vfo[state.trx][0];
  const idx = Math.max(0, BAENDER.findIndex(b => hz >= b.von && hz <= b.bis));
  const von = Math.min(Math.max(0, idx - 2), BAENDER.length - 5);
  const sicht = BAENDER.slice(von, von + 5);
  const bandSchl = sicht.map(b => b.n).join(',') + '|' + idx;
  if (bandSchl !== $('bands').dataset.schl) {
    $('bands').dataset.schl = bandSchl;
    $('bands').innerHTML = '';
    sicht.forEach(b => {
      const el = document.createElement('div');
      el.className = 'chip' + (hz >= b.von && hz <= b.bis ? ' on' : '');
      el.textContent = b.n;
      el.onclick = () => link.send(`vfo:${state.trx},0,${b.mitte}`);
      $('bands').appendChild(el);
    });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('bands').appendChild(mehr);
  }

  // Filter — die Breiten haengen an der Betriebsart.
  const art = modeArt(s.mode[state.trx]);
  const breite = Math.abs((s.filter[state.trx][1] || 0) - (s.filter[state.trx][0] || 0));
  const fSchl = art + '|' + breite;
  if (fSchl !== $('filters').dataset.schl) {
    $('filters').dataset.schl = fSchl;
    $('filters').innerHTML = '';
    FILTER[art].forEach(([w, name]) => {
      const el = document.createElement('div');
      el.className = 'chip' + (Math.abs(breite - w) < 60 ? ' on' : '');
      el.textContent = name;
      el.onclick = () => {
        // CW liegt um den Tonversatz, Sprache ab 100 Hz — wie am Pult.
        const lo = art === 'cw' ? Math.round(-w / 2) : 100;
        const hi = art === 'cw' ? Math.round(w / 2) : 100 + w;
        link.send(`rx_filter_band:${state.trx},${lo},${hi}`);
      };
      $('filters').appendChild(el);
    });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('filters').appendChild(mehr);
  }

  // Aufbereitung — Zustandsschalter bleiben leise (nur Rand), damit im Feld
  // genau eine Stelle kraeftig ist: die Sendetaste.
  if (!$('dsp').dataset.fertig) {
    $('dsp').dataset.fertig = '1';
    [['NB','rx_nb_enable'],['NR','rx_nr_enable'],['ANF','rx_anf_enable'],['SQL','sql_enable']]
      .forEach(([name, cmd]) => {
        const el = document.createElement('div');
        el.className = 'chip quiet'; el.textContent = name; el.dataset.cmd = cmd;
        el.onclick = () => {
          const an = el.classList.contains('on');
          link.send(`${cmd}:${state.trx},${an ? 'false' : 'true'}`);
          el.classList.toggle('on');   // Der Server bestaetigt nicht jeden
        };                             // dieser Schalter — hier ist die
        $('dsp').appendChild(el);      // Anzeige notgedrungen lokal.
      });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('dsp').appendChild(mehr);
  }

  // Sendeleistung — der Regler, der bis PR #145 wirkungslos war.
  if (s.drive !== null) {
    $('pwrFill').style.width = s.drive + '%';
    $('pwrKnob').style.left  = s.drive + '%';
    $('pwrVal').textContent  = s.drive + ' %';
  }
  $('afFill').style.width = state.afPct + '%';
  $('afKnob').style.left  = state.afPct + '%';
  $('afVal').textContent  = state.afPct + ' %';
}

function zeichneKopf() {
  const s = link.st;
  const verbunden = link.ws && link.ws.readyState === 1;
  // Dieselbe Lebendigkeitspruefung wie in der Fusszeile: nicht der Socket
  // entscheidet, ob es laeuft, sondern ob Daten kommen.
  const totStill = link.ready && link.letzteDaten
                 && (performance.now() - link.letzteDaten > 3000);
  $('led').className = 'dot' + (!verbunden ? ' off'
                              : (link.ready && !totStill) ? '' : ' wait');
  $('station').textContent = verbunden
    ? ('LONGPATH' + (s.device ? ' · ' + s.device.toUpperCase() : ''))
    : 'NICHT VERBUNDEN';

  const [mhz, khz, hz] = hzText(s.vfo[state.trx][0]);
  $('hz').innerHTML = `${mhz}<span class="khz">${khz}</span><span class="dez">${hz}</span>`;
  $('band').textContent = bandFuer(s.vfo[state.trx][0]);

  const sm = s.smeter[state.trx];
  if (sm !== null) {
    $('smeter').textContent = `${sEinheit(sm)} · ${Math.round(sm)} dBm`;
    // −127 dBm = S1, 6 dB je S-Stufe, S9 = −73, darueber bis +40 dB.
    // Gleiche Skala wie die Beschriftung darunter: S1 = -121 … S9+40 = -33.
    $('sbar').style.width = Math.max(0, Math.min(100, ((sm + 121) / 88) * 100)) + '%';
  }
}

function sEinheit(dbm) {
  // IARU R1 fuer Kurzwelle: S9 = -73 dBm, je S-Stufe 6 dB, also S1 = -121 dBm.
  // (Erst falsch gebaut mit -127 als S1 — das verschob die ganze Skala um
  // zwei Stufen nach oben, -92 dBm stand als S7 statt S6 da.)
  if (dbm >= -73) return 'S9+' + Math.round(dbm + 73);
  return 'S' + Math.max(1, Math.min(9, Math.floor((dbm + 121) / 6) + 1));
}

// ── Panadapter + Wasserfall ─────────────────────────────────────────────────
const pan = $('pan'), panCtx = pan.getContext('2d');
const wf = $('wf'), wfCtx = wf.getContext('2d', { willReadFrequently: false });
const N = 1024;
const fft = new Fft(N);
const spec = new Float32Array(N);
const glatt = new Float32Array(N);        // eigene FFT (Rückfall)
let specServer = new Float32Array(0);     // fertig vom Server (Regelfall)
let hatSpektrum = false;

// Die Wasserfallrampe „Gedaempft" mit genau den sieben Stuetzpunkten aus
// SpectrumWidget.cpp — Grundrauschen verschwindet, Waerme erst oben.
const STOPS = [[0,[8,8,10]],[.30,[12,12,14]],[.48,[44,44,49]],[.64,[88,88,94]],
               [.78,[129,123,92]],[.92,[216,165,95]],[1,[242,242,236]]];
function rampe(t) {
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  for (let i = 1; i < STOPS.length; i++) {
    if (t <= STOPS[i][0]) {
      const [a, ca] = STOPS[i - 1], [b, cb] = STOPS[i], k = (t - a) / (b - a);
      return [ca[0] + (cb[0]-ca[0])*k, ca[1] + (cb[1]-ca[1])*k, ca[2] + (cb[2]-ca[2])*k];
    }
  }
  return [242, 242, 236];
}

const zeile = wfCtx.createImageData(wf.width, 1);
const wfPuff = document.createElement('canvas');
wfPuff.width = wf.width; wfPuff.height = wf.height;
const wfPuffCtx = wfPuff.getContext('2d');

let letzteIq = 0;
let neueZeilen = 0;      // vom Datenstrom gefuellt, von der Zeichenschleife geleert
link.addEventListener('iq', (e) => {
  const v = e.detail.vals;
  if (v.length < N * 2) return;
  letzteIq = performance.now();
  // Hoechstens 25 Zeilen je Sekunde: darueber sieht das Auge ohnehin nichts,
  // und ein Schub aus dem Netz soll den Wasserfall nicht durchreissen.
  if (neueZeilen < 3) neueZeilen++;
  fft.spectrum(v.subarray(v.length - N * 2), spec);
  // Traegheit wie am Pult: das Bild soll nicht flackern.
  for (let i = 0; i < N; i++) {
    glatt[i] = hatSpektrum ? glatt[i] * 0.6 + spec[i] * 0.4 : spec[i];
  }
  hatSpektrum = true;
});

function zeichneBild() {
  const W = pan.width, H = pan.height;

  // Ohne Gerät kommt kein I/Q. Das muss dastehen, statt dass der Bediener
  // vor einer leeren Fläche sitzt und rät, ob die Verbindung hängt oder die
  // Station schweigt — am Telefon sieht man den Unterschied sonst nicht.
  const still = !hatSpektrum || (performance.now() - letzteIq > 2000);
  if (still) { state.hatSpektrumstrom = false; }
  if (still) {
    panCtx.fillStyle = '#141e27'; panCtx.fillRect(0, 0, W, H);
    wfCtx.fillStyle = '#0c0c0e';  wfCtx.fillRect(0, 0, wf.width, wf.height);
    panCtx.fillStyle = '#7e7e85';
    panCtx.font = '11px -apple-system,system-ui,sans-serif';
    panCtx.textAlign = 'center';
    panCtx.fillText(link.ready ? 'kein Funkgerät verbunden' : 'warte auf Longpath…',
                    W / 2, H / 2 + 4);
    panCtx.textAlign = 'left';
    hatSpektrum = false;
    return;
  }

  // Selbsttaetige Hoehenlage: der Boden auf das 10. Hundertstel, die Decke
  // ueber die Spitze. Sonst muesste der Bediener am Handy Regler suchen.
  // Zwei Quellen, ein Bild: kommt das Spektrum fertig vom Server, ist es
  // schon auf unsere Breite verdichtet; rechnen wir selbst, liegen hier die
  // eigenen FFT-Bins. Die Wahl muss VOR der Höhenlage stehen — sie rechnet
  // schon damit.
  const quelle = state.hatSpektrumstrom ? specServer : glatt;
  const M = quelle.length;
  if (M === 0) { return; }

  // Boden aus dem Mittelwert (das Grundrauschen), Decke aus der Spitze —
  // nicht Minimum/Maximum, sonst bestimmt ein einzelner Einbruch den Boden.
  let summe = 0, max = -Infinity;
  for (let i = 0; i < M; i++) { summe += quelle[i]; if (quelle[i] > max) max = quelle[i]; }
  const min = summe / M;
  state.specMin = state.specMin * 0.9 + (min - 6) * 0.1;
  state.specMax = state.specMax * 0.9 + (max + 8) * 0.1;
  const lo = state.specMin, hi = Math.max(state.specMax, lo + 20);
  const y = (db) => H - ((db - lo) / (hi - lo)) * H;

  panCtx.fillStyle = '#141e27'; panCtx.fillRect(0, 0, W, H);
  panCtx.strokeStyle = 'rgba(138,143,150,.16)'; panCtx.lineWidth = 1;
  for (let i = 1; i < 6; i++) { const yy = Math.round(H*i/6)+.5;
    panCtx.beginPath(); panCtx.moveTo(0, yy); panCtx.lineTo(W, yy); panCtx.stroke(); }
  for (let i = 1; i < 8; i++) { const xx = Math.round(W*i/8)+.5;
    panCtx.beginPath(); panCtx.moveTo(xx, 0); panCtx.lineTo(xx, H); panCtx.stroke(); }

  // Auf die Bildpunkte bringen. Kommt das Spektrum fertig vom Server, ist es
  // schon auf unsere Breite verdichtet und das hier streckt nur noch; rechnen
  // wir selbst, verdichtet es 1024 Bins — über den Spitzenwert, sonst
  // rutschen schmale Träger durch.
  const spitze = new Float32Array(W);
  for (let x = 0; x < W; x++) {
    const von = Math.floor(x * M / W);
    const bis = Math.max(von + 1, Math.floor((x + 1) * M / W));
    let m = -Infinity;
    for (let i = von; i < bis && i < M; i++) if (quelle[i] > m) m = quelle[i];
    spitze[x] = m;
  }

  const g = panCtx.createLinearGradient(0, 0, 0, H);
  g.addColorStop(0, 'rgba(194,146,79,.22)'); g.addColorStop(1, 'rgba(194,146,79,0)');
  panCtx.beginPath(); panCtx.moveTo(0, H);
  for (let x = 0; x < W; x++) panCtx.lineTo(x, y(spitze[x]));
  panCtx.lineTo(W, H); panCtx.closePath(); panCtx.fillStyle = g; panCtx.fill();

  panCtx.beginPath();
  for (let x = 0; x < W; x++) {
    const yy = y(spitze[x]);
    x ? panCtx.lineTo(x, yy) : panCtx.moveTo(x, yy);
  }
  panCtx.strokeStyle = '#c2924f'; panCtx.lineWidth = 1; panCtx.stroke();

  panCtx.fillStyle = '#9aa0a8'; panCtx.font = '9px ui-monospace,Menlo,monospace';
  for (let i = 0; i <= 3; i++) {
    const db = Math.round(hi - (hi - lo) * i / 3);
    panCtx.fillText(String(db), 3, 10 + i * (H - 14) / 3);
  }

  // Wasserfall: je eingetroffenem Spektrum eine Zeile — nicht je Bild.
  // Der Zwischenpuffer ist noetig, weil ein Canvas, der sich selbst als Quelle
  // zeichnet, nicht verlaesslich schiebt.
  while (neueZeilen > 0) {
    neueZeilen--;
    wfPuffCtx.clearRect(0, 0, wf.width, wf.height);
    wfPuffCtx.drawImage(wf, 0, 0);
    wfCtx.clearRect(0, 0, wf.width, wf.height);
    wfCtx.drawImage(wfPuff, 0, 1);
    // Der Wasserfall bekommt eine EIGENE, engere Spanne. Sobald der Server
    // echte dBm liefert, reicht die Panadapter-Spanne über 85 dB — ein
    // Träger 30 dB über dem Rauschen läge dann im dunklen Drittel der Rampe
    // und wäre kaum zu sehen. Am Pult haben Panadapter und Wasserfall aus
    // demselben Grund getrennte Regler; hier nehmen wir den Rauschboden plus
    // 55 dB, was in der Praxis vom Grundrauschen bis zum lauten Träger reicht.
    const wfLo = lo, wfHi = lo + 55;
    const d = zeile.data;
    for (let x = 0; x < wf.width; x++) {
      const db = spitze[Math.min(W - 1, Math.floor(x * W / wf.width))];
      const c = rampe((db - wfLo) / (wfHi - wfLo));
      const o = x * 4;
      d[o] = c[0]; d[o+1] = c[1]; d[o+2] = c[2]; d[o+3] = 255;
    }
    wfCtx.putImageData(zeile, 0, 0);
  }
}

// Der AF-Regler des Telefons — mit VERSTAERKUNGSVORRAT.
//
// Bis 2026-09-30 war er `afPct/100`, also 0 bis 1,0: ein reiner Daempfer
// ohne jede Reserve. Das ist fuer eine Fernbedienung der falsche Bereich,
// denn der Ton kommt schon dem AF-Regler AM RECHNER unterworfen an — der
// TCI-Abgriff sitzt hinter WDSPs panel.gain1 (RXA.c:698). Steht der Regler
// am Mac leise, weil der Bediener dort gerade nicht zuhoert, kommt das
// Telefon leise an und konnte nichts dagegen tun.
//
// Jetzt: 70 % bleibt Faktor 1,0 (die gewohnte Stellung klingt wie bisher),
// darunter wird gedaempft, darueber bis Faktor 4 (+12 dB) verstaerkt. Der
// Bereich oberhalb ist bewusst gespreizt, damit das letzte Stueck Weg nicht
// in einem Sprung liegt.
function afFaktor(pct) {
  if (pct <= 70) { return pct / 70; }                 // 0 … 1,0
  return 1 + (pct - 70) / 30 * 3;                     // 1,0 … 4,0
}

// ── Ton ─────────────────────────────────────────────────────────────────────
// iOS gibt Ton erst nach einer Beruehrung frei. Wir versuchen es bei jeder
// Beruehrung erneut, bis es klappt — ohne einen eigenen Knopf dafuer.
async function tonStarten() {
  // Riegel gegen Doppelstart. `state.node` allein reicht NICHT: zwischen der
  // Pruefung und dem Setzen liegen zwei await-Stellen, und ein einziger Tipp
  // loest ueber 'touchend' UND 'click' zwei Laeufe aus. Ergebnis waren
  // zuverlaessig zwei AudioContexts, von denen der erste nie wieder
  // geschlossen wurde (Durchsicht 2026-09-30).
  if (state.node || state.tonStartLaeuft) { return; }
  state.tonStartLaeuft = true;
  try {
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) { throw new Error('Dieser Browser kennt keinen AudioContext'); }
    const ctx = new AC({ sampleRate: 48000, latencyHint: 'interactive' });

    const gain = ctx.createGain();
    gain.gain.value = afFaktor(state.afPct);

    let node = null;
    let weg = null;

    if (ctx.audioWorklet) {
      // Der gute Weg: eigener Audio-Faden, stottert nicht, wenn der
      // Hauptfaden den Wasserfall zeichnet.
      await ctx.audioWorklet.addModule('ton-kern.js');   // muss zuerst
      await ctx.audioWorklet.addModule('rx-worklet.js');
      node = new AudioWorkletNode(ctx, 'rx-worklet', {
        outputChannelCount: [2],
        // srcRate ist die ausgehandelte Rate, nicht die des Ausgangs — ohne
        // sie liefe der Ton bei 12 kHz Quelle viermal zu schnell.
        processorOptions: { srcRate: state.audioRate },
      });
      // Leerlauf zaehlen statt wegwerfen. Das Worklet meldet bei jedem 32.
      // leeren Block; ohne diese Zeile war die einzige Diagnose fuer
      // stotternden Ton ein leerer Rumpf, und der Leerlauf musste von Hand
      // ueber den Scheitelfaktor ausgeschlossen werden.
      node.port.onmessage = (e) => {
        if (e.data && e.data.type === 'starved') {
          state.tonLeerlauf = (state.tonLeerlauf || 0) + 32;
          state.tonLeerlaufZuletzt = performance.now();
        }
      };
      weg = 'worklet';
    } else {
      // ── Rueckfall: ScriptProcessorNode ───────────────────────────────────
      //
      // `AudioWorklet` ist [SecureContext]. Eine Seite, die ueber http von
      // einer LAN-Adresse kommt — also genau der Fall "Telefon im eigenen
      // Netz" —, ist KEIN sicherer Kontext, und dann ist ctx.audioWorklet
      // schlicht undefined. Am 2026-09-30 an http://172.30.30.121:8767
      // gemessen: isSecureContext false, audioWorklet undefined,
      // createScriptProcessor vorhanden.
      //
      // Der ScriptProcessorNode ist veraltet und laeuft im Hauptfaden, kann
      // also unter Last knacken. Fuer 12 kHz Sprache reicht er, und stumm
      // ist erheblich schlimmer als gelegentlich rauh. Sobald die Seite
      // ueber https ausgeliefert wird, greift von selbst wieder der obere
      // Zweig.
      const kern = new TonKern({ ausgabeRate: ctx.sampleRate, srcRate: state.audioRate });
      // 2048 Proben sind bei 48 kHz rund 43 ms — gross genug, dass der
      // Hauptfaden dazwischen zeichnen darf, klein genug, dass die
      // Verzoegerung nicht auffaellt.
      const sp = ctx.createScriptProcessor(2048, 0, 2);
      let leerZaehler = 0;
      sp.onaudioprocess = (ev) => {
        const out = ev.outputBuffer;
        const voll = kern.zieh(out.getChannelData(0), out.getChannelData(1), out.length);
        // Denselben Leerlauf melden wie das Worklet, damit die Fusszeile auf
        // beiden Wegen dasselbe sagt.
        if (!voll && !kern.muted && (++leerZaehler & 31) === 0) {
          state.tonLeerlauf = (state.tonLeerlauf || 0) + 32;
          state.tonLeerlaufZuletzt = performance.now();
        }
      };
      // Eine Huelle, die sich nach aussen wie der Worklet-Knoten verhaelt —
      // so kennt der Rest der Seite nur EINEN Weg.
      node = {
        _sp: sp, _kern: kern,
        connect: (z) => sp.connect(z),
        disconnect: () => sp.disconnect(),
        port: { postMessage: (m) => {
          if (m.type === 'pcm') { kern.push(m.data, m.channels); }
          else if (m.type === 'mute') { kern.muted = !!m.value; }
          else if (m.type === 'flush') { kern.leeren(); }
          else if (m.type === 'rate') { kern.setRate(m.value); }
        } },
      };
      weg = 'scriptprocessor';
    }

    // ── Begrenzer, damit der Verstärkungsvorrat nicht klirrt ──────────────
    //
    // Seit der AF-Regler bis Faktor 4 verstärkt, kann ein starkes Signal die
    // Vollaussteuerung überschreiten — die AGC des Empfängers liefert in der
    // Regelung Spitzen um 0,98, mal 4 sind das 3,9. Digitales Klirren klingt
    // scheußlich und wird für einen Fehler des Programms gehalten.
    //
    // Der Begrenzer greift erst bei -3 dBFS, arbeitet mit Verhältnis 20:1 und
    // ohne Kniebereich: unterhalb tut er nichts, oberhalb hält er. Das ist
    // kein Klangeingriff, sondern ein Deckel — Kompression im eigentlichen
    // Sinn (Dynamik verdichten) wäre eine Gestaltungsfrage und steht hier
    // ausdrücklich nicht.
    const deckel = ctx.createDynamicsCompressor();
    deckel.threshold.value = -3;
    deckel.knee.value = 0;
    deckel.ratio.value = 20;
    deckel.attack.value = 0.003;
    deckel.release.value = 0.25;

    node.connect(gain);
    gain.connect(deckel);
    deckel.connect(ctx.destination);
    await ctx.resume();

    state.audio = ctx; state.node = node; state.gain = gain;
    state.tonWeg = weg; state.tonFehler = null;
    // Fuer die Fehlersuche erreichbar (siehe window.__link oben).
    window.__audioCtx = ctx; window.__gain = gain; window.__node = node;
  } catch (e) {
    // Kein Ton ist kein Grund, die Bedienung zu verlieren — aber er darf
    // auch nicht STILL fehlen. Genau das war der Fall: ein console.warn, und
    // die Seite tat weiter, als liefe alles. Die gemessenen Datenraten haben
    // dann ankommende Bytes belegt, nicht hoerbaren Ton.
    state.tonFehler = (window.isSecureContext === false)
      ? 'kein Ton — die Seite laeuft ohne sicheren Kontext'
      : ('kein Ton — ' + (e && e.message ? e.message : e));
    console.warn('Ton nicht verfuegbar:', e);
  } finally {
    state.tonStartLaeuft = false;
  }
}
['touchend', 'click'].forEach(ev =>
  document.addEventListener(ev, tonStarten, { passive: true }));

link.addEventListener('audio', (e) => {
  if (!state.node) return;
  // Kanalzahl aus dem Rahmen ableiten, nicht raten: wir bitten um mono, aber
  // ein Server darf stereo schicken.
  const kanaele = e.detail.channels || 1;
  state.node.port.postMessage(
    { type: 'pcm', data: e.detail.vals, channels: kanaele }, [e.detail.vals.buffer]);
});

// ── Regler ──────────────────────────────────────────────────────────────────
function reglerBinden(trackId, beiWert) {
  const track = $(trackId);
  const setzen = (ev) => {
    const r = track.getBoundingClientRect();
    const x = (ev.touches ? ev.touches[0].clientX : ev.clientX) - r.left;
    beiWert(Math.max(0, Math.min(100, Math.round(x / r.width * 100))));
  };
  track.addEventListener('pointerdown', (e) => { track.setPointerCapture(e.pointerId); setzen(e); });
  track.addEventListener('pointermove', (e) => { if (track.hasPointerCapture(e.pointerId)) setzen(e); });
}
reglerBinden('afTrack', (p) => {
  state.afPct = p;
  if (state.gain) state.gain.gain.value = afFaktor(p);
  zeichneBedienung();
});
reglerBinden('pwrTrack', (p) => link.send(`drive:${state.trx},${p}`));

// ── Abstimmen durch Wischen im Spektrum ─────────────────────────────────────
// Eine Wischbewegung verschiebt die Frequenz um so viele Hertz, wie sie
// Bildpunkte zuruecklegt — bezogen auf die tatsaechliche Abtastrate.
let wischVon = null, wischHz = 0, wischAktiv = false, wischLetzt = 0, wischZiel = null;

// Ab wie vielen Bildpunkten ein Wisch als Wisch gilt. Darunter war es ein
// Tipp, und ein Tipp darf die Frequenz NICHT verstellen: ein Daumen trifft
// nie auf den Punkt genau, und bis 2026-09-30 verstimmte jede Beruehrung des
// Wasserfalls das Geraet (Durchsicht, 3/3 bestaetigt).
const kWischTotzone = 10;
// Nicht bei jedem Bewegungsereignis senden. Ein Telefon liefert 60 bis 120
// davon je Sekunde, und jedes war ein eigener vfo-Befehl ueber die Leitung —
// beim langsamen Ziehen ueber das Band waren das Hunderte in Folge.
const kWischAbstandMs = 60;

$('scope').addEventListener('pointerdown', (e) => {
  wischVon = e.clientX; wischHz = link.st.vfo[state.trx][0];
  wischAktiv = false; wischLetzt = 0; wischZiel = null;
  $('scope').setPointerCapture(e.pointerId);
});
$('scope').addEventListener('pointermove', (e) => {
  if (wischVon === null) return;
  const weg = e.clientX - wischVon;
  if (!wischAktiv) {
    if (Math.abs(weg) < kWischTotzone) return;
    wischAktiv = true;
  }
  const spanne = link.st.iqRate || 192000;
  const proPixel = spanne / $('scope').getBoundingClientRect().width;
  const neu = Math.round((wischHz - weg * proPixel) / 10) * 10;
  wischZiel = neu;
  const jetzt = performance.now();
  if (jetzt - wischLetzt < kWischAbstandMs) return;
  wischLetzt = jetzt;
  link.send(`vfo:${state.trx},0,${neu}`);
});
$('scope').addEventListener('pointerup', () => {
  // Den letzten Stand nachreichen: durch die Drosselung kann bis zu ein
  // Intervall Wegstrecke ungesendet geblieben sein, und dann steht das
  // Geraet ein Stueck neben dem, wo der Finger losgelassen hat.
  if (wischAktiv && wischZiel !== null) { link.send(`vfo:${state.trx},0,${wischZiel}`); }
  wischVon = null; wischAktiv = false; wischZiel = null;
});
$('scope').addEventListener('pointercancel', () => { wischVon = null; wischAktiv = false; wischZiel = null; });

// ── Abstimmtraeger: bewusst KEIN Knopf ──────────────────────────────────────
//
// Hier stand bis zum 2026-09-30 ein Klickhorcher, der `tune:N,true` schickte.
// Das war ein Fehler, und zwar ein sendender: der Abstimmtraeger legt einen
// Dauertraeger mit voller Leistung auf die Antenne. Er heisst nur nicht
// „senden", deshalb ist er beim Nachdenken ueber die Sendesperre durchgerutscht
// — waehrend die SENDEN-Taste daneben ausdruecklich tot war.
//
// Dazu kam, was die Durchsicht am selben Tag am Server fand: der tune-Weg war
// der einzige Sendeweg ohne Besitzer, Wachhund und Sendezeit-Deckel (das ist
// jetzt behoben, TciServer.cpp). Ein Telefon, dessen WLAN abreisst oder das
// iOS einfriert, haette den Traeger unbegrenzt stehen lassen.
//
// Die Handfunke ist zum Hoeren und Bedienen. Beide Sendetasten sind tot und
// sehen auch so aus. Soll das Telefon eines Tages senden duerfen, ist das eine
// bewusste Erweiterung mit eigener Zustandsmeldung vom Server und einer
// Sicherung gegen Fehltipp (langes Druecken) — nicht ein Knopf, der still
// funktioniert, weil die Verbindung zufaellig ueber Loopback lief.

// ── Ereignisse vom Draht ────────────────────────────────────────────────────
link.addEventListener('open', () => {
  // Anmelden, falls ein Token hinterlegt ist. Auf Loopback verlangt der Server
  // keines und verwirft die Zeile stillschweigend — schadet also nicht.
  // Aus dem Netz beantwortet er BIS DAHIN nichts, deshalb muss das hier ganz
  // vorne stehen, vor jeder Anforderung.
  if (state.token) { link.send(`auth:${state.token}`); }

  // ── Tonformat aushandeln, BEVOR der Strom startet ────────────────────────
  //
  // 48 kHz float32 stereo sind 384 kB/s. 12 kHz int16 mono sind 24 — Faktor
  // sechzehn, ohne einen einzigen Codec. Die Bandbreite von 6 kHz reicht für
  // alles, was ein Empfänger an Sprache herausgibt (SSB endet bei 3 kHz), und
  // mono ist richtig, weil hier ohnehin ein Empfänger zu hören ist.
  //
  // Erst danach lohnt Opus — es würde die verbleibenden 24 auf etwa 4 kB/s
  // drücken, kostet aber eine Bibliothek im Browser.
  // ── Messwerte anfordern ──────────────────────────────────────────────────
  //
  // Longpath schickt sie NICHT von selbst: `rxSensorsEnabled` steht ab Werk
  // auf false und wird erst durch diese Zeile wahr (TciClientSession.h:235).
  // Das ist Thetis-getreu — dort verlangt `setRxSensorsEnabled` ebenfalls
  // eine ausdrueckliche Anmeldung. Ohne sie bleibt der Signalbalken leer,
  // und genau das war am echten Geraet zu sehen, bevor diese Zeile stand.
  //
  // 200 ms ist die Server-Vorgabe und zugleich sein unteres Ende der
  // sinnvollen Spanne; schneller braucht ein Balken nicht zu sein, und
  // langsamer wirkt er traege.
  link.send(`rx_sensors_enable:true,200`);
  link.send(`tx_sensors_enable:true,200`);

  link.send(`audio_samplerate:${state.audioRate}`);
  link.send(`audio_stream_channels:1`);
  // mu-law: ein Byte je Abtastung statt zwei. Bei 12 kHz mono sind das
  // 13,3 statt 24,9 kB/s — die Haelfte des gesamten Datenverbrauchs, ohne
  // eine fremde Bibliothek auf einer der beiden Seiten. Was man hoert, ist
  // an echtem Kurzwellenton gemessen gleichwertig (37,7 dB Stoerabstand,
  // unter dem Bandrauschen jedes Empfaengers).
  //
  // Ein fremder Server kennt den Namen nicht. Longpath sagt dann im
  // Protokoll Bescheid und bleibt beim bisherigen Format; frueher fiel es
  // still auf float32 — das Achtfache. Unten wird deshalb geprueft, was
  // wirklich gilt.
  link.send(`audio_stream_sample_type:mulaw8`);

  // Nachfassen: bestaetigt der Server nicht mulaw8, auf int16 zurueck. Ohne
  // das haengt der Ton an der Hoffnung, dass die Gegenseite den Namen kennt.
  // Nachfassen: kommen nach drei Sekunden KEINE mu-law-Rahmen an, auf int16
  // zurueck. Geprueft wird am Binaerkopf (audioTypRahmen), nicht am
  // Text-Echo — das kommt aus dem globalen RadioModel und meldet, was
  // irgendwo eingestellt ist, nicht was diese Verbindung bekommt. Am
  // 2026-09-30 live in die Falle getappt: das Echo sagte int16, die Seite
  // schaltete daraufhin selbst zurueck, und der billige Ton kam nie zum
  // Einsatz, obwohl der Server ihn geliefert haette.
  clearTimeout(state.tonTypPruefung);
  state.tonTypPruefung = setTimeout(() => {
    if (link.st.audioTypRahmen !== null && link.st.audioTypRahmen !== 101) {
      link.send(`audio_stream_sample_type:int16`);
    }
  }, 3000);

  link.send(`audio_start:${state.trx}`);

  // Zuerst das FERTIGE Spektrum: die Punktzahl ist unsere Breite, mehr kann
  // der Schirm nicht zeigen. Gegen rohes I/Q spart das den Faktor sechzig.
  // Ein fremder Server (Thetis, ExpertSDR) kennt den Befehl nicht und
  // verwirft ihn antwortlos — deshalb steht darunter der Rückfall.
  link.send(`spectrum_start:${state.trx},${pan.width},12`);

  // Rückfall auf rohes I/Q — aber nur, wenn der Server den Befehl gar nicht
  // KENNT, nicht schon dann, wenn gerade keine Bilder kommen.
  //
  // Bis 2026-09-30 hing der Rückfall am Ausbleiben von Daten. Beim Start
  // ohne verbundenes Funkgerät kamen zwei Sekunden lang keine — und das
  // Telefon zog daraufhin rohes I/Q, das Vielfache an Daten, obwohl der
  // Server das fertige Bild sehr wohl liefern konnte, sobald ein Gerät dran
  // war. Am echten Gerät im Log gesehen: „IQ stream subscribed rx 0" direkt
  // nach einem Start, bei dem die Verbindung noch im Aufbau war.
  //
  // Longpath bestätigt das Abonnement mit den geltenden Werten zurück; ein
  // fremder Server (Thetis, ExpertSDR) schweigt dazu. Das ist der richtige
  // Prüfstein.
  clearTimeout(state.iqRueckfall);
  state.iqRueckfall = setTimeout(() => {
    if (!link.st.spektrumBestaetigt) {
      link.send(`iq_start:${state.trx}`);
      state.rueckfall = true;
    }
  }, 2000);
});

// Fertiges Spektrum vom Server — das Telefon rechnet dann gar nichts mehr.
link.addEventListener('spectrum', (e) => {
  const v = e.detail.vals;
  if (!v.length) return;
  state.hatSpektrumstrom = true;
  letzteIq = performance.now();
  if (neueZeilen < 3) neueZeilen++;

  // Die Punktzahl bestimmt der Server (er klemmt unsere Bitte). Also nicht
  // auf N festnageln, sondern nehmen, was kommt.
  if (specServer.length !== v.length) { specServer = new Float32Array(v.length); }
  for (let i = 0; i < v.length; i++) {
    specServer[i] = hatSpektrum ? specServer[i] * 0.6 + v[i] * 0.4 : v[i];
  }
  hatSpektrum = true;
});
link.addEventListener('ready', () => zeichneBedienung());
link.addEventListener('state', () => {
  // Der Server hat das letzte Wort über die Rate. Weicht sie von unserer
  // Bitte ab, muss das Worklet es erfahren — sonst stimmt die Tonhöhe nicht.
  const r = link.st.audioRate;
  if (r && r !== state.audioRate) {
    state.audioRate = r;
    if (state.node) { state.node.port.postMessage({ type: 'rate', value: r }); }
  }
  zeichneKopf(); zeichneBedienung();
});

// Verbindung weg: Kopplungsblatt zurueckholen, aber erst nach einer Weile —
// ein kurzer Aussetzer soll den Bediener nicht aus der Bedienung werfen.
let wegSeit = 0;
setInterval(() => {
  const offen = link.ws && link.ws.readyState === 1;
  if (!offen && link.wanted) {
    if (!wegSeit) wegSeit = Date.now();
    if (Date.now() - wegSeit > 12000) {
      $('koppeln').classList.add('an');
      $('fehler').textContent = koppelGrund();
    }
  } else { wegSeit = 0; }
}, 1000);

// Warum es nicht klappt — in einem Satz, den man auf einem Telefon lesen kann.
//
// Bis 2026-09-30 stand hier eine einzige Meldung, und die war ausgerechnet
// abgeschaltet, sobald ein Token gesetzt war: `if (!link.ready && !state.token)`.
// Also genau im haeufigsten Fall — Token falsch oder veraltet — schwieg die
// Seite und liess den Bediener raten (Durchsicht 2026-09-30).
function koppelGrund() {
  // Der Server trennt mit 1008 und Klartext, wenn dreimal falsch angemeldet
  // wurde. Das ist die eindeutigste Auskunft, die es gibt.
  if (link.schliessCode === 1008) {
    return (link.schliessGrund || 'Abgewiesen')
         + '. Das Token stimmt nicht — neues holen unter Setup → TCI Server.';
  }
  if (state.token && !link.st.angemeldet) {
    return 'Longpath antwortet, nimmt das Token aber nicht an. '
         + 'In Setup → TCI Server ein neues erzeugen und hier eintragen.';
  }
  if (!state.token) {
    return 'Keine Antwort. Ist Longpath ins Netz gebunden? '
         + 'Dann braucht es das Token aus Setup → TCI Server.';
  }
  return 'Keine Antwort. Laeuft Longpath, und ist die Adresse richtig?';
}

// ── Bildschleife ────────────────────────────────────────────────────────────
function schleife(t) {
  zeichneBild();
  const r = link.tickRates(t);
  // Alles zusammen, was die Leitung kostet — auch das Spektrum, das anfangs
  // fehlte und die Anzeige zu günstig aussehen liess.
  const gesamt = r.iq + r.audio + r.spec + r.text;
  $('rate').textContent = gesamt ? (gesamt + ' kB/s') : '';
  $('fussBild').textContent = r.spec ? (r.spec + ' kB/s bild')
                            : r.iq   ? (r.iq + ' kB/s bild (roh)') : '';
  // Der Ton bekommt die Wahrheit, nicht nur eine Byte-Zahl: eine Datenrate
  // ohne hoerbaren Ton hat am 2026-09-30 eine ganze Messreihe wertlos
  // gemacht. Faellt der Ton aus, steht das hier — und nicht nur in einer
  // Konsole, die auf einem Telefon niemand sieht.
  $('fussTon').textContent = state.tonFehler
    ? state.tonFehler
    : (r.audio ? r.audio + ' kB/s ton'
                 + (state.tonWeg === 'scriptprocessor' ? ' (ersatzweg)' : '')
               : '');
  // Stottert der Ton gerade, steht das da — sonst sucht man es im Funkgeraet.
  if (!state.tonFehler && state.tonLeerlaufZuletzt
      && performance.now() - state.tonLeerlaufZuletzt < 2000) {
    $('fussTon').textContent += ' ⚠ stockt';
  }
  $('fussTon').className = state.tonFehler ? 'warn' : '';
  // Eine Verbindung kann formal offen stehen und trotzdem tot sein — WLAN
  // weg, Rechner im Ruhezustand, Longpath beendet. Der Socket merkt das erst
  // nach Minuten. Bis 2026-09-30 zeigte die Seite derweil gruen und
  // '◆ gekoppelt' (Durchsicht). Also an den DATEN messen, nicht am Socket:
  // der Server schickt Messwerte alle 200 ms, drei Sekunden Stille sind
  // eindeutig.
  const still = link.ready && link.letzteDaten
              && (performance.now() - link.letzteDaten > 3000);
  $('fussStatus').textContent = still ? '◆ keine Daten'
                              : link.ready ? '◆ gekoppelt'
                              : (link.ws && link.ws.readyState === 1) ? '◆ verbinde…' : '◇ getrennt';
  $('fussStatus').className = still ? 'warn' : (link.ready ? 'ok' : '');
  requestAnimationFrame(schleife);
}
requestAnimationFrame(schleife);

zeichneKopf();
zeichneBedienung();
