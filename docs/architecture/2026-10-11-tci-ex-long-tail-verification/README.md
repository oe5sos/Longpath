# TCI `*_ex`-Nachzug auf Thetis 2.10.3.14/.15 — Prüfprotokoll

Datum: 2026-10-11 · Fassung: `fix/discovery-quietpolls-thetis-parity`
Gegenstelle: der eigene TCI-Server, offscreen gestartet mit eigener
Konfiguration (`LONGPATH_CONFIG_DIR`, Port 40711), **ohne Funkgerät** —
also ohne Slice und mit dem Zustand, den ein Client sieht, der sich vor
dem Funkgerät verbindet.
Werkzeug: `tools/tci_probe` (`cmake --build build --target tci_probe`).

Dass kein Funkgerät dran war, ist hier keine Einschränkung, sondern der
interessante Fall: genau in diesem Zustand hat der erste Lauf zwei Fehler
gezeigt, die am Schreibtisch keiner der 23 Matrixzeilen sehen konnte.

## Zwei Befunde aus dem ersten Lauf (behoben)

**1. Quittungen bestätigten Wünsche, nicht Zustände.**
`rx_nb_enable_ex:0,true,2` wurde mit `rx_nb_enable_ex:0,true,2`
quittiert, die unmittelbar folgende Abfrage antwortete aber
`rx_nb_enable_ex:0,false,0` — ohne Slice hatte das Setzen keine Wirkung.
Dasselbe bei `agc_auto_ex`. Seither liest jeder Setz-Pfad den Istzustand
zurück und meldet diesen. Die bestehenden Handler (`rx_nb_enable`,
`rx_nr_enable_ex` und die übrigen aus Phase 9) echoen weiter den Wunsch;
das ist eine eigene Entscheidung und hier nicht angefasst.

**2. `run_cat_ex` schickte einen Rahmen, der sich beim Client zerlegte.**
Thetis hängt dem Befehlsnamen sein `;` wieder an und baut daraus
`run_cat_ex:PS;,?;` (TCIServer.cs:5133-5146). Ein konformer Client
trennt Textrahmen am `;` — die Sonde empfing folgerichtig zwei Rahmen,
`run_cat_ex:PS` und `,?`. Wir lassen das `;` jetzt weg und senden
`run_cat_ex:PS,?;`. Begründete Abweichung, inline und in der Matrix
vermerkt.

## Zweiter Lauf, nach den Behebungen

Gesendet (`>`) und empfangen (`<`), in der Reihenfolge des Drahtes:

```
> rx_step_att_enabled_ex:0,true;
> rx_step_att_enabled_ex:0;
> rx_step_att_ex:0,12;
> rx_step_att_ex:0;
> rx_preamp_att_ex:0,-20;
> rx_preamp_att_ex:0;
> rx_nb_enable_ex:0,true,2;
> rx_nb_enable_ex:0;
> agc_auto_ex:0,true;
> agc_auto_ex:0;
> vfo_sync_ex:true;
> vfo_sync_ex;
> tx_filter_band_ex:2700,300;
> tx_filter_band_ex;
> tx_filter_band_ex:300,320;
> fm_deviation_ex:0,5000;
> run_cat_ex:PS;
> vfo_swap_ex;
> rx_step_att_ex:0,-5;
> rx_preamp_att_ex:0,10;
> rx_nb_enable_ex:0,true,3;
> audio_start:0;
> iq_start:0;
--- Antworten:
< audio_samplerate:48000
< rx_step_att_enabled_ex:0,true
< rx_step_att_enabled_ex:0,true
< rx_step_att_ex:0,12
< rx_step_att_ex:0,12
< rx_preamp_att_ex:0,-20
< rx_preamp_att_ex:0,-20
< rx_nb_enable_ex:0,false,0
< rx_nb_enable_ex:0,false,0
< rx_nb_enable:0,false
< agc_auto_ex:0,false
< agc_auto_ex:0,false
< vfo_sync_ex:true
< vfo_sync_ex:true
< tx_filter_band_ex:300,2700
< tx_filter_band_ex:300,2700
< tx_filter_band_ex:300,400
< run_cat_ex:PS,?
< audio_start:0
< iq_start:0
```

## Was das belegt

| Punkt | Beleg am Draht |
| --- | --- |
| Schrittdämpfung ein/aus | `rx_step_att_enabled_ex:0,true` gesetzt, Abfrage bestätigt `true` |
| Schrittdämpfung in dB | `0,12` gesetzt und zurückgelesen |
| Vorverstärker | `0,-20` gesetzt und zurückgelesen, negativ wie bei Thetis |
| Störblanker mit Kennzahl | ehrliche Quittung `0,false,0` ohne Slice; Abfrage liefert zusätzlich die Grundform `rx_nb_enable:0,false` wie Thetis' Doppel-Emit |
| AGC-Automatik | ehrliche Quittung `0,false` ohne Slice |
| VFO-Gleichlauf | `true` gesetzt, Abfrage bestätigt |
| Sendefilter | `2700,300` → `300,2700` (verdreht, getauscht); `300,320` → `300,400` (Mindestbreite 100 Hz) |
| `fm_deviation_ex` | keine Antwort — abgewiesen, kein erfundener Hub |
| `run_cat_ex` | ein Rahmen: `run_cat_ex:PS,?` |
| `vfo_swap_ex` | keine Antwort, wie Thetis |
| Abweisungen | `rx_step_att_ex:0,-5`, `rx_preamp_att_ex:0,10`, `rx_nb_enable_ex:0,true,3` bleiben ohne Antwort, und die vorher gesetzten 12 dB / −20 dB stehen danach unverändert |

## Was noch offen ist

- Am echten Funkgerät ist nichts davon gefahren. Mit Slice und
  Schrittdämpfer-Steuerung müssen Störblanker, AGC-Automatik,
  VFO-Gleichlauf und -Tausch noch einmal gegen die Oberfläche geprüft
  werden (greift der Gleichlauf wirklich auf Slice 1 durch, tauscht
  `vfo_swap_ex` Frequenz **und** Modus im laufenden Betrieb).
- Das Sendefilter wirkt über `TransmitModel`; ob der Wert auch im
  Sendeweg ankommt, zeigt erst ein Sendeversuch.
- `fm_deviation_ex` bleibt offen, solange es keinen FM-Hub-Zustand gibt;
  `run_cat_ex` bis zur CAT-Maschine (Phase 3K).
