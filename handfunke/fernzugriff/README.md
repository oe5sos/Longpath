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

## Was noch fehlt

1. **Einfuehrungen beantworten** (Abschnitt 6.3) — ohne das kommt kein
   Telefon durch, es sieht nur, dass die Station online ist.
2. **Der Transport.** Eine ICE-Verbindung, und wo die nicht durchkommt,
   der WebSocket-Relay aus Abschnitt 12. Darin dann TCI.
3. **Die Kopplung per Code** (Station-Link, Abschnitt 3.6) — die Nummer
   mit zwei Woertern, `7-anvil-harbor`, ueber SPAKE2+EE. Erst damit
   braucht ein neues Telefon das Heimnetz nie mehr. Bis dahin koppelt man
   wie heute im eigenen Netz.

## Zwei Dinge, die man wissen muss

**Der Stationsschluessel ist die Station.** Er liegt ausserhalb des
Quellbaums, nur fuer den Benutzer lesbar. Geht er verloren, hat die
Station eine andere Id, und jedes gekoppelte Geraet muss neu gekoppelt
werden. Die Datei wird abgewiesen, wenn andere sie lesen duerfen.

**Die Verbindung muss offen bleiben.** Der Dienst kennt keine Adressen; er
erreicht die Station nur ueber die Verbindung, die sie selbst geoeffnet
hat. Faellt sie weg, ist die Station fuer jedes Telefon „offline" —
auch wenn Longpath laeuft.
