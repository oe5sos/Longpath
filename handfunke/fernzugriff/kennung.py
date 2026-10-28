"""Die Stationskennung am Rendezvous: Schluessel, Id und Registrierungsnachweis.

Massgeblich ist `docs/architecture/2026-09-23-rendezvous-v1.md`, Abschnitte
4.1 bis 4.3 (uebernommen aus NereusSDR, siehe
docs/attribution/NEREUS-RENDEZVOUS-PROVENANCE.md). Wo dieses Modul und das
Dokument sich widersprechen, hat das Dokument recht.

Drei Dinge stehen hier und sonst nirgends:

* der **Stationsschluessel** -- ein P-256-Schluessel, der die Station ist.
  Er liegt ausserhalb des Quellbaums (wie die TLS-Schluessel der Seite,
  siehe handfunke/README.md) und ueberlebt jeden Neustart. Geht er
  verloren, ist die Station eine andere: jedes gekoppelte Geraet muss neu
  gekoppelt werden.
* die **Rendezvous-Id**, aus dem Schluessel gerechnet, unter der die
  Station erreichbar ist.
* der **Registrierungsnachweis**, mit dem die Station dem Dienst zeigt,
  dass ihr der Schluessel gehoert.
"""

from __future__ import annotations

import base64
import hashlib
import os
import stat
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils as asym_utils

# Die Praefixe sind Teil des Protokolls, nicht unsere Wahl: sie stehen in
# den Konformitaetsvektoren und muessen Byte fuer Byte stimmen, sonst
# weist der Dienst die Registrierung ab.
PRAEFIX_ID = b"NereusSDR rendezvous id v1\n"
PRAEFIX_REGISTER = b"NereusSDR rendezvous register v1\n"

# RFC 4648 base32, Standardalphabet, klein, ohne Fuellzeichen; die ersten
# 26 Zeichen (Abschnitt 4.2).
ID_LAENGE = 26


def b64url(roh: bytes) -> str:
    """base64url ohne Fuellzeichen -- die einzige Kodierung auf dieser Leitung."""
    return base64.urlsafe_b64encode(roh).decode("ascii").rstrip("=")


def unb64url(text: str) -> bytes:
    """Gegenstueck zu b64url. Wirft bei allem, was nicht streng passt.

    Streng heisst (Abschnitt 4.1): nur `A-Z a-z 0-9 - _`, keine
    Fuellzeichen, eine Laenge, die nicht 1 mod 4 ist, und die unbenutzten
    niederwertigen Bits des letzten Zeichens null. Die letzte Bedingung
    faengt man am sichersten, indem man das Ergebnis zurueckkodiert und
    vergleicht -- `b64decode` selbst nimmt `...gw` und `...gx` gleich an.
    """
    if len(text) % 4 == 1:
        raise ValueError("keine gueltige base64url-Laenge")
    fehlend = (-len(text)) % 4
    try:
        roh = base64.urlsafe_b64decode(text + "=" * fehlend)
    except Exception as e:  # binascii.Error und Verwandte
        raise ValueError(f"keine gueltige base64url: {e}") from e
    if b64url(roh) != text:
        raise ValueError("base64url ist nicht kanonisch")
    return roh


def spki_der(schluessel: ec.EllipticCurvePublicKey) -> bytes:
    """Die kanonischen 91 Byte SubjectPublicKeyInfo mit unkomprimiertem Punkt."""
    der = schluessel.public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    if len(der) != 91:
        raise ValueError(f"SPKI ist {len(der)} Byte lang, erwartet sind 91")
    return der


def rendezvous_id(oeffentlich: ec.EllipticCurvePublicKey) -> str:
    """Die Id, unter der die Station registriert und erreichbar ist (4.2)."""
    abdruck = hashlib.sha256(PRAEFIX_ID + spki_der(oeffentlich)).digest()
    return base64.b32encode(abdruck).decode("ascii").lower()[:ID_LAENGE]


def registrier_transkript(nonce: bytes) -> bytes:
    """Was die Station unterschreibt (4.3). `nonce` sind die 32 ROHEN Byte."""
    if len(nonce) != 32:
        raise ValueError(f"nonce ist {len(nonce)} Byte lang, erwartet sind 32")
    return PRAEFIX_REGISTER + nonce


def unterschreibe(privat: ec.EllipticCurvePrivateKey, transkript: bytes) -> str:
    """ECDSA P-256 ueber SHA-256, roh r||s (64 Byte), base64url.

    `cryptography` liefert DER; das Protokoll will die rohe Form, darum
    der Umweg ueber decode_dss_signature.
    """
    der = privat.sign(transkript, ec.ECDSA(hashes.SHA256()))
    r, s = asym_utils.decode_dss_signature(der)
    return b64url(r.to_bytes(32, "big") + s.to_bytes(32, "big"))


def pruefe_unterschrift(
    oeffentlich: ec.EllipticCurvePublicKey, transkript: bytes, signatur: str
) -> bool:
    """Gegenstueck zu unterschreibe -- fuer die Vektoren und fuer Geraete."""
    try:
        roh = unb64url(signatur)
    except ValueError:
        return False
    if len(roh) != 64:
        return False
    r = int.from_bytes(roh[:32], "big")
    s = int.from_bytes(roh[32:], "big")
    if r == 0 or s == 0:
        return False
    der = asym_utils.encode_dss_signature(r, s)
    try:
        oeffentlich.verify(der, transkript, ec.ECDSA(hashes.SHA256()))
    except Exception:
        return False
    return True


def lade_schluessel(pfad: Path) -> ec.EllipticCurvePrivateKey:
    """Den Stationsschluessel lesen, und beim ersten Mal anlegen.

    Die Datei gehoert nur dem Benutzer (0600). Ein Schluessel, den jemand
    anders lesen kann, ist die Station -- darum wird eine Datei mit
    weiteren Rechten abgewiesen statt stillschweigend benutzt.
    """
    pfad = Path(pfad)
    if pfad.exists():
        modus = stat.S_IMODE(pfad.stat().st_mode)
        if modus & 0o077:
            raise PermissionError(
                f"{pfad} ist fuer andere lesbar (Modus {modus:o}); "
                "chmod 600, oder einen neuen Schluessel anlegen"
            )
        privat = serialization.load_pem_private_key(pfad.read_bytes(), password=None)
        if not isinstance(privat, ec.EllipticCurvePrivateKey):
            raise ValueError(f"{pfad} haelt keinen EC-Schluessel")
        if not isinstance(privat.curve, ec.SECP256R1):
            raise ValueError(f"{pfad} haelt keinen P-256-Schluessel")
        return privat

    privat = ec.generate_private_key(ec.SECP256R1())
    pfad.parent.mkdir(parents=True, exist_ok=True)
    # Erst mit 0600 anlegen, dann schreiben: zwischen open und chmod darf
    # die Datei nie offen dastehen.
    fd = os.open(pfad, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as f:
        f.write(
            privat.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption(),
            )
        )
    return privat


def lade_oeffentlich(text: str) -> ec.EllipticCurvePublicKey:
    """Einen oeffentlichen Schluessel von der Leitung lesen (122 Zeichen)."""
    der = unb64url(text)
    if len(der) != 91:
        raise ValueError(f"SPKI ist {len(der)} Byte lang, erwartet sind 91")
    schluessel = serialization.load_der_public_key(der)
    if not isinstance(schluessel, ec.EllipticCurvePublicKey):
        raise ValueError("kein EC-Schluessel")
    if not isinstance(schluessel.curve, ec.SECP256R1):
        raise ValueError("kein P-256-Schluessel")
    # Kanonisch heisst: er kodiert zurueck auf dieselben 91 Byte.
    if spki_der(schluessel) != der:
        raise ValueError("SPKI ist nicht kanonisch")
    return schluessel
