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

## Was noch offen ist

- TX-Ton (`streamType` 2) wurde nicht angefordert; für ein
  Empfangs-Aufsatz ohne Senden auch nicht nötig.
- Der Empfänger muss in ExpertSDR2 **laufen**. Steht er, kommt kein
  einziger Rahmen — egal was man anfordert. Die Sonde erkennt das an
  `< stop` und verweigert dann ein Urteil.
