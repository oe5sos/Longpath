#!/usr/bin/env python3
"""Die Kennung gegen die Konformitaetsvektoren fahren.

Die Vektoren liegen in `rendezvous/conformance/v1/crypto/` und sind die
Pruefung, der auch NereusSDRs Core und ihre iPhone-App unterliegen. Wer
hier gruen ist, rechnet dieselbe Id und unterschreibt dasselbe Transkript.

    python3 handfunke/fernzugriff/pruefe-kennung.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from kennung import (  # noqa: E402
    b64url,
    lade_oeffentlich,
    pruefe_unterschrift,
    registrier_transkript,
    rendezvous_id,
    spki_der,
    unb64url,
)

WURZEL = Path(__file__).resolve().parents[2]
VEKTOREN = WURZEL / "rendezvous" / "conformance" / "v1" / "crypto"


def fehler(zeile: str) -> None:
    print("  FEHLER:", zeile)


def pruefe_id() -> int:
    d = json.loads((VEKTOREN / "rendezvous-id.json").read_text())
    schlecht = 0
    for i, fall in enumerate(d["cases"]):
        oeff = lade_oeffentlich(fall["publicKey"])
        # Der Vektor nennt auch den Abdruck; beide Schritte einzeln pruefen,
        # damit ein Fehler sagt, WO er sitzt.
        import hashlib

        from kennung import PRAEFIX_ID

        abdruck = hashlib.sha256(PRAEFIX_ID + spki_der(oeff)).hexdigest()
        if abdruck != fall["digestHex"]:
            fehler(f"Fall {i}: Abdruck {abdruck} statt {fall['digestHex']}")
            schlecht += 1
            continue
        hier = rendezvous_id(oeff)
        if hier != fall["id"]:
            fehler(f"Fall {i}: Id {hier} statt {fall['id']}")
            schlecht += 1
    print(f"rendezvous-id: {len(d['cases'])} Faelle, {schlecht} falsch")
    return schlecht


def pruefe_praefix() -> int:
    """Die Praefixe selbst -- ein Tippfehler dort faellt sonst erst live auf."""
    from kennung import PRAEFIX_ID, PRAEFIX_REGISTER

    schlecht = 0
    d = json.loads((VEKTOREN / "rendezvous-id.json").read_text())
    if PRAEFIX_ID.hex() != d["prefixHex"]:
        fehler(f"Id-Praefix {PRAEFIX_ID.hex()} statt {d['prefixHex']}")
        schlecht += 1
    d = json.loads((VEKTOREN / "register-proof.json").read_text())
    erwartet = bytes.fromhex(d["transcriptHex"])
    nonce = unb64url(d["nonce"])
    hier = registrier_transkript(nonce)
    if hier != erwartet:
        fehler(f"Transkript {hier.hex()} statt {erwartet.hex()}")
        schlecht += 1
    if not PRAEFIX_REGISTER.endswith(b"\n"):
        fehler("Registrier-Praefix endet nicht auf Zeilenvorschub")
        schlecht += 1
    print(f"Praefixe und Transkript: {schlecht} falsch")
    return schlecht


def pruefe_unterschriften() -> int:
    """Die Faelle aus register-proof.json, Schluessel und Unterschrift zusammen.

    Jeder Fall bringt seinen EIGENEN `publicKey` mit -- vier der Faelle
    pruefen nicht die Unterschrift, sondern den Schluessel (komprimiert,
    andere Kurve, Punkt nicht auf der Kurve, nicht strenges base64url).
    `valid` ist darum erst wahr, wenn beides traegt, genau in dieser
    Reihenfolge: der Dienst prueft erst den Schluessel (Abschnitt 6.2),
    dann die Unterschrift.
    """
    d = json.loads((VEKTOREN / "register-proof.json").read_text())
    transkript = bytes.fromhex(d["transcriptHex"])
    schlecht = 0
    faelle = d.get("signatures") or d.get("cases") or []
    for i, fall in enumerate(faelle):
        erwartet = bool(fall["valid"])
        try:
            oeff = lade_oeffentlich(fall["publicKey"])
        except ValueError:
            hier = False
        else:
            hier = pruefe_unterschrift(oeff, transkript, fall["signature"])
        if hier != erwartet:
            fehler(
                f"Fall {i} ({fall.get('name', '?')}, "
                f"{fall.get('why', 'gueltig')}): {hier} statt {erwartet}"
            )
            schlecht += 1
    print(f"register-proof: {len(faelle)} Faelle, {schlecht} falsch")
    return schlecht


def pruefe_einfuehrung() -> int:
    """introduce-signature.json: die Unterschrift, mit der ein Geraet anklopft.

    Faellt die Haelfte des Weges von aussen: wer hier zu grosszuegig ist,
    laesst jeden herein, der eine fremde Einfuehrung mitgehoert hat.
    """
    from kennung import PRAEFIX_INTRODUCE, einfuehr_transkript, geraete_id

    d = json.loads((VEKTOREN / "introduce-signature.json").read_text())
    schlecht = 0

    if len(PRAEFIX_INTRODUCE) != 23:
        fehler(f"Praefix ist {len(PRAEFIX_INTRODUCE)} Byte lang, erwartet sind 23")
        schlecht += 1

    # Das Transkript, falls der Vektor es nennt.
    if "transcriptHex" in d:
        erwartet = bytes.fromhex(d["transcriptHex"])
        hier = einfuehr_transkript(d["stationId"], unb64url(d["nonce"]))
        if hier != erwartet:
            fehler(f"Transkript {hier.hex()} statt {erwartet.hex()}")
            schlecht += 1

    # Die Geraete-Id steht ohne Praefix -- eine haeufige Verwechslung.
    if "device" in d and "id" in d["device"]:
        oeff = lade_oeffentlich(d["device"]["publicKey"])
        hier = geraete_id(oeff)
        if hier != d["device"]["id"]:
            fehler(f"Geraete-Id {hier} statt {d['device']['id']}")
            schlecht += 1

    faelle = d.get("signatures") or d.get("cases") or []
    for i, fall in enumerate(faelle):
        erwartet_gut = bool(fall["valid"])
        schluessel = fall.get("publicKey", d["device"]["publicKey"])
        station = fall.get("stationId", d.get("stationId"))
        nonce_text = fall.get("nonce", d.get("nonce"))
        try:
            oeff = lade_oeffentlich(schluessel)
            transkript = einfuehr_transkript(station, unb64url(nonce_text))
        except ValueError:
            hier = False
        else:
            hier = pruefe_unterschrift(oeff, transkript, fall["signature"])
        if hier != erwartet_gut:
            fehler(
                f"Fall {i} ({fall.get('name', '?')}, "
                f"{fall.get('why', 'gueltig')}): {hier} statt {erwartet_gut}"
            )
            schlecht += 1
    print(f"introduce-signature: {len(faelle)} Faelle, {schlecht} falsch")
    return schlecht


def pruefe_rundlauf() -> int:
    """Ein eigener Schluessel, selbst unterschrieben, selbst geprueft.

    Faengt die Haelfte, die die Vektoren nicht decken: sie enthalten nur
    fremde Unterschriften, nie eine von uns.
    """
    from cryptography.hazmat.primitives.asymmetric import ec

    from kennung import unterschreibe

    schlecht = 0
    privat = ec.generate_private_key(ec.SECP256R1())
    oeff = privat.public_key()
    transkript = registrier_transkript(b"\x01" * 32)
    sig = unterschreibe(privat, transkript)
    if len(sig) != 86:
        fehler(f"eigene Unterschrift ist {len(sig)} Zeichen lang, erwartet 86")
        schlecht += 1
    if not pruefe_unterschrift(oeff, transkript, sig):
        fehler("eigene Unterschrift verifiziert nicht")
        schlecht += 1
    # Ein anderes Transkript darf NICHT verifizieren.
    if pruefe_unterschrift(oeff, registrier_transkript(b"\x02" * 32), sig):
        fehler("Unterschrift verifiziert ueber ein fremdes Transkript")
        schlecht += 1
    ident = rendezvous_id(oeff)
    if len(ident) != 26 or not all(c in "abcdefghijklmnopqrstuvwxyz234567" for c in ident):
        fehler(f"eigene Id passt nicht auf [a-z2-7]{{26}}: {ident}")
        schlecht += 1
    if b64url(b"") != "":
        fehler("b64url der leeren Folge ist nicht leer")
        schlecht += 1
    print(f"Rundlauf mit eigenem Schluessel: {schlecht} falsch")
    return schlecht


def main() -> int:
    if not VEKTOREN.is_dir():
        print(f"Vektoren fehlen: {VEKTOREN}")
        return 2
    schlecht = (
        pruefe_praefix()
        + pruefe_id()
        + pruefe_unterschriften()
        + pruefe_einfuehrung()
        + pruefe_rundlauf()
    )
    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
