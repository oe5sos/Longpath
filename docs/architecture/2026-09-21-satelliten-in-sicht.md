# Satelliten in Sicht (2026-09-21)

Zweiter Punkt der Liste aus der Logbuch-Recherche vom 2026-09-21 (nach
dem Hinflug auf der Karte): welche Amateurfunksatelliten standen zum
QSO-Zeitpunkt ueber dem Horizont, und welche stehen jetzt dort.

## Bausteine

| Teil | Datei | Herkunft |
|---|---|---|
| Propagator SGP4/SDP4 | `third_party/sgp4/SGP4.{cpp,h}` | Vallado, verbatim (MIT via python-sgp4) |
| TLE-Satz, Beobachtergeometrie | `src/core/sat/SatelliteTracker.{h,cpp}` | Longpath |
| Datei + CelesTrak-Abruf | `src/core/sat/TleStore.{h,cpp}` | Longpath |
| Dienst, Standort, Stempel | `src/core/sat/SatelliteService.{h,cpp}` | Longpath |
| Stempel beim Loggen | `RotorLogbookPanel::stampSatellites` | Longpath |
| Karte | `FlatMapWidget::paintSatellites`, `GlobeWidget` (gehoben), `QsoMapWindow` | Longpath |

Der Dienst gehoert dem Hauptfenster (`ensureSatellites()`), das Panel
bekommt ihn beim Anlegen, das Logbuchfenster und die Karte darueber.

## Geometrie

Vallados Propagator liefert den Ort im TEME-System. Beobachter:
geodaetisch → TEME ueber die Ortssternzeit θ = GMST(JD) + λ und den
WGS-72-Ellipsoid (derselbe Erdradius wie im Propagator). Sichtvektor
ρ = r_sat − r_obs, gedreht ins SEZ-System; Elevation = asin(Z/|ρ|),
Azimut = atan2(E, −S). Subsatellitenpunkt: Laenge = atan2(y, x) − GMST,
geodaetische Breite iterativ. TEME ≠ ECEF um Polbewegung und Nutation —
weit unter einem Grad, fuer „ueber dem Horizont" belanglos.

## Warum beim Loggen gestempelt wird

Ein TLE taugt Tage, nicht Jahre. Wer spaeter fragt, welche Satelliten
bei einem QSO von 2024 in Sicht waren, bekaeme mit heutigen Bahndaten
Unsinn. Deshalb rechnet das Panel im Moment des Loggens mit den
Bahndaten von jetzt und schreibt das Ergebnis als Text ins ADIF-Feld
`APP_LONGPATH_SATS`. Kein Netz, kein Standort, nichts in Sicht: kein
Feld.

## Geprueft

- `tst_satellite_tracker`: Pruefsumme, Drei-/Zweizeiler, Vallados
  Pruefvektor (Satellit 00005 zur Epoche, Hoehe/Breite/Zenit), Geometrie
  (Zenit 90°, Nord 0°, Ost 90°, Gegenfuessler), ISS-Hoehe ueber 3 h;
  mit `LONGPATH_TLE_FILE` der echte CelesTrak-Satz — QO-100 von JN67UT:
  33,9° Elevation, 163,9° Azimut, 25,9° O, 35 787 km (Tiefraumzweig).
- `tst_tle_store`: Abruf (nachgespielt), Datei, Alter, Fehlerseite
  ueberschreibt nie, ADIF-Rundweg des Stempels.
- `tst_qso_map_satellites`: Karte und Kugel zeichnen, Haken schaltet,
  Panel stempelt, Detailpaneel beschriftet.

## Offen

Die Liste aus der Recherche nennt noch „Live-Schichten" (Erdbeben,
Flugzeuge, Schiffe …). Nicht begonnen.
