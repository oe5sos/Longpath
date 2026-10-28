# FT8 im Contest — was fehlt, was schon geht, und was falsch beschriftet ist

**2026-10-28.** Betreiberfrage: „und was ist mit dem contest ft8
programm?" Gemeint ist MSHV und das, was Longpath davon nicht kann.

Dieses Blatt ist eine **Untersuchung, kein Bauplan** — der Betreiber hat
„schaue" gesagt, nicht „baue". Was daraus schon gebaut ist, steht unten
unter *Gebaut*.

---

## Die maßgebliche Quelle

`../wsjtx` (SourceForge `p/wsjt/wsjtx`, geholt 2026-10-28, Commit
`b4f9a43`), Datei `lib/77bit/packjt77.f90`. GPLv3 — dieselbe Lizenz, unter
der Longpath steht.

Sie wurde geholt, weil die Bitlagen der Contestsätze **nirgends sonst
belegbar** waren: `third_party/ft8_lib` führt sie nur als Tabelle im
Kopfkommentar, und nach der Projektregel (`CLAUDE.md`, „What Counts As
Guessing") darf ein Format nicht aus einer Tabelle erraten werden.

---

## Drei Befunde

### 1. Satz `0.2` ist tot — auch bei WSJT-X

`ft8_lib` führt ihn als `FTX_MESSAGE_TYPE_EU_VHF`:

```
0.2   PA3XYZ/P R 590003 IO91NP   28 1 1 3 12 25   70   EU VHF contest
```

WSJT-X packt ihn aber gar nicht mehr aus (`packjt77.f90:330-331`):

```fortran
else if(i3.eq.0 .and. n3.eq.2) then
   unpk77_success=.false.
```

Wer also `0.2` baut, baut etwas, das auf dem Band nicht vorkommt.

### 2. `ft8_lib` beschriftet `i3=5` falsch

Die Bibliothek führt `i3=5` als `FTX_MESSAGE_TYPE_WWROF` — mit
Fragezeichen im eigenen Kommentar („WWROF contest ?"). In Wahrheit ist
das der **EU-VHF-Contestsatz** (`packjt77.f90:186, 593-610`):

```fortran
! Type 5  <PA3XYZ> <G4ABC/P> R 590003 IO91NP      h12 h22 r1 s3 S11 g25
read(c77,1060) n12,n22,ir,irpt,iserial,igrid6
1060 format(b12,b22,b1,b3,b11,b25)
```

74 Bit: zwei **gehashte** Rufzeichen (12 und 22 Bit), ein R-Bit, 3 Bit
Rapport (`nrs = 52 + irpt`), 11 Bit laufende Nummer (vierstellig
ausgegeben), 25 Bit Locator in sechs Stellen.

Der Locator rechnet sich mit den Radizes 18, 18, 10, 10, 24, 24
(`to_grid6`, `packjt77.f90:1577-1607`), gültiger Bereich
`0 … 18 662 399`.

### 3. Die `/P`-Form läuft heute schon

`i3=2` (`PA3XYZ/P GM4ABC/P R JO22`) geht bei `ft8_lib` durch
`decode_std`, und `unpack28` hängt das `/P` korrekt an
(`message.c:124-130`). Hier fehlt **nichts**.

---

## Was EU VHF wirklich braucht

| Stück | Lage |
| --- | --- |
| `i3=2`, die `/P`-Form | **geht schon** |
| Rufzeichen-Hashtabelle | **gebaut** (siehe unten) — bei `i3=5` sind BEIDE Rufzeichen gehasht, ohne Tabelle ist der Satz wertlos |
| `i3=5` auspacken (h12/h22/r1/s3/S11/g25 + `to_grid6`) | offen |
| `i3=5` packen | offen |
| Laufende Nummer im QSO-Ablauf führen und hochzählen | offen, und der größere Teil |
| Oberfläche: Nummer anzeigen, Contestsatz wählen | offen |

**Wo das hingehört:** nicht in `third_party/ft8_lib`. Die Bibliothek
steht unter MIT, die Vorlage unter GPLv3 — eine Ableitung davon in den
fremden Baum zu legen, verdirbt dessen Herkunft. Richtig ist eine
eigene Datei in `src/core/`, die den Rohsatz über die öffentlichen
Zugriffe von `ft8_lib` liest.

---

## Gebaut: die Rufzeichen-Hashtabelle

Dabei fiel ein Mangel auf, der **den Alltagsbetrieb** betrifft und
nichts mit Contesten zu tun hat.

Im 77-Bit-Format steht nicht jedes Rufzeichen im Klartext. Passt es
nicht in die 28 Bit der Normalform, trägt die Nachricht nur eine 22-,
12- oder 10-Bit-Prüfsumme; auflösen kann sie nur, wer dasselbe
Rufzeichen vorher im Klartext gehört hat. `ft8_lib` sieht dafür eine
Schnittstelle vor (`ftx_callsign_hash_interface_t`) — und Longpath
übergab dort `nullptr` (`Ft8Decoder.cpp:129`).

Die Folge steht wörtlich in der Bibliothek (`message.c:734`):

```c
strcpy(callsign, "<...>");
```

Genau das stand bei jedem Sonderrufzeichen in den Funden, statt des
Rufzeichens.

`src/core/Ft8Rufzeichen.{h,cpp}` schließt das: ein Speicher nach allen
drei Hashlängen, die aus **einer** Rechnung abfallen
(`n12 = n22 >> 10`, `n10 = n22 >> 12`, `message.c:707-708`), begrenzt
auf 2000 Einträge, über eine Sperre für Arbeits- und Hauptfaden.

Geprüft wird durch die Bibliothek selbst — packen, auspacken — mit
Gegenprobe bei geleerter Tabelle. Der erste Anlauf der Gegenprobe war
falsch: `CQ PA3XYZ/MM` trägt das Sonderrufzeichen vollständig in 58 Bit
und hasht gar nichts, der Fall lief deshalb auch ohne Tabelle durch.
Gehasht wird der **Partner** eines Sonderrufzeichens; seither prüft der
Fall `K1ABC PA3XYZ/MM RR73`.

---

## Was offen bleibt

* `i3=5` auspacken und packen — Bitlage oben belegt, Code noch nicht.
* Ein Prüfvektor **vom Band**. `lib/77bit/encode77.out` listet
  Nachrichten, aber keine Bitmuster; ein selbstgebauter Vektor prüft
  nur die eigene Lesart gegen sich selbst.
* Der Contest-Ablauf (laufende Nummer) ist mehr Arbeit als der Codec.

Der Betreiber kündigt Contestbetrieb vorher an — bis dahin ist das
keine Dringlichkeit.
