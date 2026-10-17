#!/usr/bin/env bash
# tls-einrichten.sh — gibt der Handfunke ein Zertifikat, damit das Telefon
# sie als sicheren Kontext ansieht.
#
# Warum das sein muss, und zwar bevor ans Senden zu denken ist:
# `getUserMedia` — der einzige Weg an ein Mikrofon im Browser — ist
# **[SecureContext]**. Über `http` auf einer LAN-Adresse ist
# `navigator.mediaDevices` schlicht `undefined`. Das ist keine Vermutung:
# Martins Telefon meldet es seit dem ersten Tag selbst, in jeder Zeile von
# ~/Library/Logs/handfunke-server.log —
#
#     172.30.30.115 … sicher=false … weg=scriptprocessor
#     127.0.0.1     … sicher=true
#
# — und niemand hat es gelesen, weil am Mac (127.0.0.1 gilt als sicher)
# alles ging. Derselbe Schalter kostet nebenbei das `AudioWorklet`: der
# Empfangston läuft am Telefon bis heute über den ScriptProcessor im
# Hauptfaden, also genau dort, wo auch der Wasserfall gezeichnet wird.
#
# Ein Zertifikat behebt beides auf einmal.
#
# Gebaut wird eine kleine eigene Zertifizierungsstelle und darunter ein
# Serverzertifikat. Zwei Stufen statt einer, weil nur so das Telefon ein
# einziges Mal etwas einrichten muss: die Stelle wird dort eingerichtet und
# als vertrauenswürdig markiert, das Serverzertifikat darf danach jederzeit
# neu ausgestellt werden — bei neuer Adresse, nach Ablauf — ohne dass am
# Telefon noch einmal jemand etwas antippt.
#
#     ./tls-einrichten.sh            # anlegen oder Serverzertifikat erneuern
#     ./tls-einrichten.sh --neu-ca   # auch die Stelle neu (Telefon muss neu vertrauen)
#
# Die Schlüssel liegen bewusst AUSSERHALB des Quellbaums, unter
# ~/Longpath/werkzeug/handfunke-tls. Ein privater Schlüssel in einem
# Git-Baum ist ein privater Schlüssel auf dem Weg nach GitHub.

set -euo pipefail

ORDNER="${HANDFUNKE_TLS_DIR:-$HOME/Longpath/werkzeug/handfunke-tls}"
TAGE_CA=3650
TAGE_SERVER=365     # Apple deckelt Serverzertifikate; 365 bleibt sicher darunter

NEU_CA=0
[ "${1:-}" = "--neu-ca" ] && NEU_CA=1

mkdir -p "$ORDNER"
chmod 700 "$ORDNER"
cd "$ORDNER"

# ── Namen und Adressen, unter denen die Seite erreichbar ist ────────────────
#
# Der .local-Name ist der stabile davon: er folgt dem Rechner, auch wenn der
# Router eine andere Adresse vergibt. Die aktuellen Adressen kommen trotzdem
# mit hinein, damit ein Aufruf per Zahl nicht an einer Zertifikatswarnung
# scheitert — iOS verlangt den Namen im SAN, der CN allein zählt dort seit
# Jahren nicht mehr.
NAME="$(scutil --get LocalHostName 2>/dev/null || hostname -s)"
ALT="DNS:${NAME}.local, DNS:${NAME}, DNS:localhost, IP:127.0.0.1, IP:::1"
for adr in $(ipconfig getifaddr en0 2>/dev/null || true) \
           $(ipconfig getifaddr en1 2>/dev/null || true); do
  ALT="${ALT}, IP:${adr}"
done
echo "Namen im Zertifikat: ${ALT}"

# ── Die Zertifizierungsstelle ──────────────────────────────────────────────
if [ ! -f ca.crt ] || [ "$NEU_CA" = 1 ]; then
  echo "Zertifizierungsstelle wird angelegt …"
  openssl req -x509 -newkey rsa:2048 -sha256 -days "$TAGE_CA" -nodes \
    -keyout ca.key -out ca.crt \
    -subj "/CN=Longpath Handfunke CA/O=Longpath" \
    -addext "basicConstraints=critical,CA:TRUE,pathlen:0" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
  chmod 600 ca.key
  echo "  → ca.crt angelegt. Das Telefon muss ihr EINMAL vertrauen (siehe unten)."
else
  echo "Zertifizierungsstelle besteht bereits (ca.crt) — bleibt unverändert."
fi

# ── Das Serverzertifikat ───────────────────────────────────────────────────
echo "Serverzertifikat wird ausgestellt …"
openssl req -newkey rsa:2048 -sha256 -nodes \
  -keyout server.key -out server.csr \
  -subj "/CN=${NAME}.local/O=Longpath" 2>/dev/null
chmod 600 server.key

cat > server.ext <<EXT
basicConstraints=CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=${ALT}
EXT

openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days "$TAGE_SERVER" -sha256 -extfile server.ext 2>/dev/null
rm -f server.csr server.ext

# Die Kette, wie ein Server sie ausliefern soll: erst das eigene, dann die
# Stelle. Ohne das muss jeder Client die Stelle schon kennen — das Telefon
# kennt sie, ein Besucher am Mac aber nicht.
cat server.crt ca.crt > server-kette.crt

echo
openssl x509 -in server.crt -noout -subject -enddate -ext subjectAltName | sed 's/^/  /'
echo
cat <<ENDE
Fertig. Die Dateien liegen in:
  $ORDNER
    ca.crt            → auf das Telefon, einmalig
    server-kette.crt  → der Server liefert sie aus
    server.key        → bleibt hier, geht nirgends hin

Weiter:
  1. Dienste mit Zertifikat starten (der LaunchAgent nimmt es von selbst,
     sobald die Dateien da sind — siehe handfunke/README.md).
  2. Am Telefon http://${NAME}.local:8771/ca.crt aufrufen und das Profil
     installieren (Einstellungen > Profil geladen).
  3. **Nicht vergessen**, sonst war alles umsonst:
     Einstellungen > Allgemein > Info > Zertifikatsvertrauenseinstellungen
     > "Longpath Handfunke CA" einschalten.
  4. Die Seite künftig über https://${NAME}.local:8772/ aufrufen.
     Die Prüfseite kann-das-telefon.html muss dann isSecureContext = ja
     zeigen — erst dann gibt es überhaupt ein Mikrofon.
ENDE
