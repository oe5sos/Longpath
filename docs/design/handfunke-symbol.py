#!/usr/bin/env python3
"""Erzeugt die Startbildschirm-Symbole der Handfunke aus dem Longpath-Logo.

Vorgeschichte: hier standen zehn eigene Entwuerfe (Traeger, Boegen,
Rufzeichen, Welle ueber Horizont, langer Weg, Abstimmknopf, S-Meter,
Wasserfall, Monogramm, Yagi), vorgelegt am 2026-10-01. Der Betreiber hat
sie verworfen zugunsten des vorhandenen Programmsymbols: "verwenden wir
das logo wie bei longpath". Das ist auch die richtige Entscheidung — die
Handfunke ist eine Fernbedienung fuer Longpath, kein zweites Programm, und
zwei Symbole fuer eine Sache verwirren nur.

Was dieses Skript trotzdem tun muss: die Vorlage ist fuer macOS gebaut.
Dort zeichnet das System Symbole KLEINER als ihr Feld, mit Luft ringsum,
und die gerundete Kachel ist Teil des Bildes. iOS macht es umgekehrt: es
fuellt das Feld randvoll und rundet selbst. Unveraendert uebernommen
ergaebe das eine zweite Rundung um die erste herum und helle Zipfel in den
Ecken. Darum wird hier auf die Kachel zugeschnitten und ihr Verlauf bis an
den Rand gezogen.

Die Groessen sind die, die iOS wirklich anfordert:
  180  Startbildschirm (3x)     120  Einstellungen / Startbildschirm (2x)
  167  iPad Pro                 152  iPad
   80  Suche (2x)                60  Suche
  512  Manifest (Android/Chrome)
"""

from PIL import Image
import pathlib

WURZEL = pathlib.Path(__file__).resolve().parents[2]
VORLAGE = WURZEL / 'resources/icons/Longpath.iconset/icon_512x512@2x.png'
ZIEL = WURZEL / 'handfunke'
GROESSEN = (180, 167, 152, 120, 80, 60, 512)


def kachel(bild):
    """Schneidet auf die sichtbare Kachel zu und fuellt die durchsichtigen
    Ecken mit ihrem eigenen Verlauf, damit das Ergebnis randfuellend ist."""
    k = bild.crop(bild.getbbox())
    K = max(k.size)
    e = int(K * 0.10)                       # sicher innerhalb der Rundung
    oben = k.getpixel((k.size[0] // 2, e))[:3]
    unten = k.getpixel((k.size[0] // 2, k.size[1] - e))[:3]

    grund = Image.new('RGB', (K, K))
    px = grund.load()
    for y in range(K):
        t = y / (K - 1)
        farbe = tuple(int(oben[i] + (unten[i] - oben[i]) * t) for i in range(3))
        for x in range(K):
            px[x, y] = farbe
    grund.paste(k, ((K - k.size[0]) // 2, (K - k.size[1]) // 2), k)
    return grund


if __name__ == '__main__':
    g = kachel(Image.open(VORLAGE).convert('RGBA'))
    for kante in GROESSEN:
        g.resize((kante, kante), Image.LANCZOS).save(ZIEL / f'symbol-{kante}.png')
    print('geschrieben nach', ZIEL)
