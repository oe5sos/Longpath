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
  spanneHz: 48000,           // gezeigte Bandbreite; 0 = alles
  bildHalten: false,         // Bild einfrieren, solange abgestimmt wird
  wischVersatzPx: 0,         // wie weit der Finger von der Mitte weg ist
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
//
// Die Seite weiss, woher sie geladen wurde — und Longpath laeuft auf
// genau diesem Rechner. Die Adresse muss also niemand eintippen.
//
// Das war bis zum 2026-10-01 anders, und es hat einen Abend gekostet: Auf
// dem Telefon stand noch eine alte IP aus einem anderen Netz, und die
// Seite meldete nur "keine Antwort". Ein Rechnername aus dem Heimnetz
// (`...local`) ueberlebt jeden Netzwechsel; eine IP tut das nicht.
//
// Faellt der Name weg (Seite per file:// geoeffnet), bleibt das Feld leer
// und das Kopplungsblatt fragt wie bisher.
function eigenerHost() {
  const h = location.hostname;
  if (!h || h === 'localhost' || h === '127.0.0.1') { return ''; }
  return h;
}

/** Derselbe Rechner, Longpaths Port.
 *
 *  Es gibt nur diese eine Adresse — auch wenn die Bruecke laeuft: die
 *  belegt dieselbe Portnummer auf der Netzadresse, waehrend Longpath
 *  127.0.0.1 haelt. Darum muss hier nichts probiert und nichts
 *  unterschieden werden. */
const PORT_TCI = 50001;

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
$('adresse').value = gespeichert() || (eigenerHost() ? eigenerHost() + ':' + PORT_TCI : '');
$('token').value = tokenLesen();

/** Verbindet ohne Eingabe mit dem Rechner, von dem die Seite kam. */
function verbindeSelbst() {
  const host = eigenerHost();
  if (!host) { return false; }
  starten(host + ':' + PORT_TCI);
  return true;
}

// Beim Start: eine gemerkte Adresse hat Vorrang — der Operator hat sie
// bewusst gesetzt. ABER sie kann veraltet sein: eine IP aus einem anderen
// Netz steht nach einem Netzwechsel still im Weg, und die Seite meldete
// bisher nur "keine Antwort" (am 2026-10-01 genau so passiert, mit einer
// 192.168er-Adresse auf dem Telefon, waehrend der Mac im 172.30.30er hing).
//
// Darum: gemerkte Adresse zuerst, aber wenn sie binnen fuenf Sekunden
// nicht zu Longpath fuehrt, still auf den eigenen Rechner umschwenken —
// den, von dem diese Seite kam. Der kann gar nicht falsch sein.
if (gespeichert()) {
  starten(gespeichert());
  const gemerkt = gespeichert();
  const eigen = eigenerHost() ? eigenerHost() + ':' + PORT_TCI : '';
  if (eigen && eigen !== gemerkt) {
    let fertig = false;
    const merkeErfolg = () => { fertig = true; };
    link.addEventListener('ready', merkeErfolg, { once: true });
    setTimeout(() => {
      if (fertig) { return; }
      link.removeEventListener('ready', merkeErfolg);
      starten(eigen);
    }, 5000);
  }
} else {
  verbindeSelbst();
}

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
      el.onclick = () => { link.send(`vfo:${state.trx},0,${b.mitte}`);
        wasserfallSchnitt(); letzteMitteHz = b.mitte; };
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
  // Das `device:`-Feld des Init-Bursts ist NICHT das Funkgeraet, sondern
  // Longpaths Tarnkappe: der TCI-Server gibt sich als SunSDR2PRO aus, weil
  // WSJT-X und Hamlib TCI-Audio nur freischalten, wenn der Server sich so
  // meldet (TciEmulateSunSDR2Pro, am Pruefstand 2026-05-11 bestaetigt).
  // Hier ungefiltert angezeigt hat es am 2026-10-01 genau das angerichtet,
  // was es anrichten muss — Betreiber: "ist komischerweise mit sunsdr
  // verbunden … sollte aber mit avelina". Also nur zeigen, was stimmt:
  // die Handfunke spricht mit Longpath. Welches Geraet dort haengt, sagt
  // TCI nicht, und Raten waere schlimmer als Schweigen.
  $('station').textContent = verbunden ? 'LONGPATH' : 'NICHT VERBUNDEN';

  // Wie breit das Bild gerade ist. Ohne diese Angabe weiss man beim Kneifen
  // nicht, wo man gelandet ist — und auch nicht, wie fein die Abstimmung
  // gerade greift (das Raster haengt an der Spanne).
  // ── Der Abstimmstrich gehoert dorthin, wo die Frequenz ist ──────────────
  //
  // Bis 2026-10-01 stand er fest bei 50 % (stil.css, .cursor). Damit zeigte
  // er beim Abstimmen keine Bewegung — der Betreiber: "beim wischen nach
  // rechts und links ändert sich nur die frequenz". Genau so war es: die
  // Zahl lief, das Bild stand, und der Strich blieb stur in der Mitte.
  //
  // Die Bildmitte ist die DDC-Frequenz (dds), nicht die abgestimmte (vfo).
  // Beide fallen nur zusammen, solange nicht innerhalb der DDC-Breite
  // abgestimmt wird. Der Abstand zwischen ihnen, geteilt durch die gezeigte
  // Spanne, ist die Stelle im Bild.
  {
    const c = document.querySelector('.cursor');
    if (c) {
      // Waehrend des Haltens zeigt der Strich, wohin der Finger faehrt.
      if (state.bildHalten) {
        const b = $('scope').getBoundingClientRect().width || 1;
        // Dieselbe Richtung wie die Frequenz oben: der Zeiger folgt dem Finger.
        const anteil = 50 + (state.wischVersatzPx / b) * 100;
        c.style.left = Math.max(0, Math.min(100, anteil)).toFixed(2) + '%';
        c.style.opacity = '1';
        return;
      }
      const mitte = link.st.dds[state.trx];
      const vfo   = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
      const spanne = state.spanneHz > 0 ? state.spanneHz : (link.st.iqRate || 0);
      if (mitte && vfo && spanne > 0) {
        // 0 % ist der linke Rand, 100 % der rechte; die Mitte ist 50 %.
        const anteil = 50 + ((vfo - mitte) / spanne) * 100;
        // Am Rand stehenbleiben statt aus dem Bild zu laufen: ausserhalb
        // waere er unsichtbar, und dann waere nicht zu sehen, dass die
        // Frequenz den Ausschnitt verlassen hat.
        c.style.left = Math.max(0, Math.min(100, anteil)).toFixed(2) + '%';
        c.style.opacity = (anteil < 0 || anteil > 100) ? '0.35' : '1';
      } else {
        c.style.left = '50%';
        c.style.opacity = '1';
      }
    }
  }

  const sp = $('spanne');
  if (sp) {
    const hz = state.spanneHz > 0 ? state.spanneHz : (link.st.iqRate || 0);
    sp.textContent = hz ? (hz >= 1000 ? Math.round(hz / 1000) + ' kHz' : hz + ' Hz') : '';
  }

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

// Die Wasserfallrampe „Gedaempft" aus SpectrumWidget.cpp, mit EINER
// Abweichung: ihr dunkler Teil liegt hier auf dem Panel-Blau (--pan-bg
// #141e27) statt auf Grau.
//
// Warum: am Schreibtisch fuellt der Wasserfall ein halbes Fenster, da traegt
// Grau. Am Telefon ist er eine Handflaeche gross und wird oft im Hellen
// angesehen — dort verschwand alles unterhalb eines starken Traegers in
// einem einzigen gleichfoermigen Grau, obwohl er ein Drittel der Hoehe
// bekommt. Betreiber hat am 2026-10-01 die Richtung „Glas und Tiefe"
// gewaehlt, in der dieser Boden blau ist.
//
// Was NICHT uebernommen wird, obwohl es naheliegt: Longpaths
// WfColorScheme::ClarityBlue. Die ist lesbar, laeuft oben aber ueber Gruen
// und Gelb nach Rot — genau der Regenbogen, den „Gedaempft" 2026-08-15
// bewusst abgeschafft hat, und Rot ist hier der Warnung vorbehalten.
// Geaendert ist also nur die Grundfarbe, nicht der Aufbau: Boden
// verschwindet, Waerme erst oben, Weiss ganz oben.
//
// Die Lage der Stuetzpunkte ist am laufenden Bild nachgezogen: mit dem
// Original lagen die Traeger der Attrappe im graublauen Teil und blieben
// kuehl, obwohl sie das Lauteste im Bild waren. Der warme Ton beginnt
// jetzt bei 0,60 statt 0,78, der Boden bleibt unveraendert dunkel.
const STOPS = [[0,[11,14,19]],[.30,[20,30,39]],[.46,[33,52,74]],[.60,[124,108,78]],
               [.78,[188,144,86]],[.92,[216,165,95]],[1,[242,242,236]]];
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

// ── Den Wasserfall leeren (2026-10-01) ──────────────────────────────────────
//
// Der Wasserfall ist eine Historie: jede Zeile ist ein Augenblick von
// frueher. Nach einem Frequenzsprung zeigt diese Historie ein Band, auf dem
// man gar nicht mehr steht — sie ist nicht veraltet, sondern FALSCH, und sie
// braucht eine halbe Minute, bis sie nach oben herausgewandert ist.
//
// Betreiber am 2026-10-01: "der wasserfall unten springt nicht sofort auf
// die frequenz". Darauf hin wurde beim Sprung GELOESCHT — und am selben
// Abend kam die Gegenbeschwerde: "jede frequenzaenderung loescht unten den
// panadapter und er laedt wieder neu … man sollte immer etwas sehen".
//
// Beides stimmt, und beides zusammen ergibt die richtige Loesung: Die
// Historie ist nicht wertlos, sie steht nur an der falschen Stelle. Ein
// Pult schiebt sie darum seitlich mit, statt sie wegzuwerfen — ein Traeger
// bleibt dabei unter sich selbst stehen, und man sieht ununterbrochen
// etwas. Nur wo die Zeilen wirklich nichts mehr bedeuten (anderes Band,
// andere Bandbreite), wird eine Trennlinie gezogen statt das Bild
// geleert: oberhalb das Neue, unterhalb das Alte, und der Schnitt
// wandert in ein paar Sekunden von selbst hinaus.

function wasserfallLeeren() {
  try {
    wfCtx.fillStyle = '#0c0c0e';
    wfCtx.fillRect(0, 0, wf.width, wf.height);
  } catch (e) { /* vor dem ersten Zeichnen */ }
  neueZeilen = 0;
}

/** Hertz je Bildpunkt des Wasserfalls — eine Stelle, damit Schieben und
 *  Skalieren nicht auseinanderlaufen. */
function hzProPunkt() {
  const spanne = state.spanneHz > 0 ? state.spanneHz : (link.st.iqRate || 192000);
  return spanne / wf.width;
}

/** Schiebt die Historie um die Frequenzaenderung zur Seite.
 *
 *  Steigt die Mittenfrequenz um dHz, wandert ein fester Sender im Bild nach
 *  LINKS — darum das Minus. Der frei werdende Rand wird dunkel gefuellt:
 *  dort ist nichts bekannt, und eine Wiederholung waere gelogen.
 *
 *  Weiter als die Bildbreite hat das Schieben keinen Sinn, dann ueberlappt
 *  nichts mehr; in dem Fall ein Schnitt. */
function wasserfallSchieben(dHz) {
  if (!dHz) { return; }
  const dx = Math.round(-dHz / hzProPunkt());
  if (dx === 0) { return; }
  if (Math.abs(dx) >= wf.width) { wasserfallSchnitt(); return; }
  try {
    // Ueber den Zwischenpuffer, wie beim Zeilenschub: ein Canvas, der sich
    // selbst als Quelle zeichnet, schiebt nicht verlaesslich.
    wfPuffCtx.clearRect(0, 0, wf.width, wf.height);
    wfPuffCtx.drawImage(wf, 0, 0);
    wfCtx.fillStyle = '#0c0c0e';
    wfCtx.fillRect(0, 0, wf.width, wf.height);
    wfCtx.drawImage(wfPuff, dx, 0);
  } catch (e) { /* vor dem ersten Zeichnen */ }
}

/** Haelt den Wasserfall unter der Frequenz fest, egal wer sie geaendert hat.
 *
 *  Vier Stellen schoben die Historie frueher selbst — Tipp, Wisch, Band,
 *  Zoom. Das deckte aber nur die eigenen Bedienungen ab: dreht der Operator
 *  am Pult oder an einem anderen Client, kommt die neue Frequenz ueber den
 *  Draht herein, und davon erfuhr der Wasserfall nichts. Darum hier EINE
 *  Stelle, die im Zeichentakt die Mitte vergleicht; sie faengt jede
 *  Aenderung, ganz gleich woher sie kam. */
let letzteMitteHz = null;
function mitteVerfolgen() {
  const v = link.st.vfo[state.trx];
  const jetzt = v ? v[0] : null;
  if (jetzt === null) { return; }
  if (letzteMitteHz === null) { letzteMitteHz = jetzt; return; }
  if (jetzt === letzteMitteHz) { return; }
  wasserfallSchieben(jetzt - letzteMitteHz);
  letzteMitteHz = jetzt;
}

/** Zieht eine Trennlinie statt zu loeschen: oben das Neue, unten das Alte.
 *
 *  Fuer Bandwechsel und Zoom — dort deckt dieselbe Zeile danach etwas
 *  voellig anderes ab, und Schieben hilft nicht. Die Linie sagt, wo der
 *  Schnitt liegt, und das Bild bleibt sichtbar, bis das Alte unten
 *  herausgewandert ist. */
function wasserfallSchnitt() {
  try {
    wfCtx.fillStyle = 'rgba(216,165,95,.55)';   // Bernstein, wie alles Gemessene
    wfCtx.fillRect(0, 0, wf.width, 2);
  } catch (e) { /* vor dem ersten Zeichnen */ }
}
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
// ── Den Stummschalter aushebeln (2026-10-01) ────────────────────────────────
//
// iOS stuft Ton aus einer Webseite als "Ambient" ein, und diese Kategorie
// gehorcht dem kleinen Schieber an der Seite des Telefons: steht er auf
// stumm, bleibt der eingebaute Lautsprecher still — WEB-AUDIO EINGESCHLOSSEN.
// Kopfhoerer sind davon nicht betroffen, iOS gibt dort trotzdem aus.
//
// Genau dieses Bild am 2026-10-01: alle Zahlen richtig (Rahmen kamen an, der
// Puffer war voll, der Tonknoten lief, der Regler stand auf 1,0), nichts zu
// hoeren — aber "mit ohrstöpsel geht es". Von aussen sieht das aus wie ein
// Fehler im Programm und ist keiner.
//
// Spielt die Seite ein <audio>-Element ab, wechselt iOS in die Kategorie
// "Playback", und die ignoriert den Schieber. Das Element traegt eine
// Sekunde Stille und laeuft in Schleife; es ist nicht zu hoeren und kostet
// nichts.
//
// Der Aufruf muss aus einer Beruehrung kommen — deshalb steht er in
// tonStarten() und nicht irgendwo beim Laden.
function stummesElementStarten() {
  const el = $('stillhalter');
  if (!el) { return; }
  try {
    el.volume = 0;          // hoerbar ist daran nichts
    const p = el.play();
    if (p && p.catch) { p.catch(() => { /* ohne geht es eben nur mit Hoerer */ }); }
  } catch (e) { /* desgleichen */ }
}

async function tonStarten() {
  // Riegel gegen Doppelstart. `state.node` allein reicht NICHT: zwischen der
  // Pruefung und dem Setzen liegen zwei await-Stellen, und ein einziger Tipp
  // loest ueber 'touchend' UND 'click' zwei Laeufe aus. Ergebnis waren
  // zuverlaessig zwei AudioContexts, von denen der erste nie wieder
  // geschlossen wurde (Durchsicht 2026-09-30).
  if (state.node || state.tonStartLaeuft) { return; }
  state.tonStartLaeuft = true;
  try {
    // ZUERST: iOS in die Playback-Kategorie bringen, bevor der AudioContext
    // entsteht. Danach gehorcht der Ton dem Stummschalter nicht mehr.
    stummesElementStarten();

    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) { throw new Error('Dieser Browser kennt keinen AudioContext'); }
    // KEINE Abtastrate vorgeben. Die Rate ist eine Eigenschaft des
    // Ausgabegeraets, nicht unsere Wahl: iOS liefert je nach Hoerer 48000,
    // 44100 oder bei Bluetooth auch 16000, und eine abweichende Vorgabe
    // wird dort teils ignoriert, teils schlaegt die Erzeugung fehl. Wir
    // brauchen sie ohnehin nicht — TonKern rechnet auf ctx.sampleRate um,
    // was immer das ist.
    const ctx = new AC({ latencyHint: 'interactive' });

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
      //
      // EIN Eingangskanal, nicht null. Ein ScriptProcessorNode ohne Eingang
      // wird nicht überall getaktet: manche Umsetzungen rufen
      // onaudioprocess nur, wenn etwas hineinfliesst, und Safari gehoert
      // dazu. Der Knoten haengt dann stumm im Graphen, und niemand sieht,
      // warum. Deshalb bekommt er unten eine stille Quelle vorgeschaltet —
      // sie liefert Nullen und dient nur dem Takt.
      const sp = ctx.createScriptProcessor(2048, 1, 2);
      let leerZaehler = 0;
      sp.onaudioprocess = (ev) => {
        state.tonTakte = (state.tonTakte || 0) + 1;
        const out = ev.outputBuffer;
        const voll = kern.zieh(out.getChannelData(0), out.getChannelData(1), out.length);
        // Denselben Leerlauf melden wie das Worklet, damit die Fusszeile auf
        // beiden Wegen dasselbe sagt.
        if (!voll && !kern.muted && (++leerZaehler & 31) === 0) {
          state.tonLeerlauf = (state.tonLeerlauf || 0) + 32;
          state.tonLeerlaufZuletzt = performance.now();
        }
      };
      // Die stille Quelle, die den Takt garantiert (siehe oben). Ein
      // ConstantSourceNode mit offset 0 liefert Nullen, kostet nichts und
      // haelt den ScriptProcessor am Laufen.
      let takt = null;
      try {
        takt = ctx.createConstantSource();
        takt.offset.value = 0;
        takt.connect(sp);
        takt.start();
      } catch (e) {
        // Kennt der Browser ConstantSourceNode nicht, tut es auch ein
        // leerer Puffer in Dauerschleife.
        try {
          const leer = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
          const q = ctx.createBufferSource();
          q.buffer = leer; q.loop = true; q.connect(sp); q.start();
          takt = q;
        } catch (e2) { /* dann eben ohne — auf Chrome laeuft es auch so */ }
      }

      // Eine Huelle, die sich nach aussen wie der Worklet-Knoten verhaelt —
      // so kennt der Rest der Seite nur EINEN Weg.
      node = {
        _sp: sp, _kern: kern,
        connect: (z) => sp.connect(z),
        disconnect: () => { try { if (takt) takt.disconnect(); } catch (e) {} sp.disconnect(); },
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
    // Schwelle dicht unter Vollaussteuerung, nicht bei -3 dBFS.
    //
    // Bei -3 griff er auch in der Stellung 70 %, die ausdruecklich als
    // "klingt wie bisher" zugesagt ist: die AGC des Empfaengers liefert in
    // der Regelung Spitzen um 0,98 (-0,2 dBFS), und die liegen ueber -3.
    // Der Deckel zog damit jede Sprachspitze 20:1 zusammen — aus einem
    // Schutz gegen Klirren war unversehens ein Klangeingriff geworden.
    // Gefunden bei der Durchsicht der eigenen Reparatur (2026-09-30).
    //
    // Bei -1 dBFS bleibt die Eins-zu-eins-Stellung unangetastet (dort kann
    // nichts ueber 1,0 kommen, die Quelle ist ja begrenzt) und der Deckel
    // tut genau das, wofuer er da ist: er faengt ab, was der
    // Verstaerkungsvorrat darueber hinaustreibt.
    deckel.threshold.value = -1;
    deckel.knee.value = 0;
    deckel.ratio.value = 20;
    deckel.attack.value = 0.003;
    deckel.release.value = 0.25;

    node.connect(gain);
    gain.connect(deckel);
    deckel.connect(ctx.destination);

    // Zwei Horchposten, damit sich nachweisen laesst, WO ein Ton verschwindet:
    // einer am Regler (vor dem Begrenzer), einer am Ausgang. Die Seite misst
    // sich damit selbst und meldet es — auf einem Telefon ist das der einzige
    // Weg, an diese Zahlen zu kommen.
    try {
      state.mess1 = ctx.createAnalyser(); state.mess1.fftSize = 2048;
      state.mess1.smoothingTimeConstant = 0;
      gain.connect(state.mess1);
      state.mess2 = ctx.createAnalyser(); state.mess2.fftSize = 2048;
      state.mess2.smoothingTimeConstant = 0;
      deckel.connect(state.mess2);
    } catch (e) { /* ohne Messpunkte laeuft es trotzdem */ }
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
// Der sichtbare Weg: ein Knopf, der sagt, was er tut. Die Beruehrung
// irgendwo auf der Seite bleibt zusaetzlich — sie schadet nicht und hilft
// dem, der ohnehin tippt.
$('tonAn').addEventListener('click', async () => {
  await tonStarten();
  zeichneTonKnopf();
  // Nach dem Tippen melden: das ist der Augenblick, in dem sich
  // entscheidet, ob die Tonkette steht. Ohne diese Zeile muss man raten,
  // ob der Knopf gedrueckt wurde und was er bewirkt hat.
  setTimeout(() => melde('knopf'), 1500);
});
['touchend', 'click'].forEach(ev =>
  document.addEventListener(ev, async () => {
    await tonStarten(); zeichneTonKnopf();
  }, { passive: true }));

// Testton: ein Sinus durch DIESELBE Kette (Regler, Begrenzer, Ausgang).
// Hoert man ihn nicht, liegt es an der Kette oder am Geraet — hoert man ihn,
// liegt es an den Daten. Das trennt die beiden Faelle in einem Griff.
// Erreichbar ueber langes Tippen auf die Datenrate oben rechts.
async function testton() {
  await tonStarten();
  const C = state.audio;
  if (!C) { return; }
  try {
    const o = C.createOscillator();
    o.frequency.value = 700;
    const g = C.createGain();
    g.gain.value = 0.25;
    o.connect(g);
    // Bewusst am Regler VORBEI direkt an den Ausgang: so prueft der Ton die
    // Ausgabe des Geraets, nicht unsere Lautstaerkerechnung.
    g.connect(C.destination);
    o.start();
    setTimeout(() => { try { o.stop(); o.disconnect(); g.disconnect(); } catch (e) {} }, 2000);
    melde('testton');
  } catch (e) { /* nichts */ }
}
(() => {
  const r = $('rate');
  if (!r) { return; }
  let halt = null;
  const an = () => { halt = setTimeout(testton, 700); };
  const aus = () => { clearTimeout(halt); };
  ['touchstart', 'mousedown'].forEach(e => r.addEventListener(e, an, { passive: true }));
  ['touchend', 'touchcancel', 'mouseup', 'mouseleave'].forEach(e =>
    r.addEventListener(e, aus, { passive: true }));
})();

// Der Knopf steht da, solange kein Ton laeuft, und verschwindet danach.
function zeichneTonKnopf() {
  const k = $('tonAn');
  if (!k) { return; }
  const laeuft = !!state.node && state.audio && state.audio.state === 'running';
  k.hidden = laeuft;
  if (!laeuft) { k.textContent = state.tonFehler ? 'TON?' : 'TON EIN'; }
}
setInterval(zeichneTonKnopf, 1000);
zeichneTonKnopf();

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
    // Das Bild festhalten, solange der Finger unten ist.
    //
    // Longpath fuehrt die Bildmitte der Abstimmung nach
    // (RadioModel.cpp: setCenterFrequency(slice->frequency())). Das Bild
    // wandert also MIT, die Mitte bleibt die Mitte — und sichtbar aendert
    // sich nur die Zahl. Betreiber am 2026-10-01: "beim wischen nach rechts
    // und links ändert sich nur die frequenz".
    //
    // Was man beim Abstimmen sehen will, ist das Gegenteil: ein stehendes
    // Bild, durch das der Abstimmstrich faehrt, damit man sieht, WOHIN man
    // faehrt. Solange der Finger unten ist, wird deshalb kein neues Bild
    // angenommen und der Strich wandert stattdessen. Beim Loslassen laeuft
    // es normal weiter.
    state.bildHalten = true;
  }
  // Mit der GEZEIGTEN Spanne rechnen, nicht mit der vollen DDC-Breite.
  // Vorher stand hier immer iqRate (192 kHz): bei 373 Punkten waren das
  // 515 Hz je Bildpunkt, und die Frequenz sprang beim Abstimmen in
  // Halbkilohertz-Schritten. Betreiber am 2026-10-01: "frequenz kann man
  // zwar ändern, aber sehr schlecht".
  const spanne = state.spanneHz > 0 ? state.spanneHz : (link.st.iqRate || 192000);
  const proPixel = spanne / $('scope').getBoundingClientRect().width;
  // Das Raster folgt der Spanne: wer eng zoomt, will auch fein abstimmen.
  // 10 Hz bei schmaler Sicht, 100 Hz bei breiter — sonst zappelt die
  // Anzeige, ohne dass man das Signal trifft.
  const raster = spanne <= 24000 ? 10 : (spanne <= 96000 ? 50 : 100);
  // PLUS, nicht minus: der Finger zieht den ZEIGER, nicht das Band.
  //
  // Solange das Bild mitwanderte, galt die Karten-Konvention — Finger nach
  // rechts schob das Band nach rechts, man fuhr also abwaerts. Seit das Bild
  // beim Abstimmen steht (bildHalten), zieht man den Zeiger durch ein
  // stehendes Spektrum, und dann muss er dem Finger folgen: rechts ist
  // rechts, und rechts sind die hoeheren Frequenzen.
  //
  // Betreiber am 2026-10-01: "zeiger geht genau seitenverkehrt". Ich hatte
  // beim Umbau die alte Richtung stehen lassen.
  const neu = Math.round((wischHz + weg * proPixel) / raster) * raster;
  wischZiel = neu;
  // Wie weit der Strich vom Bildmittelpunkt weg ist, in Bildpunkten.
  state.wischVersatzPx = weg;
  const jetzt = performance.now();
  if (jetzt - wischLetzt < kWischAbstandMs) return;
  wischLetzt = jetzt;
  link.send(`vfo:${state.trx},0,${neu}`);
});
// ── Kneifen zum Vergroessern (2026-10-01) ───────────────────────────────────
//
// Betreiber: "vergrössern kann ich leider nicht". Es gab schlicht keinen Weg.
//
// Die Spanne wird am SERVER beschnitten (spectrum_start, vierter Wert), nicht
// hier — nur so steigt die Aufloesung wirklich. Schnitte der Browser zu,
// malte er dieselben groben Punkte nur breiter.
const kSpannen = [6000, 12000, 24000, 48000, 96000, 192000];
let kneifVon = 0;
const zeiger = new Map();

function spanneSetzen(hz) {
  const neu = kSpannen.reduce((a, b) =>
    Math.abs(b - hz) < Math.abs(a - hz) ? b : a);
  if (neu === state.spanneHz) { return; }
  state.spanneHz = neu;
  link.send(`spectrum_start:${state.trx},${pan.width},12,${neu}`);
  // Nach einem Zoom decken dieselben Zeilen eine andere Bandbreite ab.
  // Schieben hilft da nicht — ein Schnitt sagt, ab wo der neue Massstab
  // gilt, und laesst das Alte sichtbar hinauswandern.
  wasserfallSchnitt();
  zeichneKopf();
}

$('scope').addEventListener('pointerdown', (e) => {
  zeiger.set(e.pointerId, e.clientX);
  if (zeiger.size === 2) {
    const [a, b] = [...zeiger.values()];
    kneifVon = Math.abs(a - b);
    wischVon = null;              // kein Abstimmen waehrend des Kneifens
    wischAktiv = false;
  }
}, { passive: true });

$('scope').addEventListener('pointermove', (e) => {
  if (!zeiger.has(e.pointerId)) { return; }
  zeiger.set(e.pointerId, e.clientX);
  if (zeiger.size !== 2 || !kneifVon) { return; }
  const [a, b] = [...zeiger.values()];
  const jetzt = Math.abs(a - b);
  if (jetzt < 20) { return; }
  // Auseinander = naeher heran = kleinere Spanne.
  spanneSetzen(state.spanneHz * (kneifVon / jetzt));
  kneifVon = jetzt;
}, { passive: true });

['pointerup', 'pointercancel', 'pointerleave'].forEach(ev =>
  $('scope').addEventListener(ev, (e) => {
    zeiger.delete(e.pointerId);
    if (zeiger.size < 2) { kneifVon = 0; }
  }, { passive: true }));

// ── Tippen springt dorthin (2026-10-01) ─────────────────────────────────────
//
// Betreiber: "wenn ich ein signal sehe, vor allem im unteren pandapter
// möchte ich auch dort hinklicken, das funktioniert aber nicht".
//
// Es ging nicht, weil die Totzone (gegen ungewolltes Verstimmen beim
// Anfassen) jeden Tipp verschluckte, ohne ihn je als Befehl zu werten. Das
// war eine halbe Loesung: nicht verstimmen ist richtig, nichts tun ist es
// nicht.
//
// Jetzt: ein kurzer Tipp OHNE Bewegung springt auf die getippte Stelle, ein
// Wisch zieht wie bisher. Beide Canvas liegen im selben Element, also gilt
// das fuer Spektrum und Wasserfall gleichermassen — und gerade im
// Wasserfall sieht man eine Station oft zuerst.
function frequenzAnStelle(clientX) {
  const r = $('scope').getBoundingClientRect();
  if (!r.width) { return null; }
  const spanne = state.spanneHz > 0 ? state.spanneHz : (link.st.iqRate || 192000);
  const mitte  = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
  if (!mitte) { return null; }
  // Die Bildmitte ist die abgestimmte Frequenz (Longpath fuehrt sie nach).
  const versatz = (clientX - r.left) / r.width - 0.5;
  const raster = spanne <= 24000 ? 10 : (spanne <= 96000 ? 50 : 100);
  return Math.round((mitte + versatz * spanne) / raster) * raster;
}

$('scope').addEventListener('pointerup', (e) => {
  // Kein Wisch gewesen und nur ein Finger im Spiel? Dann war es ein Tipp.
  if (!wischAktiv && zeiger.size <= 1 && wischVon !== null) {
    const ziel = frequenzAnStelle(e.clientX);
    if (ziel) {
      const vorher = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : ziel;
      link.send(`vfo:${state.trx},0,${ziel}`);
      // Nicht hier schieben: mitteVerfolgen() tut es, sobald das Geraet die
      // neue Frequenz bestaetigt. Zweimal schieben hiesse doppelt schieben.
    }
  }
  // Den letzten Stand nachreichen: durch die Drosselung kann bis zu ein
  // Intervall Wegstrecke ungesendet geblieben sein, und dann steht das
  // Geraet ein Stueck neben dem, wo der Finger losgelassen hat.
  if (wischAktiv && wischZiel !== null) {
    link.send(`vfo:${state.trx},0,${wischZiel}`);
    // Auch hier nicht selbst schieben — siehe mitteVerfolgen().
  }
  wischVon = null; wischAktiv = false; wischZiel = null;
  state.bildHalten = false;
  state.wischVersatzPx = 0;
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
  link.send(`spectrum_start:${state.trx},${pan.width},12,${state.spanneHz}`);

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
  // Waehrend des Abstimmens kein neues Bild annehmen — siehe
  // state.bildHalten. Der Zaehler laeuft weiter, damit die Datenrate
  // stimmt; nur das Bild friert ein.
  if (state.bildHalten) { state.hatSpektrumstrom = true; return; }
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

// ── Selbstmeldung (2026-09-30) ──────────────────────────────────────────────
//
// Ein Telefon hat keine Konsole, die jemand lesen koennte, und Vorlesen
// lassen ist muehsam und fehleranfaellig. Also meldet die Seite einmal
// nach dem Start, wie es um ihre Tonkette steht — reine Zustandszahlen,
// kein Inhalt, und nur an den Rechner, von dem sie geladen wurde.
async function melde(anlass) {
  const C = state.audio, K = state.node && state.node._kern;
  const d = {
    sicher: window.isSecureContext,
    weg: state.tonWeg || 'keiner',
    fehler: state.tonFehler || '-',
    ctx: C ? C.state : 'kein ctx',
    ctxRate: C ? C.sampleRate : 0,
    takte: state.tonTakte || 0,          // wurde onaudioprocess je gerufen?
    af: state.gain ? +state.gain.gain.value.toFixed(2) : -1,
    afPct: state.afPct,
    rahmen: link.bytes.audio,
    tonTyp: link.st.audioTypRahmen,
    rate: link.st.audioRate,
    vorrat: K ? K.have : -1,
    ziel: K ? K.target : -1,
    anlauf: K ? K.anlauf : '-',
    leer: K ? K.starved : -1,
    ready: link.ready,
  };
  // Pegel an beiden Horchposten — in dBFS, gerundet.
  const pegel = (an) => {
    if (!an) { return 'kein'; }
    const z = new Float32Array(an.fftSize);
    an.getFloatTimeDomainData(z);
    let s = 0; for (let i = 0; i < z.length; i++) { s += z[i] * z[i]; }
    const rms = Math.sqrt(s / z.length);
    return rms > 0 ? Math.round(20 * Math.log10(rms)) : 'still';
  };
  d.vorDeckel  = pegel(state.mess1);
  d.nachDeckel = pegel(state.mess2);
  d.ausgabe    = (C && C.destination) ? C.destination.channelCount : -1;
  d.anlass = anlass || 'start';
  const q = Object.entries(d).map(([k, v]) => k + '=' + encodeURIComponent(String(v))).join('&');
  try { await fetch('/melde?' + q); } catch (e) {}
}
setTimeout(() => melde('start'), 12000);

// ── Bildschleife ────────────────────────────────────────────────────────────
function schleife(t) {
  mitteVerfolgen();
  zeichneBild();
  const r = link.tickRates(t);
  // Alles zusammen, was die Leitung kostet — auch das Spektrum, das anfangs
  // fehlte und die Anzeige zu günstig aussehen liess.
  const gesamt = r.iq + r.audio + r.spec + r.text;
  $('rate').textContent = gesamt ? (gesamt + ' kB/s') : '';
  // Kommt kein Bild, steht hier WARUM — nicht nur eine fehlende Zahl.
  //
  // Am 2026-10-01 blieb der Wasserfall schwarz, waehrend Ton und
  // Signalpegel liefen, und nichts auf der Seite sagte, woran es lag
  // (Ursache war: die installierte Longpath-Fassung kannte den Befehl
  // `spectrum_start` gar nicht). Dieselbe Stille entsteht, wenn in
  // Longpath schlicht kein Funkgeraet verbunden ist. Ein leerer Kasten
  // laesst den Operator raten; ein Satz nicht.
  const bildLaeuft = r.spec > 0 || r.iq > 0;
  $('fussBild').className = bildLaeuft ? '' : 'warn';
  $('fussBild').textContent =
      r.spec ? (r.spec + ' kB/s bild')
    : r.iq   ? (r.iq + ' kB/s bild (roh)')
    : link.ready ? 'kein bild — funkgerät verbunden?'
    : '';
  // Der Ton bekommt die Wahrheit, nicht nur eine Byte-Zahl: eine Datenrate
  // ohne hoerbaren Ton hat am 2026-09-30 eine ganze Messreihe wertlos
  // gemacht. Faellt der Ton aus, steht das hier — und nicht nur in einer
  // Konsole, die auf einem Telefon niemand sieht.
  $('fussTon').textContent = state.tonFehler
    ? state.tonFehler
    : !state.node ? 'Ton aus — auf TON EIN tippen'
    : (r.audio ? r.audio + ' kB/s ton'
                 + (state.tonWeg === 'scriptprocessor' ? ' (ersatzweg)' : '')
               : '');
  // Stottert der Ton gerade, steht das da — sonst sucht man es im Funkgeraet.
  //
  // Nur bei STEHENDER Verbindung: ohne Gegenstelle laeuft die Tonkette
  // selbstverstaendlich leer, und "stockt" waere dann die falsche Erklaerung
  // fuer etwas, das die Fusszeile daneben schon richtig benennt
  // ("getrennt"). Genau so am 2026-09-30 gesehen.
  // Nur melden, wenn ueberhaupt ein Tonstrom laeuft. Ohne Strom laeuft die
  // Kette selbstverstaendlich leer — "stockt" waere dann die falsche
  // Erklaerung fuer "es kommt nichts", und sie stuende dauerhaft da. Die
  // Pruefung auf link.ready allein genuegte nicht: die Verbindung kann
  // stehen, ohne dass Ton abonniert ist.
  if (link.ready && r.audio > 0 && !state.tonFehler && state.tonLeerlaufZuletzt
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
