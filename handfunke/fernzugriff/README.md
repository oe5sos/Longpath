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
| `relais.py` | Der WebSocket-Relay (Abschnitt 12), Endenseite: Loopback-Steckdose fuer den ICE-Agenten, ein WebSocket zum Relay |
| `pruefe-tonkanal.py` | Laufzeit der Tonbloecke bei Verlust: ein Kanal gegen eigener Tonkanal, jeweils mit und ohne Verlust |
| `pruefe-relay.py` | Der Relay gegen den echten Dienst und den echten Relay aus `rendezvous/server/`, in einem nachgestellten Netz ohne UDP |

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
2. **Der WebSocket-Relay auf der Telefonseite.** Die Station nimmt das
   Angebot an (`relais.py`, `sitzung.py`), geprueft in `pruefe-relay.py`.
   Eine **Webseite kann den Relay aber nicht benutzen**: der Relay reicht
   rohe ICE-Datagramme durch, und ein Browser erlaubt seiner WebRTC-
   Verbindung weder einen eigenen UDP-Port noch einen fremden Kandidaten.
   Die Spezifikation legt das Anbinden den nativen Apps auf (iPhone-App,
   Aufgabe 29). Fuer die Seite braucht es einen eigenen Weg -- etwa den
   Datenstrom als Nachrichten durch den Relay-WebSocket, mit einer
   eigenen Verschluesselung Ende zu Ende --, das ist nicht gebaut.
3. **Die Kopplung per Code** (Station-Link, Abschnitt 3.6) — die Nummer
   mit zwei Woertern, `7-anvil-harbor`, ueber SPAKE2+EE. **Nicht gebaut,
   bewusst:** es gibt dafuer weder in Python noch im Browser eine Bibliothek
   ohne neue Abhaengigkeit, und ein Eigenbau waere genau die Kryptografie,
   die man nicht selbst schreibt. Die Wahl samt Begruendung steht in
   [`KOPPLUNG-PER-CODE.md`](KOPPLUNG-PER-CODE.md); sie braucht Martins Ja zu
   libsodium und emscripten (nur als Bauwerkzeug). Bis dahin wird einmal
   zu Hause gekoppelt (`koppeln.py`).
4. ~~Ein eigener Kanal fuer den Ton.~~ **Gebaut und belegt.** Der
   Verdacht stimmt, aber nur unter Verlust: auf Loopback stockt nichts
   (der Befund vom 2026-10-28 war der Schub der Tonpumpe). `pruefe-tonkanal.py`
   stellt eine Strecke mit Verlust und Laufzeit nach (alles durch den
   Relay, dort fallen Datagramme heraus): bei 10 % Verlust steigt die
   Laufzeit der Opus-Bloecke auf dem geordneten Kanal im p99 von 28 auf
   etwa 500 ms (Spitze ueber 800 ms), auf dem eigenen Kanal bleibt sie bei
   28 ms; dafuer gehen die verlorenen Bloecke (etwa 10 %) wirklich
   verloren. Die Seite legt den Kanal `longpath-ton` an (ungeordnet,
   `maxRetransmits: 0`); die Station schickt Opus nur dorthin, wenn er offen
   und das Geraet angemeldet ist, sonst bleibt alles wie vorher. Vom
   Telefon wird auf dem Tonkanal nichts angenommen. **JS-Seite nur auf
   Syntax geprueft**, nicht im Browser (siehe Bericht).
## Zwei Dinge, die man wissen muss

**Der Stationsschluessel ist die Station.** Er liegt ausserhalb des
Quellbaums, nur fuer den Benutzer lesbar. Geht er verloren, hat die
Station eine andere Id, und jedes gekoppelte Geraet muss neu gekoppelt
werden. Die Datei wird abgewiesen, wenn andere sie lesen duerfen.

**Die Verbindung muss offen bleiben.** Der Dienst kennt keine Adressen; er
erreicht die Station nur ueber die Verbindung, die sie selbst geoeffnet
hat. Faellt sie weg, ist die Station fuer jedes Telefon „offline" —
auch wenn Longpath laeuft.
