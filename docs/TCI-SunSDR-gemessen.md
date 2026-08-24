# TCI am SunSDR2 QRP — gemessen, nicht gelesen

Gerät: SunSDR2 QRP, Serie EED03411900124, ExpertSDR2 (TCI 1.4),
gemessen am 2026-08-24 mit `tools/tci_probe.cpp`.

## Was trägt

TCI 1.4 liefert **beides**: I/Q und RX-Ton. Longpath kann als
TCI-*Client* auf ExpertSDR2 aufsetzen — das SunSDR-Netzprotokoll
muss nicht nachgebaut werden.

```
IQ        ~23 Rahmen/s, Empfänger 0, 48000 Hz, Float32, 2 Kanäle
RX-Ton    ~12 Rahmen/s, Empfänger 0, 48000 Hz, Float32, 2 Kanäle
```

Angefordert mit `audio_samplerate:48000; audio_start:0; iq_start:0;`.
Beide werden mit `audio_start:0` / `iq_start:0` bestätigt.

## Der Kopf — was wirklich drinsteht

Der Kopf ist 64 Byte, wie bei Thetis. Nachgeprüft: die Gesamtlänge
ist bei beiden Strömen **genau** `64 + Feld20 * 4`.

| Rahmen  | Byte gesamt | Feld 20 | Feld 28      |
|---------|-------------|---------|--------------|
| IQ      | 16448       | 4096    | 0            |
| RX-Ton  | 32832       | 8192    | 3218636258   |

**Feld 20 zählt ALLE Werte eines Rahmens, über die Kanäle hinweg** —
nicht Abtastungen je Kanal. 4096 Werte im IQ-Rahmen sind 2048
komplexe Abtastungen.

**Feld 28 (Kanäle) ist unbrauchbar.** ExpertSDR2 füllt es nicht: im
IQ-Rahmen steht 0, im Tonrahmen `3218636258` = `0xbfd87de2`, also
das Bitmuster einer Fließkommazahl (−1,691). Offenbar ein
wiederverwendeter Puffer. Wer dieses Feld liest, bekommt Unsinn.

Die Kanalzahl muss daher **aus dem Durchsatz** kommen:

```
IQ       23,08 Rahmen/s × 4096 Werte / 48000 Hz = 1,97
RX-Ton   11,50 Rahmen/s × 8192 Werte / 48000 Hz = 1,96
```

Zwei unabhängige Ströme, beide 2 — das ist belastbar. Für Longpath
heißt das: Kanalzahl aus dem Stromtyp ableiten (IQ = 2 verschränkt,
RX-Ton = 2 Stereo), nicht aus dem Kopf.

## Abtastraten — bis 192 kHz

`iq_samplerate:` ist umstellbar und wird bestätigt. Gemessen, jeweils
14 Sekunden, Kanalzahl als Gegenprobe:

| iq_samplerate | Rahmen/14 s | Kanäle gemessen |
|---------------|-------------|-----------------|
| 48000         | 324         | 1,97            |
| 96000         | 664         | 2,02            |
| 192000        | 1298        | 1,98            |

Die Panadapter-Spanne ist also **nicht** auf 48 kHz begrenzt. Der
RX-Ton bleibt davon unberührt bei 48 kHz.

**Das Ratenfeld im Binärkopf (Offset 4) folgt der Umstellung NICHT.**
Es steht immer auf 48000, auch wenn tatsächlich 192 kHz fließen. Wer
den Kopf glaubt, bekommt ein Viertel der Spanne und Ton in einem
Viertel der Geschwindigkeit. Die wahre Rate steht im **Textkanal**
(`iq_samplerate:` / `audio_samplerate:`) — nur dort.

Damit sind drei Kopf-Felder unbrauchbar: Rate (4), Kanäle (28), und
Feld 28 schwankt zusätzlich zwischen Durchläufen (0, 1229, oder ein
Fließkomma-Bitmuster). Verlässlich sind allein Empfänger (0),
Abtasttyp (8), Wertzahl (20) und Stromtyp (24).

## Mittenfrequenz — dds:, nicht vfo:

Für den Panadapter zählt, um welche Frequenz der I/Q-Strom selbst
liegt — das ist **nicht** dasselbe wie die VFO-Anzeige. Gemessen
gegen ExpertSDR2, ein einzelner Empfänger, VFO A auf 1.920.500 Hz,
VFO B auf 1.900.000 Hz, ZF-Durchlassmitte 1.910.670 Hz:

```
vfo:0,0,1920500     VFO-Anzeige A — innerhalb der ZF-Durchlassbreite
vfo:0,1,1900000      VFO-Anzeige B — dito
if:0,0,9830          Versatz A zur ZF-Mitte  (1920500 − 1910670)
if:0,1,-10670        Versatz B zur ZF-Mitte  (1900000 − 1910670)
dds:0,1910670         die Mitte selbst — um DIESE liegt der I/Q-Strom
```

Gegenprobe: `dds` + `if` ergibt immer wieder `vfo` (1910670 + 9830 =
1920500, 1910670 − 10670 = 1900000). `dds:` kommt in der
Selbstauskunft VOR den `vfo:`/`if:`-Zeilen, nicht danach — die
Reihenfolge ist nicht die "logische". Für Longpaths Panadapter ist
darum ausschließlich `dds:` relevant: `vfo:`/`if:` beschreiben, wo
der *Bediener* innerhalb der Durchlassbreite steht, nicht wo der
I/Q-Strom zentriert ist. `TciClient::ddcCenterHz()` liest deshalb nur
`dds:`, siehe `src/core/TciClient.h`.

## Zwei Empfänger, zwei dds:-Zeilen

`trx_count:2` — das SunSDR2 QRP meldet über ExpertSDR2 **zwei**
Empfänger-Plätze, nicht einen, und jeder hat seine eigene
Mittenfrequenz:

```
dds:0,14164070     Empfänger 0 — 20m
dds:1,1905000      Empfänger 1 — 80m
```

Gemessen am 2026-08-24 mit einer laufenden Longpath-Sitzung parallel
zur Sonde: eine Umstimmung *irgendeines* Empfängers löst sofort eine
frische `dds:`-Zeile aus, unaufgefordert, mitten in der Sitzung — nicht
nur in der Selbstauskunft. Die Zeile trägt aber **nur den Index des
tatsächlich umgestimmten Empfängers**, nie beide. `dds:0,...` kommt in
der Selbstauskunft immer vor `dds:1,...`, unabhängig davon welcher
Empfänger seit wann auf welcher Frequenz steht.

Bench-Fund: eine erste Fassung des Panadapter-Codes ignorierte den
Empfänger-Index in dieser Zeile komplett und übernahm einfach die
zuletzt gesehene `dds:` — die von Empfänger 1, weil dessen Zeile in der
Selbstauskunft zuletzt kommt. Der Panadapter blieb dadurch auf
Empfänger 1s Frequenz hängen, ganz gleich wohin Empfänger 0 (der
tatsächlich als I/Q-Strom abonnierte) umgestimmt wurde. Für Longpaths
Panadapter zählt ausschließlich die `dds:`-Zeile des Empfängers, dessen
I/Q-Strom tatsächlich angefordert wurde (`iq_start:<index>`) — jede
andere muss verworfen werden.

## Was noch offen ist

- TX-Ton (`streamType` 2) wurde nicht angefordert; für ein
  Empfangs-Aufsatz ohne Senden auch nicht nötig.
- Der Empfänger muss in ExpertSDR2 **laufen**. Steht er, kommt kein
  einziger Rahmen — egal was man anfordert. Die Sonde erkennt das an
  `< stop` und verweigert dann ein Urteil.
