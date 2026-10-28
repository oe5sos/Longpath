"""Welche Telefone herein duerfen.

Ein Geraet ist hier: eine Id (der Abdruck seines Schluessels), sein
oeffentlicher Schluessel, ein Name und wann es zuletzt da war. Die Liste
liegt als JSON neben dem Stationsschluessel, ausserhalb des Quellbaums.

**Was eine angenommene Einfuehrung bedeutet, und was nicht.** Sie ist
keine Anmeldung (Abschnitt 4.4, letzter Absatz): der Dienst waehlt die
nonce und sieht jede Unterschrift, und eine Verbindung behaelt ihre nonce
fuer mehrere Einfuehrungen hintereinander. Sie ist nur der Grund, einen
ICE-Versuch auf dieses Geraet zu verwenden. Wer wirklich hereindarf,
entscheidet sich auf der Sitzung darueber ein zweites Mal -- dort mit
einem frischen Wert, den das Geraet unterschreibt (siehe `sitzung.py`).
"""

from __future__ import annotations

import json
import os
import stat
import time
from dataclasses import asdict, dataclass
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import ec

from kennung import (
    einfuehr_transkript,
    geraete_id,
    lade_oeffentlich,
    pruefe_unterschrift,
)


@dataclass
class Geraet:
    ident: str
    schluessel: str  # base64url des SPKI DER, 122 Zeichen
    name: str
    art: str = "phone"  # phone, tablet, computer
    gekoppelt_am: float = 0.0
    zuletzt_da: float = 0.0

    def oeffentlich(self) -> ec.EllipticCurvePublicKey:
        return lade_oeffentlich(self.schluessel)


class Geraeteliste:
    """Die gekoppelten Geraete, als Datei gehalten."""

    def __init__(self, pfad: Path) -> None:
        self.pfad = Path(pfad)
        self._geraete: dict[str, Geraet] = {}
        self._lies()

    # -- Platte -----------------------------------------------------------

    def _lies(self) -> None:
        if not self.pfad.exists():
            return
        roh = json.loads(self.pfad.read_text())
        for eintrag in roh.get("geraete", []):
            g = Geraet(**eintrag)
            self._geraete[g.ident] = g

    def _schreibe(self) -> None:
        self.pfad.parent.mkdir(parents=True, exist_ok=True)
        # Erst daneben schreiben, dann umbenennen: ein Absturz mittendrin
        # darf die Liste nicht halb zurueck lassen.
        neben = self.pfad.with_suffix(self.pfad.suffix + ".neu")
        fd = os.open(neben, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(fd, "w") as f:
            json.dump(
                {"geraete": [asdict(g) for g in self._geraete.values()]},
                f,
                indent=1,
                ensure_ascii=False,
            )
        os.replace(neben, self.pfad)

    # -- Fragen -----------------------------------------------------------

    def __len__(self) -> int:
        return len(self._geraete)

    def __iter__(self):
        return iter(list(self._geraete.values()))

    def hole(self, ident: str) -> Geraet | None:
        return self._geraete.get(ident)

    def ist_gekoppelt(self, ident: str) -> bool:
        return ident in self._geraete

    def pruefe_einfuehrung(
        self, ident: str, station_id: str, nonce: bytes, signatur: str
    ) -> Geraet | None:
        """Darf diese Einfuehrung beantwortet werden? (4.4)

        Gibt das Geraet zurueck oder None. **None heisst schweigen**, nicht
        absagen: die Station antwortet auf eine Einfuehrung, die sie nicht
        annimmt, gar nicht -- sonst verriete sie, welche Geraete gekoppelt
        sind (Abschnitt 6.4).
        """
        geraet = self._geraete.get(ident)
        if geraet is None:
            return None
        try:
            oeff = geraet.oeffentlich()
        except ValueError:
            return None
        if not pruefe_unterschrift(oeff, einfuehr_transkript(station_id, nonce), signatur):
            return None
        return geraet

    # -- Aendern ----------------------------------------------------------

    def koppele(self, schluessel: str, name: str, art: str = "phone") -> Geraet:
        """Ein Geraet aufnehmen. Ein schon bekanntes behaelt seine Daten."""
        oeff = lade_oeffentlich(schluessel)
        ident = geraete_id(oeff)
        vorhanden = self._geraete.get(ident)
        if vorhanden is not None:
            # Beim Bestaetigen eines bekannten Schluessels bleiben Name und
            # Datum stehen (Station-Link 3.6: "the existing device record
            # is preserved").
            return vorhanden
        g = Geraet(
            ident=ident,
            schluessel=schluessel,
            name=name or "Telefon",
            art=art,
            gekoppelt_am=time.time(),
        )
        self._geraete[ident] = g
        self._schreibe()
        return g

    def gesehen(self, ident: str) -> None:
        g = self._geraete.get(ident)
        if g is None:
            return
        g.zuletzt_da = time.time()
        self._schreibe()

    def widerrufe(self, ident: str) -> bool:
        if ident not in self._geraete:
            return False
        del self._geraete[ident]
        self._schreibe()
        return True
