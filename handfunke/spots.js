// Spots im Bild — die reinen Teile.
//
// Kein DOM, kein WebSocket, kein Canvas. Was hier steht, ist die Rechnerei,
// die auf einem Telefon still falsch sein kann:
//
//   1. **Die Stelle.** Ein Spot sitzt auf einer Frequenz, das Bild zeigt eine
//      Spanne. Rechnet man die Stelle anders aus als den Abstimmstrich, sitzt
//      die Marke ein paar Pixel daneben — und wer sie antippt, landet neben
//      der Station. Darum dieselbe Rechnung wie `frequenzAnStelle()`, nur
//      andersherum.
//
//   2. **Die Beschriftungen.** Zwei Spots 300 Hz auseinander sind auf einem
//      Telefon derselbe Pixel. Uebereinandergedruckte Rufzeichen sind nicht
//      nur haesslich, sie sind unlesbar — und eine unlesbare Marke ist
//      schlimmer als keine, weil man sie trotzdem antippt.
//
//   3. **Das Alter.** Ein Spot von vor zwanzig Minuten ist meistens weg.
//      Er darf stehen, aber nicht so hell wie einer von vor einer Minute.
//
// Gegenstueck in Longpath: `SpotAuswahl` und der Befehl `spots:` in
// TciServer.cpp.

/** `spot_zeile:`-Argumente -> Satz. Gibt `null`, wenn nichts dasteht. */
export function spotLesen(args) {
  const nr = parseInt(args[0], 10);
  const hz = parseInt(args[1], 10);
  const ruf = (args[2] || '').trim().toUpperCase();
  if (isNaN(nr) || isNaN(hz) || hz <= 0 || !ruf) return null;
  const alter = parseInt(args[5], 10);
  return {
    nr, hz, ruf,
    mode:   (args[3] || '').trim().toUpperCase(),
    quelle: (args[4] || '').trim().toUpperCase(),
    alterSek: isNaN(alter) ? 0 : Math.max(0, alter),
  };
}

/** Ab hier gilt ein Spot als alt und wird blasser; bei kVerblasstAb ist er
 *  so blass wie er wird. Er verschwindet NICHT — das entscheidet Longpath
 *  anhand der Lebenszeit, und zwei Stellen, die ueber dasselbe befinden,
 *  laufen auseinander. */
export const kFrischBis   = 120;    // s
export const kVerblasstAb = 1800;   // s
export const kMindestDeckung = 0.3;

/** 1.0 frisch, kMindestDeckung ab kVerblasstAb, dazwischen linear. */
export function deckung(alterSek) {
  const a = Math.max(0, alterSek || 0);
  if (a <= kFrischBis) return 1;
  if (a >= kVerblasstAb) return kMindestDeckung;
  const t = (a - kFrischBis) / (kVerblasstAb - kFrischBis);
  return 1 - t * (1 - kMindestDeckung);
}

/**
 * Marken fuer das Bild.
 *
 * `mitteHz` ist die abgestimmte Frequenz (= Bildmitte, Longpath fuehrt sie
 * nach), `spanneHz` die gezeigte Breite, `breitePx` die Breite des Bildes.
 *
 * `beschriftungPx` ist die geschaetzte Breite eines Rufzeichens. Marken,
 * deren Beschriftungen sich ueberschneiden wuerden, kommen in die naechste
 * Reihe — nicht uebereinander. Zwei Spots 300 Hz auseinander sind auf einem
 * Telefon derselbe Pixel, und das ist nicht der Ausnahmefall: ein Pile-up
 * sieht genau so aus.
 *
 * Gibt `{x, hz, ruf, mode, quelle, reihe, deckung}` zurueck, nach x sortiert.
 * Was ausserhalb des Bildes laege, faellt weg.
 */
export function marken(zeilen, mitteHz, spanneHz, breitePx,
                       { beschriftungPx = 54, reihen = 3 } = {}) {
  const aus = [];
  if (!Array.isArray(zeilen) || !(spanneHz > 0) || !(breitePx > 0)) return aus;

  for (const z of zeilen) {
    if (!z || !(z.hz > 0)) continue;
    // Dieselbe Rechnung wie frequenzAnStelle(), nur andersherum.
    const anteil = (z.hz - mitteHz) / spanneHz + 0.5;
    if (anteil < 0 || anteil > 1) continue;
    aus.push({
      x: anteil * breitePx,
      hz: z.hz, ruf: z.ruf, mode: z.mode, quelle: z.quelle,
      alterSek: z.alterSek || 0,
      deckung: deckung(z.alterSek),
      reihe: 0,
    });
  }

  aus.sort((a, b) => a.x - b.x);

  // Reihen vergeben: je Reihe merken, wo die letzte Beschriftung endet.
  // Passt eine Marke in keine Reihe, bleibt sie in der letzten — lieber
  // eine enge Stelle als eine Marke, die es nicht gibt.
  const endet = new Array(reihen).fill(-Infinity);
  for (const m of aus) {
    const anfang = m.x - beschriftungPx / 2;
    let r = 0;
    while (r < reihen && anfang < endet[r]) r++;
    m.reihe = Math.min(r, reihen - 1);
    endet[m.reihe] = m.x + beschriftungPx / 2;
  }
  return aus;
}

/**
 * Die Marke, deren BESCHRIFTUNG unter dem Finger liegt — oder `null`.
 *
 * Getrennt von `trefferBei`, und das ist der ganze Punkt: auf dem
 * Rufzeichen steht das Rufzeichen (QRZ, ins Logblatt), auf dem Strich steht
 * die Frequenz (abstimmen). Wer beides auf denselben Tipp legt, muss sich
 * für eines entscheiden und nimmt dem Bediener das andere weg.
 *
 * Geprüft wird gegen den Kasten, den das Zeichnen WIRKLICH gesetzt hat
 * (`m.kasten`), nicht gegen eine zweite Rechnung. Zwei Rechnungen laufen
 * auseinander, und dann trifft der Finger etwas anderes als das Auge sieht.
 * Marken ohne Kasten (noch nie gezeichnet) zählen nicht.
 */
export function trefferAufSchrift(marken, x, y, luft = 4) {
  for (const m of marken || []) {
    const k = m && m.kasten;
    if (!k) continue;
    if (x >= k.x - luft && x <= k.x + k.b + luft
        && y >= k.y - luft && y <= k.y + k.h + luft) {
      return m;
    }
  }
  return null;
}

/**
 * Die Marke unter dem Finger, oder `null`.
 *
 * `radiusPx` ist grosszuegig: ein Finger ist breiter als ein Strich. Bei
 * zwei Treffern gewinnt der naehere — und bei gleichem Abstand der ERSTE,
 * damit zweimal Tippen nicht zweimal etwas anderes trifft.
 */
export function trefferBei(marken, x, radiusPx = 22) {
  let beste = null, bester = Infinity;
  for (const m of marken || []) {
    const d = Math.abs(m.x - x);
    if (d <= radiusPx && d < bester) { beste = m; bester = d; }
  }
  return beste;
}
