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
