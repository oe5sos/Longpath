# Der Vermittlungsserver für longpath.at

Das ist die einzige Voraussetzung des Fernzugriffs, die nicht im Repo
liegt. Ohne ihn finden Telefon und Station einander nicht — er stellt
vor, und wo der direkte Weg zu ist, trägt er den Ton.

Das allgemeine Rezept steht in `rendezvous/README.md` (aus NereusSDR
übernommen). Dieses Blatt ist dasselbe, nur mit unseren Namen und den
zwei Stellen, an denen unsere Lage von der dortigen abweicht.

## Was zu mieten ist

Ein kleiner eigener Server, nichts Besonderes:

| | |
| --- | --- |
| Betriebssystem | **Ubuntu 24.04** |
| Adressen | eine öffentliche **IPv4 und eine IPv6**, beide direkt auf der Netzwerkkarte |
| Arbeitsspeicher | 2 GB (1 GB täte es für uns zwei auch) |
| Zugang | SSH als `root` |
| Datenmenge | 1 TB im Monat reicht weit — eine Stunde Fernbetrieb über den Relay kostet rund 160 MB, und nur wenn der direkte Weg zu ist |

**Was nicht geht:** ein Anbieter, der die IPv4 hinter seinem eigenen NAT
versteckt. Der Relay braucht die Adresse, die auch draußen gilt. Wer
"dedizierte IPv4" sagt, meint das Richtige.

**Vorschlag:** Hetzner Cloud **CX22** in Nürnberg oder Falkenstein, rund
4,50 € im Monat. Von Oberösterreich sind das etwa 20 ms, es gibt IPv4
und IPv6 ohne NAT, und 20 TB Datenmenge sind dabei. Netcup oder
DigitalOcean tun es genauso; nur die drei Punkte oben müssen stimmen.

Beim Anlegen diesen Schlüssel als SSH-Schlüssel hinterlegen — er liegt
schon auf dem MacBook (`~/.ssh/id_ed25519_rendezvous`):

```
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIM3I8PQdqfkLdsEWiLob7IGrAnAole1FOGxqROttv1Vf longpath-rendezvous deploy
```

## Die vier DNS-Einträge

Bei **Cloudflare**, in der Zone `longpath.at`:

| Typ | Name | Wert |
| --- | --- | --- |
| A | `rv` | die IPv4 des Servers |
| AAAA | `rv` | die IPv6 des Servers |
| A | `rv4` | dieselbe IPv4 |
| AAAA | `rv6` | dieselbe IPv6 |

Dass `rv4` kein AAAA bekommt und `rv6` kein A, ist Absicht und kein
Versehen: der Relay braucht beide Adressfamilien getrennt ansprechbar,
weil ein Telefon in einem reinen IPv6-Mobilfunknetz einen IPv6-Relay
braucht und eines in einem alten Netz einen IPv4-Relay.

### Die Falle: alle vier auf „DNS only"

Die graue Wolke, **nicht** die orange. Unsere Website läuft über
Cloudflares Proxy, und für eine Website ist das richtig — hier wäre es
falsch, gleich dreifach:

* Caddy holt sein Zertifikat bei Let's Encrypt selbst, über Port 80.
  Hinter dem Proxy kommt die Prüfung nie beim Server an.
* STUN und TURN sind **UDP**. Cloudflares Proxy trägt kein UDP; die
  Namen `rv4` und `rv6` müssen auf den Server selbst zeigen.
* Der Weg von draußen lebt davon, dass beide Enden ihre wirklichen
  Adressen austauschen. Ein Proxy davor verdeckt genau das.

## Dann übernehme ich

Sobald der Server läuft und die Einträge gesetzt sind, brauche ich nur
die IPv4 — alles Weitere steht fertig im Repo und läuft von hier aus:

1. `rendezvous/` auf den Server kopieren,
2. `deploy/setup-server.sh` (installiert Caddy und coturn, legt die
   Geheimnisse an, schreibt alle Konfigurationen, richtet die Dienste
   ein) — erst mit `--dry-run`, dann echt,
3. `rendezvous/deploy.sh` veröffentlicht den Dienst,
4. prüfen: die beiden `curl`-Abfragen müssen `426` mit dem
   Begrüßungstext liefern, und `turnutils_stunclient` muss über beide
   Adressfamilien antworten.

Die Firewall ist der einzige Schritt, der von Hand kommt, und er gehört
an den Anfang — **SSH zuerst freigeben**, sonst sperrt man sich aus:

```sh
ufw allow OpenSSH
ufw allow 80/tcp
ufw allow 443/tcp
ufw allow 3478/udp
ufw allow 443/udp
ufw allow 61000:65535/udp
ufw enable
```

## Die Seite gleich mit ausliefern

Der Server sollte auch die App selbst ausliefern, unter `/app/`. Das
löst drei Dinge auf einmal, die sonst einzeln im Weg stehen:

* **Von draußen erreichbar.** Heute kommt die Seite von
  `172.30.30.x` — von unterwegs also gar nicht.
* **Sicherer Kontext.** Der Schlüsselspeicher des Browsers
  (`crypto.subtle`) gibt es nur über https. Ohne ihn kein
  Geräteschlüssel, also **kein Fernzugriff** — und auch kein
  Mikrofon, kein AudioWorklet und kein Opus-Dekoder. Am Telefon
  gemessen: `sicher=false` in jeder Selbstmeldung von 172.30.30.x.
* **Keine Einstellung, die man vergessen kann.** Kommt die Seite vom
  Vermittler, nimmt sie ihn automatisch als Dienst (`dienstVomOrt()`
  in `app.js`) — im Telefon steht dann nur noch die Stations-Id.

Die Seite ist statisch; es genügt, den Ordner hinzulegen. In den
`Caddyfile`-Block des Dienstes, VOR dessen `handle`-Blöcken:

```
handle_path /app/* {
    root * /var/www/longpath-app
    file_server
    header Cache-Control "no-cache"
}
```

Der Pfad muss `/app/` sein und nicht `/`: dort liegt der Rendezvous
selbst, und WebSockets lassen sich in Caddy nicht sauber heraussortieren
(das Rezept sagt warum — Apples Client schreibt `Upgrade: WebSocket`,
und Caddy vergleicht Kopfzeilen genau).

Hinlegen, von hier aus:

```bash
rsync -a --delete --exclude 'fernzugriff/' --exclude '*.py' --exclude 'pruefe-*' \
      handfunke/ root@rv.longpath.at:/var/www/longpath-app/
```

Die `.py`-Dateien bleiben hier: die Attrappe, der Seitenserver und der
Vermittler gehören auf den Mac im Shack, nicht auf einen Server im
Internet.

## Was danach im Telefon steht

Nur noch die Stations-Id, 26 Zeichen — mehr nicht, wenn die Seite vom
Server kommt. Kommt sie weiter aus dem Heimnetz, beim allerersten Mal
`rv.longpath.at#<id>`, damit die Seite sich den Dienst merkt.
