# CAT: zwei Fassungen, zwei Zweige — Stand 2026-10-12

In der Nacht vom 11. auf den 12. Oktober ist der CAT-Server **zweimal**
gebaut worden, in zwei Sitzungen, die voneinander nichts wussten. Keine
der beiden ist in `main`. Dieses Blatt sagt, was jede kann, wo sie
kollidieren und was zu tun ist.

## Die zwei Fassungen

| | `feat/fm-relaisbetrieb` (Worktree `worktrees/rx-eq`) | `fix/discovery-quietpolls-thetis-parity` |
| --- | --- | --- |
| Protokollschicht | `CatProtocol.{h,cpp}` + `CatModeMap.h` | `CatParser.{h,cpp}` |
| Transport | `CatServer.{h,cpp}`, **vier Kanäle** (0..3) wie bei Thetis | `CatServer.{h,cpp}`, **ein** Kanal |
| Befehle | 10: `AI FA FB FR FT ID IF MD RX TX` | 16: dieselben plus `PS SM RT XT RC RU RD` |
| Oberfläche | `CatApplet` verdrahtet (TCP-LEDs je Kanal, Platz für PTY und VAX) | Setup-Seite *CAT & Network → TCP/IP CAT*, zwei Gruppen |
| Sendetor | — | eigener Schalter, Standard zu, `RX;` immer erlaubt |
| Rücklesen nach dem Setzen | — | ja, für Frequenz, Modus, RIT/XIT |
| rigctld | — | `RigctldServer`, eigener Port, eigenes Sendetor |
| Abbau-Wächter in `~MainWindow` | ja (fünfte Stelle derselben Fehlerform) | — |
| Livelauf | am laufenden Programm bestätigt | gegen Rohclient **und** hamlibs TS-2000-Treiber |
| Prüfstände | `tst_cat_protokoll`, `tst_cat_server`, `tst_cat_verdrahtung` | `tst_cat_server`, `tst_rigctld_server`, `tst_cat_page_render` |

## Wo es wirklich kollidiert

Beide Zweige legen **`src/core/CatServer.{h,cpp}`** und
**`tests/tst_cat_server.cpp}`** an. Ein Merge beider Zweige in `main`
gibt dort harte Konflikte; alles andere lässt sich nebeneinander legen
(die Protokollschichten heißen verschieden, `RigctldServer` und
`CatApplet` berühren sich nicht).

## Was zusammengenommen das Beste ist

Die beiden Hälften ergänzen sich fast lückenlos:

* Vom Relais-Zweig: **der Transport** (vier Kanäle, Applet-Verdrahtung,
  der vorbereitete Platz für PTY und VAX) und der Abbau-Wächter.
* Vom Parity-Zweig: **die Protokolltiefe** (sechs Befehle mehr, darunter
  die ganze RIT/XIT-Familie als Split-Ersatz), das Rücklesen nach jedem
  Setzen, das Sendetor und die rigctld-Brücke.

Billiger Weg: die sechs zusätzlichen Befehle, das Rücklesen und das
Sendetor in `CatProtocol` nachtragen, `CatParser` fallen lassen, den
Vier-Kanal-`CatServer` behalten, `RigctldServer` und die Setup-Seite
unverändert mitnehmen.

## Was in dieser Nacht NICHT getan wurde

Die Zusammenführung. Zwei gleichnamige Klassen über zwei Zweige zu
verschmelzen ist Handarbeit mit Folgen für die Oberfläche, und keine der
beiden Fassungen ist dringend: beide liegen auf ihrem Zweig, `main` hat
weiter kein CAT. Die Entscheidung, welche Struktur bleibt, gehört dem
Betreiber — dieses Blatt soll sie auf fünf Minuten verkürzen.

## Wie es dazu kam

Zwei Sitzungen im selben Repo, beide nach demselben Befund ("Longpath
spricht kein CAT"), beide nach Thetis als Quelle, beide in derselben
Nacht. Die Notiz `two-sessions-same-worktree` im Gedächtnis warnt vor
fremden uncommitteten Änderungen — sie warnt nicht davor, dass zwei
Sitzungen dieselbe Funktion bauen. Dafür hilft nur, vor einem neuen
Baustein einmal `git branch -a` und die Zweige der letzten Tage
anzusehen.
