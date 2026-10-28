#!/usr/bin/env python3
"""Die drei Schranken der Koppelstelle pruefen.

    python3 handfunke/fernzugriff/pruefe-koppeln.py

Eine Koppelstelle, die zu viel annimmt, macht die Station offen. Geprueft
wird darum jede Schranke einzeln -- und jede auch von der anderen Seite:
dass sie durchlaesst, was durch soll.
"""

from __future__ import annotations

import json
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

from geraete import Geraeteliste  # noqa: E402
from kennung import b64url, spki_der  # noqa: E402
from koppeln import Koppelstelle, antwort, im_eigenen_netz  # noqa: E402

schlecht = 0


def fehler(z: str) -> None:
    global schlecht
    schlecht += 1
    print("  FEHLER:", z)


def gut(z: str) -> None:
    print("  ok:", z)


def neuer_schluessel() -> str:
    return b64url(spki_der(ec.generate_private_key(ec.SECP256R1()).public_key()))


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        liste = Geraeteliste(Path(tmp) / "geraete.json")
        stelle = Koppelstelle(liste)

        # 1. Ohne Fenster geht nichts.
        e = stelle.nimm(neuer_schluessel(), "A", "phone", "127.0.0.1")
        if e.get("ok"):
            fehler("ohne Fenster wurde ein Geraet aufgenommen")
        else:
            gut("ohne Fenster: abgewiesen")

        # 2. Mit Fenster, aber aus einem fremden Netz.
        stelle.oeffne()
        e = stelle.nimm(neuer_schluessel(), "B", "phone", "8.8.8.8")
        if e.get("ok"):
            fehler("ein Geraet aus einem fremden Netz wurde aufgenommen")
        else:
            gut("fremdes Netz: abgewiesen, auch bei offenem Fenster")
        if len(liste) != 0:
            fehler(f"die Liste hat {len(liste)} Eintraege, erwartet 0")

        # 3. Mit Fenster, aus dem eigenen Netz: muss gehen.
        s = neuer_schluessel()
        e = stelle.nimm(s, "Martins iPhone", "phone", "127.0.0.1")
        if not e.get("ok"):
            fehler(f"das eigene Netz kam nicht durch: {e.get('grund')}")
        else:
            gut(f"eigenes Netz: aufgenommen ({e['ident'][:8]}…)")

        # 4. Ein Schluessel je Fenster.
        e = stelle.nimm(neuer_schluessel(), "C", "phone", "127.0.0.1")
        if e.get("ok"):
            fehler("ein zweites Geraet kam durch dasselbe Fenster")
        else:
            gut("zweites Geraet: abgewiesen, das Fenster war verbraucht")

        # 5. Die Frist.
        stelle = Koppelstelle(liste, fenster_s=0.2)
        stelle.oeffne()
        time.sleep(0.35)
        e = stelle.nimm(neuer_schluessel(), "D", "phone", "127.0.0.1")
        if e.get("ok"):
            fehler("ein abgelaufenes Fenster hat noch aufgenommen")
        else:
            gut("abgelaufenes Fenster: abgewiesen")

        # 6. Kaputte Schluessel.
        stelle = Koppelstelle(liste)
        stelle.oeffne()
        for name, s in (("zu kurz", "AAAA"), ("kein base64url", "!!!"),
                        ("leer", " ")):
            e = stelle.nimm(s, "X", "phone", "127.0.0.1")
            if e.get("ok"):
                fehler(f"ein kaputter Schluessel ({name}) wurde aufgenommen")
        gut("kaputte Schluessel: abgewiesen")
        if stelle.offen is False:
            fehler("ein kaputter Schluessel hat das Fenster verbraucht")
        else:
            gut("das Fenster ueberlebt einen kaputten Schluessel")

        # 7. Die Antwort fuer die Seite.
        kode, rumpf = antwort(stelle, b"{}", "127.0.0.1")
        m = json.loads(rumpf)
        if kode != 200 or "offen" not in m:
            fehler(f"die Zustandsabfrage antwortet {kode}: {rumpf!r}")
        else:
            gut(f"Zustandsabfrage: offen={m['offen']}, noch {m['rest']} s")
        kode, rumpf = antwort(stelle, json.dumps(
            {"schluessel": neuer_schluessel(), "name": "E"}).encode(), "8.8.8.8")
        if kode != 403:
            fehler(f"ein fremdes Netz bekommt {kode} statt 403")
        else:
            gut("ueber die Schnittstelle: fremdes Netz bekommt 403")

        # 8. Dieselbe Antwort fuer „kein Fenster" und „schon genommen".
        a = Koppelstelle(liste).nimm(neuer_schluessel(), "F", "phone", "127.0.0.1")
        b = Koppelstelle(liste)
        b.oeffne()
        b.nimm(neuer_schluessel(), "G", "phone", "127.0.0.1")
        c = b.nimm(neuer_schluessel(), "H", "phone", "127.0.0.1")
        if a.get("grund") != c.get("grund"):
            fehler("'kein Fenster' und 'schon genommen' antworten verschieden "
                   "-- das verraet, wann jemand koppelt")
        else:
            gut("'kein Fenster' und 'schon genommen' sind nicht zu unterscheiden")

    print()
    print("ALLES GRUEN" if schlecht == 0 else f"{schlecht} Abweichungen")
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
