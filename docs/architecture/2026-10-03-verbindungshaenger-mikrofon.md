# Der Verbindungsaufbau kann am Mikrofon hängenbleiben

**Fund vom 2026-10-03. Kein Vorschlag, eine Beschreibung** — was zu tun ist,
entscheidet der Betreiber.

## Was passiert ist

Longpath wurde gestartet und über „Connect" mit einem Funkgerät verbunden.
Danach stand das Programm. Nicht langsam — **vollständig**:

| Beobachtung | Wert |
|---|---|
| Prozessorlast | 0,2 % (also kein Drehen, sondern Warten) |
| Logdatei | letzte Zeile 17:40:39, danach nichts mehr |
| TCI-Port 50001 | horcht, nimmt Verbindungen aber nicht an (`timed out`) |
| Verbindungsdialog | reagiert nicht, auch „Abbrechen" nicht |

Die Oberfläche war vollständig tot, während das Programm äußerlich lief.

## Wo es steht

`sample(1)` auf den Prozess, Hauptfaden, von außen nach innen:

```
RadioModel::connectToRadio
  └─ QEventLoop::exec                      ← verschachtelte Ereignisschleife
       └─ (Ereignisverteilung)
            └─ WdspEngine::initialize
                 └─ WdspEngine::finishInitialization
                      └─ initializedChanged(bool)
                           └─ AudioEngine::start
                                └─ AudioEngine::ensureTxInputOpen
                                     └─ AudioEngine::makeBus
                                          └─ PortAudioBus::open   ← hier
```

Die verschachtelte Schleife ist `RadioModel.cpp:6888` und für sich korrekt
und gut begründet: sie wartet darauf, dass die WDSP-Initialisierung fertig
ist, bevor die Verbindung aufgebaut wird. Der Kommentar dort nennt sogar
ausdrücklich den Grund („no re-entrant Connect-clicks or similar").

Das Problem entsteht erst dadurch, **was in dieser Schleife noch passiert**.

## Warum das Mikrofon überhaupt beteiligt ist

`AudioEngine::ensureTxInputOpen()` sagt es selbst:

> Phase 3M-1b: open the platform-default mic on `start()` so the
> PhoneCwApplet mic-level meter (and PcMicSource on TX) has signal without
> requiring the user to visit Setup → Audio → Devices.

Also eine **Bequemlichkeit**: der Pegelbalken soll ohne Zutun Ausschlag
zeigen. Dafür wird der Mikrofon-Eingang beim Verbindungsaufbau geöffnet —
obwohl zum Empfangen kein Mikrofon nötig ist.

`PortAudioBus::open()` hat dabei **kein Zeitlimit**. Blockiert CoreAudio,
blockiert der ganze Hauptfaden, und mit ihm alles: Oberfläche, TCI-Server,
Abbruchmöglichkeit.

## Der Auslöser hier — und warum er nicht die Ursache ist

Ausgelöst wurde es dadurch, dass „Connect" aus dem Hintergrund gedrückt
wurde, während Longpath in einem Vollbild-Space lag. macOS wollte die
Mikrofon-Berechtigung erfragen; der Dialog erschien dort, wo niemand
hinsah. Belegt durch `UserNotificationCenter`, gestartet **eine Sekunde**
nach Longpath und seither stehend.

Das ist ein ungewöhnlicher Weg in den Zustand. Die Bruchstelle ist er
nicht — dieselbe Blockade entsteht bei:

* einem Audio-Interface, das ein anderes Programm exklusiv hält,
* einem USB-Gerät, das beim Aufwachen hakt,
* jeder noch nicht beantworteten Berechtigungsanfrage,
* einem CoreAudio-Treiber, der langsam antwortet.

Dass dieser Pfad unzuverlässig ist, steht übrigens schon im Haus:
`MacMicPermission.h` beschreibt, dass `ensureTxInputOpen()` über PortAudio
„in practice unreliable" ist, und holt die Berechtigung deshalb beim
Programmstart ausdrücklich ein. Die Lösung dort ist richtig und greift hier
nur nicht, wenn die Antwort noch aussteht.

## Was im Contest daran gefährlich ist

Der Verbindungsaufbau ist genau der Augenblick, in dem man es eilig hat.
Friert er ein, ist kein Weg zurück: kein Abbrechen, kein anderes Funkgerät
wählen, kein Beenden über das Menü. Es bleibt das Abschießen des Programms
— mit allem, was an Fensterlage und Einstellungen nicht gesichert ist.

## Richtungen, bewusst nicht entschieden

1. **Zeitlimit um `PortAudioBus::open()`.** Kleinster Eingriff. Nach N
   Sekunden aufgeben, warnen, ohne Mikrofon weitermachen — empfangen geht
   auch so.
2. **Das Mikrofon später öffnen.** Nicht beim Verbinden, sondern wenn es
   gebraucht wird: beim ersten MOX oder wenn das Applet mit dem Pegelbalken
   sichtbar wird. Der Verbindungsweg wäre dann frei von Audio-Eingaben.
3. **Beides.** Das Zeitlimit als Netz, die späte Öffnung als eigentliche
   Behebung.

Richtung 2 ist die sauberere und die größere Änderung; sie berührt den
TX-Pfad und gehört deshalb dem Betreiber. Richtung 1 wäre in einer
Sitzung erledigt.

## Wie man es nachstellt

Longpath so starten, dass es nicht im sichtbaren Space liegt, und die
Mikrofon-Berechtigung noch nicht erteilt haben. Dann verbinden. Sicherer
Nachweis auch ohne das: `sample <pid> 3` während des Aufbaus — steht
`PortAudioBus::open` unter `connectToRadio`, ist es dieser Pfad.

---

## Nachtrag 2026-10-03 abends — zwei Funde, beide am Code und am Betriebslog

### 1. Es ist nicht ein Gerät, es sind sieben

Der Nachmittag hat das Mikrofon behoben (eigener Zweig: keine eifrige
Öffnung, solange TCC die Frage noch stellt). Das schließt den *beobachteten*
Fall, nicht die Gattung. `AudioEngine::start()` öffnet nämlich **sieben**
Geräte hintereinander, jedes ohne Zeitlimit, alle in derselben
verschachtelten Ereignisschleife:

| # | Aufruf | was dahinter liegt |
|---|---|---|
| 1 | `ensureSpeakersOpen()` | Lautsprecher, `PortAudioBus` |
| 2 | `ensureTxInputOpen()` | Mikrofon, `PortAudioBus` |
| 3–6 | `makeVaxBus(1..4)` | VAX RX 1–4, CoreAudio-HAL-Plug-in |
| 7 | `makeVaxTxBus()` | VAX TX, CoreAudio-HAL-Plug-in |

Jeder einzelne davon kann aus denselben Gründen hängen wie das Mikrofon.
Eine Berechtigungsprüfung je Gerät ist deshalb keine Lösung der Gattung —
sie war nur die richtige Lösung des einen Falls, der wirklich eingetreten
ist.

### 2. Gemessen, nicht geschätzt: im Normalfall dauert das Ganze 20 ms

Aus Martins Betriebslog vom 2026-10-03, 19:02 (Verbindung zur SunSDR QRP,
alles in Ordnung):

```
19:02:41.274  locked 49152 bytes for tag PortAudioBus::m_ring
19:02:41.282  PortAudioBus: mic opened at native 44100 Hz, resampling to 48000 Hz
19:02:41.292  PortAudioBus: input via [Core Audio] on "BoomAudio" — latency 11.6 ms
19:02:41.292  VAX 1..4 + VAX TX bus opened (eager)
19:02:41.292  AudioEngine started ( speakers bus open )
```

Also: **Mikrofon 18 ms, alle fünf VAX-Busse zusammen unter 1 ms.** Ein
Zeitlimit von wenigen Sekunden wäre damit vier Größenordnungen über dem
Normalfall — es kann im gesunden Betrieb nicht auslösen. Das ist das
Argument für Richtung 1, und es ist jetzt belegt statt geraten.

Nebenbefund aus derselben Zeile, der die Gefahr nicht kleiner macht: der
TX-Eingang ist hier **kein eingebautes Mikrofon**, sondern `BoomAudio` —
ein fremdes virtuelles Audiogerät, das mit 44 100 Hz läuft und über r8brain
hochgerechnet wird. Genau diese Sorte Gerät (fremder Treiber, der nebenbei
noch von einem anderen Programm gehalten werden kann) ist der plausibelste
Kandidat für ein Hängen ohne Berechtigungsdialog.

### 3. Der Zustand war aus dem Log nicht zu erkennen — und das war kein Zufall

Es gibt eine Zeile, die den ganzen Nachmittag erspart hätte:

```
INF: Microphone TCC status on launch: NotDetermined
```

`requestMicrophonePermission()` schreibt sie bei jedem Start. In keinem von
Martins Logs steht sie. Der Grund ist Reihenfolge, nicht Filterung: der
Aufruf stand in `main.cpp` rund **45 Zeilen vor** dem Öffnen der Log-Datei
und dem `qInstallMessageHandler`. Er lief also, seine Ausgabe ging ins
Leere.

Behoben im selben Zug: der Aufruf steht jetzt direkt nach
`logStartupHardwareInventory()`, also nach dem Umleiter. Später ist
gefahrlos — der Mikrofon-Eingang wird erst beim Verbinden geöffnet, und bis
dahin liegen Fenster und Ereignisschleife längst.

**Die Regel dahinter, und sie ist allgemeiner als dieser Fall:** eine
Diagnoseausgabe vor dem Einrichten des Logs ist keine Diagnoseausgabe. Wer
in `main()` etwas protokolliert, muss wissen, ob der Umleiter schon steht.

### 4. Dieselbe Falle ein zweites Mal — und dort verschwand eine Warnung

Nachdem die Reihenfolge einmal aufgefallen war, habe ich `main()` darauf
durchgesehen. Es gibt einen zweiten Fall, und der ist unangenehmer:

`Longpath::elevateGuiMainThreadPriority()` lief rund **70 Zeilen vor** dem
`qInstallMessageHandler`. Diese Funktion meldet im Erfolgsfall

```
INF: GUI main thread elevated to USER_INTERACTIVE QoS
```

und im Misserfolgsfall

```
WRN: Failed to elevate GUI main thread (errno …)
```

**In keinem der fünf vorliegenden Betriebslogs steht eine der beiden
Zeilen.** Die Erfolgsmeldung zu verlieren ist Kosmetik; die Warnung zu
verlieren ist es nicht — sie ist die einzige Stelle, an der man sieht, dass
die Oberfläche auf voreingestellter Dienstgüte läuft. Genau das war 2026-05
das Fehlerbild, für das die Erhöhung eingebaut wurde („whole program
stutters when a build happens"). Hätte sie stillschweigend nicht gegriffen,
wäre das aus dem Log nicht zu belegen gewesen.

Beide Aufrufe stehen jetzt hinter dem Umleiter. Dass das Log nicht noch
früher geöffnet wird, ist Absicht: der Block läge sonst vor der
Kommandozeilen-Auswertung, und dann würde jedes `Longpath --version` eine
neue Logdatei anlegen **und die älteste wegwerfen** (die Ablage hält fünf).

**Belegt, nicht geschätzt:** `grep -l "GUI main thread elevated"` über alle
fünf Logs → 0 Treffer bei 5 Dateien. Die Zeile existiert im Quellcode
(`RealtimeAudioPriority.cpp:160`).
