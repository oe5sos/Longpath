// Logbuch in der App — die reinen Teile.
//
// Hier steht ABSICHTLICH kein DOM und kein WebSocket. Zwei Dinge gehen
// beim Logbuch leicht schief, und beide lassen sich nur dann pruefen,
// wenn sie ohne Telefon laufen:
//
//   1. Die Sammelstelle. Longpath schickt je Kontakt eine Zeile und erst
//      danach `log_last_ok:<anzahl>`. Trifft eine zweite Anfrage ein,
//      waehrend die erste noch laeuft, mischen sich zwei Listen. Die
//      Liste sieht dann vollstaendig aus und ist es nicht — das ist
//      schlimmer als eine leere.
//
//   2. Der Dupe-Satz. Wer "schon gearbeitet" sagt, obwohl Band oder
//      Betriebsart unbekannt sind, verhindert einen Kontakt, den es noch
//      nicht gibt — und niemand erfaehrt davon, weil nichts passiert.
//      Unbekannt muss sich darum wie unbekannt lesen, nicht wie "nein".
//
// Gegenstueck in Longpath: `LogbuchRueckschau` und die beiden Befehle
// `log_last:` / `log_dup:` in TciServer.cpp.

/**
 * Sammelt die Zeilen EINER Abfrage und gibt die Liste erst heraus, wenn
 * die Abschlusszeile dieselbe Anzahl nennt.
 */
export class Sammelstelle {
  constructor() { this.zeilen = []; this.offen = false; }

  /** Eine Abfrage beginnt. Alles Vorige ist damit hinfaellig. */
  beginnen() { this.zeilen = []; this.offen = true; }

  /**
   * Eine `log_qso_zeile:`-Zeile. Nummer 0 beginnt eine neue Liste — so
   * uebersteht die Sammelstelle auch eine Antwort, deren Anfang sie nicht
   * gesehen hat (Seite neu geladen, Abfrage noch unterwegs).
   */
  zeile(satz) {
    if (!satz) return;
    if (satz.nr === 0) { this.zeilen = []; this.offen = true; }
    if (!this.offen) return;
    this.zeilen.push(satz);
  }

  /**
   * Die Abschlusszeile. Gibt die Liste zurueck, wenn die Anzahl stimmt,
   * sonst `null` — dann fehlt etwas, und die Seite darf keine
   * unvollstaendige Liste als vollstaendig zeigen.
   */
  abschluss(anzahl) {
    const fertig = this.offen && this.zeilen.length === anzahl;
    const l = this.zeilen;
    this.zeilen = [];
    this.offen = false;
    return fertig ? l : null;
  }
}

/** `log_qso_zeile:`-Argumente -> Satz. Gibt `null`, wenn nichts dasteht. */
export function zeileLesen(args) {
  const nr = parseInt(args[0], 10);
  const ruf = (args[3] || '').trim().toUpperCase();
  if (isNaN(nr) || !ruf) return null;
  return {
    nr,
    datum: (args[1] || '').trim(),   // yyyymmdd, leer wenn ohne Zeit
    zeit:  (args[2] || '').trim(),   // hhmmss
    ruf,
    band:  (args[4] || '').trim(),
    mode:  (args[5] || '').trim(),
    rstS:  (args[6] || '').trim(),
    rstE:  (args[7] || '').trim(),
  };
}

/** `log_dup_ok:`-Argumente -> Befund. */
export function befundLesen(args) {
  const ruf = (args[0] || '').trim().toUpperCase();
  if (!ruf) return null;
  const anzahl = parseInt(args[1], 10);
  return {
    ruf,
    anzahl: isNaN(anzahl) ? 0 : anzahl,
    datum: (args[2] || '').trim(),
    zeit:  (args[3] || '').trim(),
    band:  (args[4] || '').trim(),
    mode:  (args[5] || '').trim(),
    gleichesBand: (args[6] || '') === '1',
    dupe:         (args[7] || '') === '1',
  };
}

/** "20261002" + "143000" -> "02.10. 14:30". Leeres Datum -> "". */
export function zeitKurz(datum, zeit) {
  if (!/^\d{8}$/.test(datum || '')) return '';
  const tag = datum.slice(6, 8), mon = datum.slice(4, 6);
  if (!/^\d{4,6}$/.test(zeit || '')) return `${tag}.${mon}.`;
  return `${tag}.${mon}. ${zeit.slice(0, 2)}:${zeit.slice(2, 4)}`;
}

/**
 * Was unter dem Rufzeichenfeld steht.
 *
 * `{text, art}` mit art 'neu' | 'bekannt' | 'dupe' | '' — die Seite
 * entscheidet die Farbe, nicht dieser Satz. Messing fuer bekannt und
 * dupe; kraeftiges Rot bleibt der Warnung (Hausregel).
 */
export function dupeSatz(b) {
  if (!b || !b.ruf) return { text: '', art: '' };
  if (b.anzahl <= 0) return { text: 'NEU — noch nie gearbeitet', art: 'neu' };

  const wann = zeitKurz(b.datum, b.zeit);
  const wo = [b.band, b.mode].filter(Boolean).join(' ');
  const mal = b.anzahl === 1 ? '1×' : `${b.anzahl}×`;
  const hinten = [wann, wo].filter(Boolean).join(' · ');

  // Die Unterscheidung, auf die es ankommt: dasselbe Band UND dieselbe
  // Betriebsart ist ein Dupe. Dasselbe Rufzeichen auf einem anderen Band
  // ist ein neuer Kontakt — und oft genau der, den man haben will.
  if (b.dupe) {
    return { text: `DUPE · ${wo || 'dieses Band'} schon gearbeitet`
                   + (wann ? ` · ${wann}` : ''), art: 'dupe' };
  }
  if (b.gleichesBand) {
    return { text: `${mal} · dieses Band schon, andere Betriebsart`
                   + (hinten ? ` · ${hinten}` : ''), art: 'bekannt' };
  }
  return { text: `${mal}${hinten ? ` · zuletzt ${hinten}` : ''}`
                 + ' · dieses Band noch nicht', art: 'bekannt' };
}
