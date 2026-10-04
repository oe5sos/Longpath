# Zwei Minuten Mitschnitt — die Anleitung

**Wofür:** drei Fragen, an denen die QRP-Arbeit sonst stehen bleibt
(Stand 2026-10-03, siehe `docs/architecture/2026-10-02-sunsdr-paritaet.md`):

1. Welcher Rahmen schaltet den **zweiten Empfänger** ein? Der Platz wird
   akzeptiert und quittiert, bleibt aber stumm.
2. Welcher Rahmen stellt die **Abtastrate**? `0x18` tut es nicht,
   obwohl er den Haupttakt trägt.
3. Welche Rahmen gehören überhaupt noch zum Verbindungsablauf? Die
   dreizehn bekannten sind unvollständig; gemessen wurden „rund zwei
   Dutzend".

**Was dabei nicht gebraucht wird:** keine Antenne, kein Senden, keine
Freigabe für HF. Es wird nur zugehört.

## Vorbereitung

Longpath beenden, damit es dem anderen Programm nicht im Weg steht. Die
QRP hängt an `en9` (eigenes Kabel, 192.168.16.200) — die Schnittstelle
steht im Befehl unten, bei einem anderen Netzweg entsprechend ändern.

## Durchgang A: Verbinden

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-A.pcap host 192.168.16.200
```

Läuft. Jetzt **ExpertSDR2 starten und verbinden lassen**, zehn Sekunden
warten, ExpertSDR2 beenden. Dann `tcpdump` mit **Strg-C** beenden.

## Durchgang B: dasselbe, aber mit zweitem Empfänger

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-B.pcap host 192.168.16.200
```

ExpertSDR2 starten, verbinden, **RX2 einschalten**, zehn Sekunden warten,
beenden. `tcpdump` mit Strg-C beenden.

## Durchgang C (wenn es schnell gehen soll, auch später): Abtastrate

```bash
sudo tcpdump -i en9 -s 0 -w ~/Desktop/expert-C.pcap host 192.168.16.200
```

ExpertSDR2 starten, verbinden, dann **die Abtastrate umstellen** (eine
andere Bandbreite wählen), zehn Sekunden warten, beenden.

## Auswerten — ein Befehl je Frage

Der Ablauf im Überblick, und welche Rahmen Longpath nie schickt:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap
```

**Der RX2-Einschalter** — der Rahmen, der sich zwischen A und B
unterscheidet, ist er:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap --vergleich ~/Desktop/expert-B.pcap
```

**Die Abtastrate** — dasselbe mit C:

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py ~/Desktop/expert-A.pcap --vergleich ~/Desktop/expert-C.pcap
```

Beide Befehle geben am Ende fertige `LONGPATH_SUNSDR_PRE`/`_EXTRA`-Zeilen
aus, mit denen sich derselbe Ablauf **ohne Neubau** ausprobieren lässt.

## Warum der Weg über den Vergleich geht und nicht über Probieren

Die Opcode-Nummern der QRP sind nicht die der DX: bei drei am Gerät
gemessenen Befehlen liegt die QRP um eins darunter, bei `0x01` nicht
(`docs/architecture/2026-10-02-sunsdr-paritaet.md`, Abschnitt 3a). Einen
Opcode zu erraten und ans Funkgerät zu schicken ist deshalb keine
Abkürzung, sondern ein Risiko — zwei Nummern neben `RX2_ENABLE` liegen
Drive und PA-Freigabe. Der Vergleich zweier Mitschnitte beantwortet
dieselbe Frage, ohne etwas zu schicken.

## Prüfen, dass die Auswertung funktioniert, ohne Mitschnitt

```bash
python3 ~/Longpath/NereusSDR/tools/sunsdr_handshake_diff.py --selftest
```

Baut sich ein eigenes pcap und wertet es aus.
