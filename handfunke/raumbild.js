/**
 * raumbild.js — die räumliche Spektrumansicht fürs Telefon.
 *
 * Longpath hat sie am Mac als `src/gui/DssRenderer.{h,cpp}` (dort wieder
 * aus AetherSDR portiert, GPLv3). Das hier ist dieselbe Ansicht auf einem
 * Canvas: **dieselben Formeln, dieselben Konstanten, dieselbe Palette.**
 * Wo diese Datei und `DssRenderer.cpp` auseinandergehen, hat der C++-Code
 * recht und diese Datei den Fehler.
 *
 * Es ist kein echtes 3D und braucht darum auch kein WebGL: die Zeilen
 * werden nach hinten gestaffelt, jede füllt bis zum Boden, und ein
 * Horizont je Bildspalte merkt sich, wie weit eine nähere Zeile die
 * Spalte schon beansprucht hat. So wird jeder Vorhangpixel genau einmal
 * geschrieben — das ist derselbe Kunstgriff, mit dem die Fassung am Mac
 * aus 0,6 s je Bild 7 ms gemacht hat.
 *
 * Die Farben kommen von aussen (`palette(dbm)`), damit Wasserfall und
 * Raumbild nie auseinanderlaufen koennen.
 */

// ── Die Konstanten. Alle aus DssRenderer, keine davon geraten ──────────────
//
// Geometrie (DssRenderer.h): die vier teilt sich der Mac-Code mit einem
// spaeteren GPU-Pfad, damit beide nicht driften koennen.
export const ZEILEN_SICHTBAR   = 96;     // kVisibleRows: Tiefe nach hinten
export const SPALTEN           = 768;    // kCols

// ── Frequenztreu: jede Zeile behaelt die volle Breite ─────────────────────
//
// Nachgezogen aus PR #344 (`kTrueScaleWidthFrac`). Vorher verjuengten sich
// die hinteren Zeilen auf 60 % -- und damit stand ein Traeger dort nicht
// mehr ueber seiner Frequenz. Am Bildschirmfoto des Betreibers gemessen:
// ein Traeger auf 14.0758 MHz zog seinen Kamm auf 14.1066, also 31 kHz zur
// Mitte hin. Der Wasserfall darunter verjuengt sich nie, und genau um
// diesen Betrag widersprachen sich die beiden Bilder.
//
// Die Tiefe tragen jetzt der Anstieg nach hinten, der Dunst und die
// Abdunklung allein. Die KAMMHOEHE schrumpft weiter mit dem alten Wert --
// sonst saehe das Bild flach aus.
const BREITE_HINTEN            = 1.00;   // kTrueScaleWidthFrac
const KAMM_HINTEN              = 0.60;   // kBackWidthFrac, nur noch fuer die Hoehe
const TIEFE_ANTEIL             = 0.58;   // kDepthSpanFrac
const KAMM_VORN                = 0.46;   // kFrontMaxRidgeFrac
const DUNST                    = 0.16;   // kHaze
// Nur fuer die gemalte Fassung (DssRenderer.cpp):
const MIN_HELLIGKEIT           = 0.50;   // kMinDim
const ZEIT_ALPHA               = 0.60;   // kTemporalAlpha
const HANG_VERSTAERKUNG        = 0.55;   // kSlopeGain
const SCHATTEN_UNTEN           = 0.68;   // kShadeLo
const SCHATTEN_OBEN            = 1.32;   // kShadeHi

const klemme = (v, a, b) => (v < a ? a : v > b ? b : v);
const median3 = (a, b, c) => Math.max(Math.min(a, b), Math.min(Math.max(a, b), c));

/** Qt's QColor::lighter(165): die Helligkeit mal 1,65, Farbton und Saettigung bleiben. */
function heller(r, g, b, faktor) {
  const max = Math.max(r, g, b);
  if (max <= 0) { return [0, 0, 0]; }
  const neu = Math.min(255, max * faktor);
  const k = neu / max;
  return [Math.min(255, r * k), Math.min(255, g * k), Math.min(255, b * k)];
}

const mische = (a, b, t) => a + (b - a) * t;

export class Raumbild {
  constructor() {
    this.zeilen = [];        // neueste zuerst
    this.rohVor1 = null;     // fuer den Median aus drei Zeiten
    this.rohVor2 = null;
    this.rohZahl = 0;
    this.bild = null;        // ImageData
    this.breite = 0;
    this.hoehe = 0;
  }

  leeren() {
    this.zeilen = [];
    this.rohVor1 = this.rohVor2 = null;
    this.rohZahl = 0;
  }

  get zeilenZahl() { return Math.min(this.zeilen.length, ZEILEN_SICHTBAR); }

  /**
   * Eine frische Spektrumzeile aufnehmen (dBm, beliebig viele Punkte).
   *
   * Drei Schritte wie am Mac: auf SPALTEN heruntergerechnet und dabei die
   * Spitzen behalten (ein Mittelwert fraesse genau die Traeger weg, die
   * man sehen will), Median aus drei Zeiten gegen Knackser, und eine
   * leichte zeitliche Glaettung.
   */
  zeileAufnehmen(punkteDbm) {
    const roh = this._herunterrechnen(punkteDbm);
    let zeile;
    if (this.rohZahl >= 2) {
      zeile = new Float32Array(SPALTEN);
      for (let c = 0; c < SPALTEN; c++) {
        zeile[c] = median3(roh[c], this.rohVor1[c], this.rohVor2[c]);
      }
    } else {
      zeile = roh.slice();
    }
    this.rohVor2 = this.rohVor1;
    this.rohVor1 = roh;
    this.rohZahl++;

    const vorn = this.zeilen[0];
    if (vorn) {
      for (let c = 0; c < SPALTEN; c++) {
        zeile[c] = ZEIT_ALPHA * zeile[c] + (1 - ZEIT_ALPHA) * vorn[c];
      }
    }
    this.zeilen.unshift(zeile);
    if (this.zeilen.length > ZEILEN_SICHTBAR + 8) { this.zeilen.length = ZEILEN_SICHTBAR + 8; }
  }

  /** Spitzen behalten, nicht mitteln. */
  _herunterrechnen(punkte) {
    const aus = new Float32Array(SPALTEN);
    const n = punkte.length;
    if (n === 0) { aus.fill(-200); return aus; }
    for (let c = 0; c < SPALTEN; c++) {
      const a = Math.floor((c * n) / SPALTEN);
      const b = Math.max(a + 1, Math.floor(((c + 1) * n) / SPALTEN));
      let m = -Infinity;
      for (let i = a; i < b && i < n; i++) { if (punkte[i] > m) { m = punkte[i]; } }
      aus[c] = (m === -Infinity) ? -200 : m;
    }
    return aus;
  }

  /**
   * Das Bild neu malen.
   *
   * `palette(dbm)` liefert [r,g,b]; `grund` ist die Hintergrundfarbe, in
   * die der Dunst nach hinten hinein verblasst. `bodenDbm` ist der
   * Rauschboden: er liegt auf der Grundlinie, `bodenDbm + spanneDb`
   * erreicht den vollen Kamm. `zKurve` unter 1 hebt die schwachen
   * Signale an, damit sie nicht flach am Boden kleben.
   */
  malen(ctx, breite, hoehe, { bodenDbm, spanneDb, zKurve = 0.7, palette, grund }) {
    const W = Math.max(1, Math.floor(breite));
    const H = Math.max(1, Math.floor(hoehe));
    if (!this.bild || this.breite !== W || this.hoehe !== H) {
      this.bild = ctx.createImageData(W, H);
      this.breite = W; this.hoehe = H;
    }
    const px = this.bild.data;

    // Grundfarbe ueberall.
    const [gr, gg, gb] = grund;
    for (let i = 0; i < px.length; i += 4) {
      px[i] = gr; px[i + 1] = gg; px[i + 2] = gb; px[i + 3] = 255;
    }
    if (!this.zeilenZahl || !(spanneDb > 0)) {
      ctx.putImageData(this.bild, 0, 0);
      return;
    }

    const zc = Math.max(0.05, zKurve);
    const tiefeSpanne = H * TIEFE_ANTEIL;
    const kammVorn = H * KAMM_VORN;

    // Der Horizont: je Bildspalte die oberste Zeile, die eine naehere
    // Spur schon beansprucht hat. Alles darunter ist fuer die Spuren
    // dahinter verdeckt.
    const horizont = new Int32Array(W).fill(H);

    const ys = new Float64Array(SPALTEN);
    const fuell = new Uint8Array(SPALTEN * 3);
    const kamm = new Uint8Array(SPALTEN * 3);

    for (let alter = 0; alter < this.zeilenZahl; alter++) {
      const tiefe = alter / ZEILEN_SICHTBAR;
      const breitenAnteil = 1 - tiefe * (1 - BREITE_HINTEN);
      const einzug = W * (1 - breitenAnteil) * 0.5;
      const zeilenB = W - 2 * einzug;
      const grundlinieY = H - tiefe * tiefeSpanne;
      // Die Hoehe folgt dem alten Verjuengungswert, die Breite nicht mehr.
      const kammMax = kammVorn * (1 - tiefe * (1 - KAMM_HINTEN));
      const dim = MIN_HELLIGKEIT + (1 - MIN_HELLIGKEIT) * (1 - tiefe);
      const zeile = this.zeilen[alter];

      for (let c = 0; c < SPALTEN; c++) {
        let staerke = klemme((zeile[c] - bodenDbm) / spanneDb, 0, 1);
        staerke = Math.pow(staerke, zc);
        ys[c] = grundlinieY - staerke * kammMax;
      }

      const hangMass = kammMax > 1 ? kammMax : 1;
      for (let c = 0; c < SPALTEN; c++) {
        const hang = (ys[Math.max(0, c - 1)] - ys[Math.min(SPALTEN - 1, c + 1)]) / hangMass;
        const schatten = klemme(1 + HANG_VERSTAERKUNG * hang, SCHATTEN_UNTEN, SCHATTEN_OBEN);
        const [pr, pg, pb] = palette(zeile[c]);
        const d = tiefe * DUNST;
        const f = dim * schatten;
        fuell[c * 3]     = klemme(mische(pr, gr, d) * f, 0, 255);
        fuell[c * 3 + 1] = klemme(mische(pg, gg, d) * f, 0, 255);
        fuell[c * 3 + 2] = klemme(mische(pb, gb, d) * f, 0, 255);
        const [hr, hg, hb] = heller(pr, pg, pb, 1.65);
        kamm[c * 3]     = klemme(mische(hr, gr, d) * dim, 0, 255);
        kamm[c * 3 + 1] = klemme(mische(hg, gg, d) * dim, 0, 255);
        kamm[c * 3 + 2] = klemme(mische(hb, gb, d) * dim, 0, 255);
      }

      // Pixelmitte wie bei einer Flaechenfuellung ohne Kantenglaettung.
      const x0 = Math.max(0, Math.ceil(einzug - 0.5));
      const x1 = Math.min(W - 1, Math.floor(einzug + zeilenB - 0.5));
      const spalteJePx = (SPALTEN - 1) / Math.max(zeilenB, 1e-9);
      const kammDicke = (alter === 0) ? 1.6 : 1.0;

      for (let x = x0; x <= x1; x++) {
        const cf = (x + 0.5 - einzug) * spalteJePx;
        const c0 = klemme(Math.floor(cf), 0, SPALTEN - 1);
        const c1 = Math.min(SPALTEN - 1, c0 + 1);
        const t = cf - c0;
        const y = ys[c0] + (ys[c1] - ys[c0]) * t;

        const obenHier = Math.max(0, Math.round(y));
        const bisher = horizont[x];
        if (obenHier >= bisher) { continue; }   // ganz verdeckt

        const q = (c0 + (t > 0.5 ? 1 : 0)) * 3;
        const fr = fuell[q], fg = fuell[q + 1], fb = fuell[q + 2];
        const kr = kamm[q], kg = kamm[q + 1], kb = kamm[q + 2];

        for (let yy = obenHier; yy < bisher; yy++) {
          const i = (yy * W + x) * 4;
          const amKamm = (yy - obenHier) < kammDicke;
          px[i]     = amKamm ? kr : fr;
          px[i + 1] = amKamm ? kg : fg;
          px[i + 2] = amKamm ? kb : fb;
          px[i + 3] = 255;
        }
        horizont[x] = obenHier;
      }
    }

    ctx.putImageData(this.bild, 0, 0);
  }
}
