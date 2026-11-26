// Desktop-Layout der App (ab ca. 900 px Breite), 2026-11-21.
//
// Martins Blatt: docs/design/handfunke-entwuerfe/2026-11-21-desktop. Unter
// 900 px bleibt „Ein Schirm" unveraendert. Hier wird NICHTS neu gebaut, nur
// umgesetzt: dieselben Elemente (mit denselben Kennungen und Ereignishoerern)
// wandern in ein Raster und kehren bei schmaler Breite an ihren Platz zurueck.
// Neu sind nur die Darstellungen: SWR-Zeiger, Log-Spiegel.
// Datenstrom, Station, Server: unveraendert.

const BREITE = '(min-width: 900px)';

export function desktopStarten({ link, $, schrittHz, logHolen }) {
  const mq = window.matchMedia(BREITE);
  const platz = new Map();           // Element -> Platzhalter (Rueckweg)
  let gitter = null, logZeilen = null, fbText = null;

  const mk = (html) => {
    const d = document.createElement('div');
    d.innerHTML = html.trim();
    return d.firstElementChild;
  };
  const hin = (el, ziel) => {
    if (!el) { return; }
    if (!platz.has(el)) {
      const ph = document.createComment('dt');
      el.parentNode.insertBefore(ph, el);
      platz.set(el, ph);
    }
    ziel.append(el);
  };
  const gruppe = (titel, ...kinder) => {
    const g = mk('<div class="dtGruppe"></div>');
    // Ueber hin(), damit die Pillen beim Zurueck auf „Ein Schirm" wieder an
    // ihren Platz kommen (vorher blieben Band/Betriebsart/Filter/NR weg).
    kinder.forEach((k) => hin(k, g));
    g.setAttribute('aria-label', titel);
    return g;
  };

  function bauen() {
    gitter = mk('<div id="dtGitter"></div>');
    const bar = mk('<div id="dtBar"></div>');
    const links = mk('<div id="dtLinks"></div>');
    const mitte = mk('<div id="dtMitte"></div>');
    const rechts = mk('<div id="dtRechts"></div>');
    gitter.append(bar, links, mitte, rechts);

    bar.append(gruppe('Band', $('bands')), gruppe('Betriebsart', $('modes')),
               gruppe('Filter', $('filters')), gruppe('Schritt', $('schritte')),
               gruppe('NR', $('dsp')));

    // Links: die Instrumentenzeile (S-Meter | SWR | Kompass), darunter das Log.
    const log = mk('<div class="schicht dtPnl dtLogPnl"><div class="dtLogKopf"><span class="caps">Log</span></div><div id="dtLogZeilen"></div></div>');
    logZeilen = log.querySelector('#dtLogZeilen');
    hin($('instrumente'), links);
    links.append(log);
    // FT8-Liste unter dem Log, am Desktop immer aufgeklappt (stil.css).
    hin($('ft8Feld'), links);
    hin($('logAuf'), log.querySelector('.dtLogKopf'));

    hin(document.querySelector('.scope'), mitte);

    // Rechts: Kopf, Combined (Frequenz, Filterbreite, Schritt), Regler, Rotor.
    const comb = mk('<div class="schicht dtPnl"><div class="caps">Combined</div>' +
      '<div id="dtComb"></div>' +
      '<div class="dtZeile"><span class="caps">Filterbreite</span><span id="dtFb" class="dtW">—</span></div>' +
      '<div class="dtZeile"><span class="caps">Schritt</span><span id="dtSchr" class="dtW">100 Hz</span></div>' +
      '<div class="dtZeile dtAgc"><span class="caps">AGC</span><span id="dtAgcW"></span></div>' +
      '<div id="dtVersatz"></div></div>');
    fbText = comb.querySelector('#dtFb');
    hin(document.querySelector('header'), rechts);
    rechts.append(comb);
    hin(document.querySelector('.vfo'), comb.querySelector('#dtComb'));
    // AGC: die Pillen und AGC-T neben die Ueberschrift, wie die Werte darueber.
    hin($('agc'), comb.querySelector('#dtAgcW'));
    hin($('agcWert'), comb.querySelector('#dtAgcW'));
    // RIT/XIT: dieselben Zeilen wie am Telefon, hier in „Combined" wie am Mac.
    hin($('ritZeile'), comb.querySelector('#dtVersatz'));
    hin($('xitZeile'), comb.querySelector('#dtVersatz'));
    hin(document.querySelector('.sliders'), rechts);
    hin($('rotorFeld'), rechts);
    hin(document.querySelector('.mikrow'), rechts);

    document.body.prepend(gitter);
    // Sendeleiste und Fusszeile bleiben, wo sie sind (fest unten).
  }

  function aufraeumen() {
    for (const [el, ph] of platz) {
      if (ph.parentNode) { ph.parentNode.replaceChild(el, ph); }
    }
    platz.clear();
    if (gitter) { gitter.remove(); gitter = null; }
    document.body.classList.remove('dt');
  }

  function umschalten() {
    if (mq.matches && !gitter) {
      document.body.classList.add('dt');
      bauen();
      logHolen();
    } else if (!mq.matches && gitter) {
      aufraeumen();
    }
  }
  mq.addEventListener('change', umschalten);

  // Werte, die das Raster zusaetzlich zeigt. Gelesen, nie geschrieben.
  setInterval(() => {
    if (!gitter) { return; }
    const f = document.querySelector('#filters .chip.on');
    // Nach dem Ziehen der Kanten passt meist keine Filterpille mehr; dann die
    // gemeldete Breite selbst.
    const fl = (link.st.filter && link.st.filter[0]) || [];
    const breite = Math.abs((fl[1] || 0) - (fl[0] || 0));
    const fbNeu = f ? f.textContent.trim()
      : breite ? (breite >= 1000 ? (breite / 1000).toFixed(2).replace(/0$/, '') + 'k' : breite + ' Hz') : '—';
    if (fbText && fbText.textContent !== fbNeu) { fbText.textContent = fbNeu; }
    const s = document.getElementById('dtSchr');
    if (s) { const hz = schrittHz(); s.textContent = hz === 1000 ? '1 kHz' : hz + ' Hz'; }
    // Log spiegeln: die Zeilen der Logliste, ohne eigene Abfrage-Logik.
    const quelle = document.getElementById('logListe');
    if (quelle && logZeilen) {
      const zeilen = [...quelle.children].slice(0, 8).map((k) => (k.children.length ? [...k.children].map((c) => c.textContent.trim()).filter(Boolean).join('  ') : k.textContent.trim())).filter(Boolean);
      const neu = zeilen.length ? zeilen.map((t) => '<div class="dtLogZ">' + t.replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c])) + '</div>').join('') : '<div class="dtKlein">noch keine Einträge</div>';
      if (logZeilen.innerHTML !== neu) { logZeilen.innerHTML = neu; }
    }
  }, 500);

  link.addEventListener('ready', () => { if (gitter) { logHolen(); } });
  umschalten();
}
