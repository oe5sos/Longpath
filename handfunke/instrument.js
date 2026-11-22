// Die Zeigerinstrumente von Longpath, fuer die App.
//
// Kein eigener Entwurf: Geometrie, Teilung, Verlaeufe und Farben stammen aus
// Martins Vorlage ~/Downloads/zeiger-verfeinert.html (Stand 2026-08-17), nach
// der auch NeedleInstrument/InstrumentPainter am Mac gebaut sind
// (src/gui/instruments/): Bogen von 168 Grad nach 12 Grad, Rille mit Schatten,
// auslaufender Sektor bis zum Zeiger, Glut am Drehpunkt, Zeiger mit heller
// Kante, rote Schwelle als harte Kante. Skalen aus ReadingSource.cpp:
// S-Meter 0..9 gleichmaessig und darueber gestaucht (S9 bei 0,66), SWR 1..3
// mit Schwelle 2,5. Der Rotor-Kompass folgt RotatorItem.cpp (Punkte alle
// 10 Grad, grosse Punkte alle 45 Grad, Himmelsrichtungen aussen).
//
// Neu ist nur die Fassung fuer kleine Flaechen: weniger, groessere Beschriftung.

const NS = 'http://www.w3.org/2000/svg';
const C = { face: '#5e5e64', arc: '#2c2c31', meas: '#d8a55f', dim: '#6b5630',
            danger: '#a86b6d', inset: '#08080a' };
const el = (t, a) => { const e = document.createElementNS(NS, t);
  for (const k in a) { e.setAttribute(k, a[k]); } return e; };

// Mass der Vorlage
const CX = 260, CY = 168, R = 148, W = 13, D0 = 168, D1 = 12;
export const ZEIGER_VIEWBOX = '96 8 328 172';

/** dBm -> S-Einheiten (S9 = -73 dBm, 6 dB je Stufe, darueber 10 dB). */
export const sEinheiten = (dbm) => (dbm <= -73 ? (dbm + 127) / 6 : 9 + (dbm + 73) / 10);
const sigFrac = (s) => (s <= 9 ? (s / 9) * 0.66 : 0.66 + ((s - 9) / 6) * 0.34);

export const SIG_KLEIN = {
  threshold: null, frac: sigFrac,
  ticks: [[1, '1'], [5, '5'], [9, '9'], [13, '+40']],
  minor: [2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15], label: 26,
};
export const SWR_KLEIN = {
  threshold: 2.5, frac: (v) => (v - 1) / 2,
  ticks: [[1, '1'], [2, '2'], [3, '3']],
  minor: [1.25, 1.5, 1.75, 2.25, 2.5, 2.75], label: 26,
};

/** Baut ein Zeigerinstrument in das svg; gibt update(wert|null) zurueck. */
export function zeiger(svg, cfg, kennung) {
  svg.setAttribute('viewBox', ZEIGER_VIEWBOX);
  svg.replaceChildren();
  const pt = (r, d) => { const a = d * Math.PI / 180; return [CX + Math.cos(a) * r, CY - Math.sin(a) * r]; };
  const deg = (v) => D0 + (D1 - D0) * cfg.frac(v);
  const arc = (r, d0, d1) => { const p0 = pt(r, d0), p1 = pt(r, d1);
    return `M ${p0[0].toFixed(1)} ${p0[1].toFixed(1)} A ${r} ${r} 0 0 1 ${p1[0].toFixed(1)} ${p1[1].toFixed(1)}`; };
  const sector = (ro, ri, d0, d1) => { const a = pt(ro, d0), b = pt(ro, d1), c = pt(ri, d1), d = pt(ri, d0);
    return `M ${a[0].toFixed(1)} ${a[1].toFixed(1)} A ${ro} ${ro} 0 0 1 ${b[0].toFixed(1)} ${b[1].toFixed(1)} L ${c[0].toFixed(1)} ${c[1].toFixed(1)} A ${ri} ${ri} 0 0 0 ${d[0].toFixed(1)} ${d[1].toFixed(1)} Z`; };
  const defs = el('defs', {}); svg.appendChild(defs);
  const grad = (id, col, o1, o2, o3) => { const g = el('radialGradient', { id: kennung + id, cx: CX, cy: CY, r: R - W / 2 - 2, gradientUnits: 'userSpaceOnUse' });
    g.appendChild(el('stop', { offset: '0.18', 'stop-color': col, 'stop-opacity': '0' }));
    g.appendChild(el('stop', { offset: '0.72', 'stop-color': col, 'stop-opacity': o2 }));
    g.appendChild(el('stop', { offset: '1', 'stop-color': col, 'stop-opacity': o3 })); defs.appendChild(g); };
  grad('fade', C.meas, 0, '.10', '.26'); grad('fadeD', C.danger, 0, '.12', '.30');
  const gg = el('radialGradient', { id: kennung + 'glow', cx: CX, cy: CY, r: R * 0.62, gradientUnits: 'userSpaceOnUse' });
  gg.appendChild(el('stop', { offset: '0', 'stop-color': C.meas, 'stop-opacity': '.16' }));
  gg.appendChild(el('stop', { offset: '0.55', 'stop-color': C.meas, 'stop-opacity': '.05' }));
  gg.appendChild(el('stop', { offset: '1', 'stop-color': C.meas, 'stop-opacity': '0' })); defs.appendChild(gg);
  const glow = el('path', { d: sector(R * 0.62, 0, D0, D1), fill: `url(#${kennung}glow)` }); svg.appendChild(glow);
  svg.appendChild(el('path', { d: arc(R, D0, D1), fill: 'none', stroke: C.inset, 'stroke-width': W }));
  svg.appendChild(el('path', { d: arc(R + W / 2 - 1.1, D0, D1), fill: 'none', stroke: '#000', 'stroke-width': 2.2, opacity: '.75' }));
  if (cfg.threshold != null) {
    svg.appendChild(el('path', { d: arc(R, deg(cfg.threshold), D1), fill: 'none', stroke: C.danger, 'stroke-width': W, opacity: '.42' }));
  }
  svg.appendChild(el('path', { d: arc(R, D0, D1), fill: 'none', stroke: C.arc, 'stroke-width': 1 }));
  const fill = el('path', { fill: `url(#${kennung}fade)` }); svg.appendChild(fill);
  cfg.minor.forEach((v) => { const a = deg(v), p1 = pt(R - W / 2 - 1, a), p2 = pt(R - W / 2 - 5, a);
    svg.appendChild(el('line', { x1: p1[0].toFixed(1), y1: p1[1].toFixed(1), x2: p2[0].toFixed(1), y2: p2[1].toFixed(1), stroke: C.face, 'stroke-width': 1.2, opacity: '.55' })); });
  cfg.ticks.forEach((t) => { const a = deg(t[0]), p1 = pt(R - W / 2 - 1, a), p2 = pt(R - W / 2 - 11, a), lp = pt(R - W / 2 - 28, a);
    svg.appendChild(el('line', { x1: p1[0].toFixed(1), y1: p1[1].toFixed(1), x2: p2[0].toFixed(1), y2: p2[1].toFixed(1), stroke: C.face, 'stroke-width': 2 }));
    const tx = el('text', { x: lp[0].toFixed(1), y: (lp[1] + cfg.label * 0.35).toFixed(1), fill: C.face, 'font-size': cfg.label,
      'font-family': 'ui-monospace, Menlo, monospace', 'text-anchor': 'middle' }); tx.textContent = t[1]; svg.appendChild(tx); });
  const edge = el('line', { stroke: C.meas, 'stroke-width': 1.8, opacity: '.75' });
  const needle = el('line', { stroke: C.meas, 'stroke-width': 3.2, 'stroke-linecap': 'round' });
  svg.appendChild(edge); svg.appendChild(needle);
  svg.appendChild(el('circle', { cx: CX, cy: CY, r: 7, fill: '#15151a', stroke: C.dim, 'stroke-width': 1.4 }));
  return (v) => {
    if (v === null || v === undefined || isNaN(v)) {          // ohne Messung nur das Zifferblatt
      fill.setAttribute('d', ''); needle.setAttribute('visibility', 'hidden'); edge.setAttribute('visibility', 'hidden');
      glow.setAttribute('opacity', '.55'); return;
    }
    needle.removeAttribute('visibility'); edge.removeAttribute('visibility');
    const a = deg(v), over = cfg.threshold != null && v > cfg.threshold, col = over ? C.danger : C.meas;
    fill.setAttribute('d', sector(R - W / 2 - 2, 12, D0, a));
    fill.setAttribute('fill', `url(#${kennung}${over ? 'fadeD' : 'fade'})`);
    glow.setAttribute('opacity', over ? '.85' : '1');
    const n0 = pt(10, a), n1 = pt(R - W / 2 - 3, a);
    needle.setAttribute('x1', n0[0].toFixed(1)); needle.setAttribute('y1', n0[1].toFixed(1));
    needle.setAttribute('x2', n1[0].toFixed(1)); needle.setAttribute('y2', n1[1].toFixed(1)); needle.setAttribute('stroke', col);
    const e0 = pt(24, a), e1 = pt(R - W / 2 - 3, a);
    edge.setAttribute('x1', e0[0].toFixed(1)); edge.setAttribute('y1', e0[1].toFixed(1));
    edge.setAttribute('x2', e1[0].toFixed(1)); edge.setAttribute('y2', e1[1].toFixed(1)); edge.setAttribute('stroke', col);
  };
}

/** Kompass-Zifferblatt nach RotatorItem: Punkte alle 10 Grad, grosse alle 45. */
export function kompassBlatt(g) {
  g.replaceChildren();
  const r = 78, rt = 100, h = 220;   // Halbmesser 0,8 h/2 beim Mac, hier im 220er Mass
  for (let d = 0; d < 360; d += 10) {
    const a = (d - 90) * Math.PI / 180, gross = d % 45 === 0;
    g.appendChild(el('circle', { cx: (r * Math.cos(a)).toFixed(1), cy: (r * Math.sin(a)).toFixed(1),
      r: gross ? 4.2 : 1.8, fill: gross ? (d === 0 ? C.danger : '#909096') : '#58585e' }));
  }
  for (const [t, d] of [['N', 0], ['E', 90], ['S', 180], ['W', 270]]) {
    const a = (d - 90) * Math.PI / 180;
    const x = el('text', { x: (rt * Math.cos(a)).toFixed(1), y: (rt * Math.sin(a) + 7).toFixed(1), fill: d === 0 ? C.danger : '#909096',
      'font-size': 21, 'font-weight': d === 0 ? 700 : 500, 'font-family': 'ui-monospace, Menlo, monospace', 'text-anchor': 'middle' });
    x.textContent = t; g.appendChild(x);
  }
}
