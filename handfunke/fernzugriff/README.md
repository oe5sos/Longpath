# Fernzugriff: die App ohne das eigene Netz

Heute findet die App Longpath nur im eigenen WLAN: sie spricht den
TCI-Server auf Port 50001 an, und von unterwegs ist der nicht erreichbar.
Das hier ist der Weg nach draussen — kein VPN, keine Portfreigabe, kein
fester Anschluss.

**Der Weg ist NereusSDRs Rendezvous**, uebernommen statt neu erfunden:
ein kleiner Vermittlungsdienst, bei dem sich die Station anmeldet und
unter dem ein Telefon sie findet. Die Spezifikation ist
`docs/architecture/2026-09-23-rendezvous-v1.md` im Upstream und hat
Vorrang vor allem, was hier steht; der Dienst selbst liegt unveraendert in
`rendezvous/` (siehe `docs/attribution/NEREUS-RENDEZVOUS-PROVENANCE.md`).

## Was schon steht

| Datei | Was sie tut |
| --- | --- |
| `kennung.py` | Der Stationsschluessel (P-256), die Rendezvous-Id und der Registrierungsnachweis — Abschnitte 4.1 bis 4.3 |
| `station.py` | Meldet die Station am Rendezvous an, holt eine Nummer und haelt die Verbindung offen — Abschnitte 6.1, 6.2, 6.5 |
| `pruefe-kennung.py` | Faehrt die Kennung gegen die Konformitaetsvektoren, denen auch NereusSDRs Core unterliegt |
| `pruefe-station.py` | Faehrt die Station gegen einen echten, selbst gestarteten Dienst |

```bash
python3 handfunke/fernzugriff/pruefe-kennung.py
python3 handfunke/fernzugriff/pruefe-station.py
```

Beide brauchen `cryptography` und `websockets`. Die Umgebung dafuer liegt
ausserhalb des Quellbaums:

```bash
/opt/homebrew/bin/python3.12 -m venv ~/Longpath/rendezvous-venv
~/Longpath/rendezvous-venv/bin/pip install cryptography websockets pytest pytest-asyncio
~/Longpath/rendezvous-venv/bin/python handfunke/fernzugriff/pruefe-kennung.py
```

| `sitzung.py` | Die WebRTC-Verbindung, die zweite Pruefung und die Bruecke zu Longpaths TCI |
| `geraete.py` | Welche Telefone herein duerfen |
| `geraet.py` | Die Geraeteseite in Python — die Vorlage fuer `../fernzugriff.js` |
| `fuehren.py` | Das Betriebsprogramm: Station fahren, Geraete koppeln und widerrufen |
| `pruefe-rundlauf.py` | Der ganze Weg gegen einen echten Dienst und die TCI-Attrappe |

## Fahren

```bash
# Die Station, dauerhaft:
python3 handfunke/fernzugriff/fuehren.py --dienst wss://rv.example.com/

# Ein Telefon aufnehmen (den Schluessel zeigt die Seite):
python3 handfunke/fernzugriff/fuehren.py --koppeln <schluessel> --name "iPhone"
python3 handfunke/fernzugriff/fuehren.py --zeigen
```

Im Telefon steht dann im Adressfeld nur noch die Stations-Id: 26 Zeichen
aus `a-z` und `2-7`. Beim ersten Mal `dienst#id`, danach merkt die Seite
sich den Dienst.

## Was noch fehlt

1. **Ein Rendezvous im Internet.** Ohne ihn nuetzt der ganze Weg nichts;
   er ist die einzige Voraussetzung, die nicht in diesem Baum liegt.
   `rendezvous/README.md` ist das ganze Rezept (Caddy, coturn, zwei
   systemd-Dienste).
2. **Der WebSocket-Relay** aus Abschnitt 12. Die Station nimmt das
   Angebot entgegen und laesst es liegen. Ohne ihn bleibt der Weg durch
   Netze, die nur Web durchlassen, zu — TURN deckt die meisten anderen.
3. **Die Kopplung per Code** (Station-Link, Abschnitt 3.6) — die Nummer
   mit zwei Woertern, `7-anvil-harbor`, ueber SPAKE2+EE. Erst damit
   braucht ein neues Telefon das Heimnetz nie mehr. Bis dahin wird der
   Schluessel einmal von Hand uebergeben.
4. **Ein eigener Kanal fuer den Ton.** Gemessen am 2026-10-28 gegen die
   Attrappe: in zehn Fenstern zu zwei Sekunden stockte der Ton einmal,
   bei 41 kB/s und 1,5 % Rechenlast an der Station. Es ist also kein
   Engpass beim Rechnen. Der Verdacht liegt auf dem Kanal selbst: er ist
   geordnet und verlaesslich, und damit haelt ein nachgelieferter Rahmen
   alle folgenden auf. Richtig waere ein zweiter Kanal fuer den Ton,
   ungeordnet und ohne Wiederholung — nachgewiesen ist das aber nicht.

## Zwei Dinge, die man wissen muss

**Der Stationsschluessel ist die Station.** Er liegt ausserhalb des
Quellbaums, nur fuer den Benutzer lesbar. Geht er verloren, hat die
Station eine andere Id, und jedes gekoppelte Geraet muss neu gekoppelt
werden. Die Datei wird abgewiesen, wenn andere sie lesen duerfen.

**Die Verbindung muss offen bleiben.** Der Dienst kennt keine Adressen; er
erreicht die Station nur ueber die Verbindung, die sie selbst geoeffnet
hat. Faellt sie weg, ist die Station fuer jedes Telefon „offline" —
auch wenn Longpath laeuft.
