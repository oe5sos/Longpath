// =================================================================
// src/gui/MainWindow_Flex.cpp  (Longpath)
// =================================================================
//
// Flex-Treiber Stufe 2: Ton und Bild ins Fenster.
//
// Longpath-original — aber es haengt an zwei Naehten, die aus AetherSDR
// stammen (https://github.com/aethersdr/AetherSDR, GPLv3, primary
// author Jeremy [KK7GWY]), und deshalb steht das hier und nicht nur in
// der Herkunftstabelle:
//
//   * `AudioEngine::feedFlexAudioData` ist das dritte Mal desselben
//     Musters; die zwei davor (`feedKiwiSdrAudioData`,
//     `feedSunSdrAudioData`) sind aus AetherSDRs Tonweg portiert und
//     muenden wie dieser in `rxBlockReady`.
//   * `SpectrumWidget::updateKiwiSpectrumDbm` ist der Eingang fuer
//     fertige dBm-Bins einer fremden Quelle; seine Begruendung („keine
//     zweite Rohrleitung noetig") ist in SpectrumWidget.h an AetherSDR
//     zitiert.
//   * Das Sicherheitstor (`streamIndex() >= 0` nie fuettern) kommt aus
//     der SunSDR-Durchsicht, die ihrerseits AetherSDRs Aufbau folgt.
//
// Dieselbe Lage wie `src/gui/applets/StripGraphics.h`: eigener Code,
// der auf portiertem aufsitzt — also wird die Quelle benannt, nicht nur
// die eigene Urheberschaft.
//
// Der Zuschnitt folgt den zwei fremden Quellen, die es schon gibt —
// MainWindow_KiwiSdr.cpp (875 Zeilen) und MainWindow_SunSdr.cpp (849) —
// und ist deutlich kleiner als beide, weil ein Flex kein
// Profil-Verwaltungswerk braucht: es gibt genau eine Maschine, und ihre
// Stroeme tragen ihre Kennung im Kopf.
//
// Was hier passiert, ist die Naht aus
// docs/architecture/2026-10-18-flex-stufe1-design.md §5:
//
//   Ton  -> AudioEngine::feedFlexAudioData  -> rxBlockReady
//           (der EINE Punkt, an dem fertiger Ton hereinkommt; daneben
//            liegen feedKiwiSdrAudioData und feedSunSdrAudioData)
//   Bild -> SpectrumWidget::updateKiwiSpectrumDbm
//           (fertige dBm-Bins ueber einen eigenen Ausschnitt; KEINE
//            zweite Rohrleitung noetig — das begruendet SpectrumWidget.h
//            selbst bei :1405-1421)
//
// Die 34 WDSP-Stellen in RadioModel bleiben unberuehrt: eine
// Flex-Scheibe ist eine Scheibe OHNE RxChannel, und sie fragen alle
// nach einem.
//
// =================================================================
// Zum Namen `updateKiwiSpectrumDbm`
// =================================================================
//
// Der Eingang heisst `kiwi…`, weil er fuer den KiwiSDR gebaut wurde; er
// ist aber der allgemeine Eingang fuer FERTIGE dBm-Bins einer fremden
// Quelle, und die Begruendung in SpectrumWidget.h ist nicht
// kiwi-spezifisch. Hier wird er darum bewusst mitbenutzt statt
// hundert Zeilen daneben zu stellen. Das ist eine Namensschuld, kein
// Fehler — ein Umbenennen waere ein eigener Handgriff.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

#include "gui/MainWindow.h"

#include "core/AudioEngine.h"
#include "core/FlexVitaStream.h"
// Fuer den Anspruchs-Abgleich: Kiwi fuehrt seine Zuordnung in einer
// eigenen Tabelle, nicht ueber streamIndex().
#include "core/KiwiSdrManager.h"
#include "gui/meters/MeterPoller.h"
#include "gui/ConnectionPanel.h"
#include "core/ClarityController.h"
#include <QTimer>
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/BandDefaults.h"

#include <QLoggingCategory>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcFlexGui, "longpath.flex.gui", QtInfoMsg)

// Gemessen am 2026-10-18: 24 012,3 Hz ueber ein 24-Sekunden-Fenster,
// also 24 kHz auf 0,05 % genau. Als Konstante und nicht als Annahme im
// Wandler — eine falsche Rate hoert man sofort als falsche Tonhoehe.
constexpr int kFlexAudioRateHz = 24000;
}  // namespace

// Dasselbe Tor wie sunSdrControllableSlice(): die harte Regel aus der
// SunSDR-Sicherheitsdurchsicht gilt fuer jede fremde Quelle — eine
// Scheibe mit echter DDC-Bindung (`streamIndex() >= 0`, ein wirkliches,
// womoeglich sendefaehiges Geraet) wird aus diesem Weg NIE gefuettert
// oder gesteuert.
//
// Die `m_shuttingDown`-Wache ist nicht Schmuck: `m_radioModel` ist ein
// rohes RadioModel*, kein QPointer, wird beim Abbau also freigegeben und
// nicht auf nullptr gesetzt (derselbe ASAN-bestaetigte Heap-UAF wie bei
// SunSDR, docs/architecture/2026-08-29-sunsdr-tci-teardown-segfault-investigation.md).
SliceModel* MainWindow::flexControllableSlice() const
{
    if (m_shuttingDown) { return nullptr; }
    if (!m_radioModel || m_flexTargetSliceId < 0) { return nullptr; }
    SliceModel* slice = m_radioModel->sliceById(m_flexTargetSliceId);
    if (!slice || slice->streamIndex() >= 0) { return nullptr; }
    return slice;
}

void MainWindow::wireFlex()
{
    if (!m_radioModel) { return; }

    // ── Die Zielscheibe waehlen, sobald die Anmeldung durch ist ──────
    //
    // Erst dann gibt es ueberhaupt Stroeme. Gewaehlt wird nach derselben
    // Regel wie bei den zwei anderen fremden Quellen, und mit deren
    // Anspruechen: Kiwi und SunSDR fuehren ihre Zuordnung ebenfalls
    // nicht ueber `streamIndex()`, also muss sie hier mitgeprueft
    // werden — sonst traegt eine Scheibe zwei Erzeuger, und die Spur
    // bekommt zwei Faeden.
    connect(m_radioModel, &RadioModel::flexGuiClientRegistered, this,
            [this](const QString&) {
        if (m_shuttingDown || !m_radioModel) { return; }

        const auto istFremdBelegt = [this](const SliceModel* s) {
            if (!s) { return true; }
            if (m_sunSdrTargetSliceId == s->sliceIndex()) { return true; }
            if (m_kiwiSdrManager
                && !m_kiwiSdrManager->assignedProfileForSlice(s->sliceIndex()).isEmpty()) {
                return true;
            }
            return false;
        };

        SliceModel* ziel = m_radioModel->activeSlice();
        if (ziel && (ziel->streamIndex() >= 0 || istFremdBelegt(ziel))) {
            ziel = nullptr;
        }
        if (!ziel) {
            for (SliceModel* kandidat : m_radioModel->slices()) {
                if (kandidat && kandidat->streamIndex() < 0
                    && !istFremdBelegt(kandidat)) {
                    ziel = kandidat;
                    break;
                }
            }
        }
        if (!ziel) {
            // 2026-10-21 an Martins Station: ALLE Scheiben hingen an
            // einem echten Geraet (`streamIndex() >= 0`), also war keine
            // frei — und damit lief weder Ton NOCH Bild, denn beides
            // haengt an dieser Zuordnung (der Panadapter ueber
            // `setKiwiDisplaySource(true)` weiter unten). Die alte
            // Warnung behauptete „Bild laeuft, Ton nicht"; das war
            // falsch und hat eine Stunde Suche gekostet.
            //
            // Die Flex ist ein eigenes Geraet und wartet nicht darauf,
            // dass eine Scheibe frei wird: sie bekommt ihre eigene.
            // `suppressAutoStreamBinding` ist dabei Pflicht — ohne das
            // greift `bindSliceToStream()` zu, die Scheibe bekommt
            // einen echten `streamIndex()`, und die Sicherheitswache
            // liest das als „ein wirkliches Geraet hat uebernommen"
            // und schneidet genau den Weg ab, den wir gerade bauen.
            const int neu = m_radioModel->addSlice(QString(), true);
            ziel = (neu >= 0) ? m_radioModel->sliceById(neu) : nullptr;
            m_flexSliceWasCreated = (ziel != nullptr);
        }
        if (!ziel) {
            qCWarning(lcFlexGui) << "keine Scheibe fuer die Flex, und das "
                                    "Anlegen misslang — weder Ton noch Bild";
            return;
        }

        m_flexTargetSliceId = ziel->sliceIndex();

        // ── Und sie muss die AKTIVE Scheibe sein (2026-10-22) ────────
        //
        // `RadioModel::onBandButtonClicked` — der Weg jedes Band- und
        // Frequenzwechsels — beginnt mit `activeSlice()` und kehrt ohne
        // ein Wort um, wenn es keine gibt. Beim Flex legen wir die
        // Scheibe selbst an; aktiv wird sie dabei nicht. Folge am
        // Geraet am 2026-10-22: der Betreiber drehte einen Abend lang
        // an Band und Frequenz, und an die Maschine ging NULL Mal ein
        // `slice tune`. Sie stand die ganze Zeit auf 14.100.
        m_radioModel->setActiveSliceById(m_flexTargetSliceId);

        // Von innen nach aussen. Erst HIER verdrahtet, weil es die
        // Zielscheibe vorher nicht gibt.
        //
        // ── `Qt::UniqueConnection` geht nicht mit einer Lambda ───────
        //
        // Hier stand `Qt::UniqueConnection`, damit ein zweites Anmelden
        // nicht doppelt sendet. Qt unterstuetzt das aber NUR fuer
        // Zeiger auf Elementfunktionen; bei einer Lambda scheitert
        // `connect()` still und gibt eine leere Verbindung zurueck —
        // es wird GAR NICHTS verbunden.
        //
        // Folge am Geraet, 2026-10-22, ein ganzer Abend: der Betreiber
        // drehte an Band und Frequenz, die Anzeige folgte, und an die
        // Maschine ging in sechs Laeufen NULL Mal ein `slice tune`. Die
        // Flex stand die ganze Zeit auf 14.100 USB — dort ist nur
        // Rauschen, und genau das hat er gehoert. Gemessen:
        //   Klick im Panadapter "pan-0" 1.41747e+07 Scheibe 0
        //   gesperrt false
        // und danach kein Mucks mehr.
        //
        // Das Doppelte wird jetzt von Hand verhindert: die alte
        // Verbindung wird vor der neuen geloest.
        disconnect(m_flexAbstimmBand);
        m_flexAbstimmBand = connect(ziel, &SliceModel::frequencyChanged, this,
                [this](double hz) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            // Sagt, WARUM nicht abgestimmt wird. Ohne das sieht ein
            // nicht durchgereichter Drehknopf genauso aus wie einer,
            // der gar nicht erst ankommt (2026-10-22, eine Stunde).
            if (!flexControllableSlice()) {
                SliceModel* s = m_radioModel
                    ? m_radioModel->sliceById(m_flexTargetSliceId) : nullptr;
                qCWarning(lcFlexGui)
                    << "Abstimmen verworfen — Zielscheibe"
                    << m_flexTargetSliceId
                    << "vorhanden" << (s != nullptr)
                    << "streamIndex" << (s ? s->streamIndex() : -999);
                return;
            }
            m_radioModel->tuneFlexSlice(hz);
        });

        // ── Betriebsart und Filterbreite, derselbe Weg ──────────────
        //
        // Bis zum 2026-10-22 ging nur die Frequenz hinaus — und auch
        // die nicht (siehe oben). Mit dem Abstimmen allein hoerte der
        // Betreiber Stationen, aber keine verstaendliche Sprache:
        // „als waere es eine andere modulation". Das war sie auch.
        // Longpath zeigte LSB, die Maschine stand auf USB, weil die
        // Betriebsart nur EINMAL beim Verbinden gesetzt wurde.
        //
        // Dieselbe Wache wie bei der Frequenz: ein Zustandsschub der
        // Maschine darf den Befehl nicht zurueckwerfen, und eine
        // Scheibe mit echter DDC-Bindung wird nie gefuettert.
        disconnect(m_flexArtBand);
        m_flexArtBand = connect(ziel, &SliceModel::dspModeChanged, this,
                [this](DSPMode art) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceMode(art);
        });

        disconnect(m_flexFilterBand);
        m_flexFilterBand = connect(ziel, &SliceModel::filterChanged, this,
                [this](int tief, int hoch) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceFilter(tief, hoch);
        });

        // ── Rauschminderung: der Knopf schaltet die MASCHINE ────────
        //
        // Der Betreiber am 2026-10-28: „ich kann zwar klicken, aber
        // kein unterschied zu hören." Flex-Ton kommt fertig
        // demoduliert in den Mischer; Longpaths NR/NB/ANF sitzen im
        // WDSP-Empfangskanal, den dieser Ton nie durchläuft. Also
        // gehen die drei Schalter dorthin, wo gerechnet wird.
        //
        // Longpath kennt vier Verfahren und einen Dreifachschalter für
        // den Austaster, die Maschine je einen Schalter. Übersetzt
        // wird auf AN/AUS: welches Verfahren im Gerät läuft, bestimmt
        // das Gerät — eine NR2-Marke an einem fremden Algorithmus wäre
        // eine Beschriftung, die nichts bedeutet.
        auto rauschHinaus = [this]() {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            SliceModel* s = m_radioModel
                ? m_radioModel->sliceById(m_flexTargetSliceId) : nullptr;
            if (!s) { return; }

            // ── Wer rechnet: die Maschine oder Longpath? ─────────────
            //
            // Betreiber am 2026-10-28: „aber jeder nr sollte anders
            // funktionieren! das ist bei anvelina ja schon!" Also
            // teilen sich die Steckplaetze auf:
            //
            //   NR1 / NR2 / NR4  die Maschine (anr, emnr und sbnr sind
            //                    WDSP-Einheiten und haben auf fertigem
            //                    Ton kein Gegenstueck)
            //   NR3              rnnoise in Longpath
            //   MNR              Apple Accelerate in Longpath
            //
            // Nie beide zugleich: zwei Minderungen hintereinander
            // klingen nach Eimer, und keiner der beiden Regler wuerde
            // dann noch das tun, was auf ihm steht.
            const NrSlot gewaehlt = s->activeNr();
            const bool eigenesVerfahren =
                (gewaehlt == NrSlot::NR3 || gewaehlt == NrSlot::MNR);

            if (AudioEngine* ae = m_radioModel->audioEngine()) {
                ae->setExternalAudioNr(s->sliceIndex(),
                                       eigenesVerfahren ? gewaehlt : NrSlot::Off,
                                       1.0f);
            }

            m_radioModel->setFlexSliceNoise(
                gewaehlt != NrSlot::Off && !eigenesVerfahren,
                s->nbMode() != NbMode::Off,
                s->anfEnabled());
        };
        disconnect(m_flexNrBand);
        m_flexNrBand = connect(ziel, &SliceModel::activeNrChanged, this,
                               [rauschHinaus](NrSlot) { rauschHinaus(); });
        disconnect(m_flexNbBand);
        m_flexNbBand = connect(ziel, &SliceModel::nbModeChanged, this,
                               [rauschHinaus](NbMode) { rauschHinaus(); });
        disconnect(m_flexAnfBand);
        m_flexAnfBand = connect(ziel, &SliceModel::anfEnabledChanged, this,
                                [rauschHinaus](bool) { rauschHinaus(); });

        // Und einmal gleich jetzt, damit die Maschine mit dem
        // uebereinstimmt, was im Fenster steht — sonst gilt der
        // Unterschied bis zum ersten Drehen weiter.
        m_radioModel->setFlexSliceMode(ziel->dspMode());
        m_radioModel->setFlexSliceFilter(ziel->filterLow(), ziel->filterHigh());
        rauschHinaus();

        // ── Beim VERBINDEN gewinnt die MASCHINE ─────────────────────
        //
        // Zwei Dinge laufen hier gegeneinander: Longpath uebernimmt
        // zuerst Frequenz und Ausschnitt der Maschine, danach stellt
        // das Layoutprofil die gespeicherte Scheibe her — und die kann
        // auf einem ganz anderen Band stehen. Am Geraet gemessen am
        // 2026-10-23: die Maschine lieferte 7.065–7.265 MHz, das
        // Fenster stand nach dem Profil auf 14.150–14.300. KEINE
        // Ueberlappung, also hatte der Panadapter nichts zu zeichnen
        // (Bandmeldung: Rauschteppich −600 dBm, Hub 0 dB). Der
        // Betreiber: „oben alles schwarz".
        //
        // Der erste Versuch zog den PANADAPTER auf die Scheibe. Das
        // war verkehrt herum und sofort zu merken: der Betreiber hoerte
        // auf 80 m, und das Programm riss ihm die Maschine beim
        // Starten auf die 20 m aus dem gespeicherten Profil. Wer das
        // Geraet gerade von SmartSDR oder der Frontplatte aus benutzt,
        // soll davon nichts merken.
        //
        // Also andersherum: nach dem Profilaufbau wird die Frequenz
        // der MASCHINE in die Scheibe zurueckgeschrieben. Der
        // Panadapter stimmt dann ohnehin, denn er steht dort schon.
        // 1500 ms, weil der Profilaufbau in sechs Schritten laeuft und
        // beim Messen rund 600 ms brauchte.
        QTimer::singleShot(1500, this, [this]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            SliceModel* s = flexControllableSlice();
            if (!s) { return; }
            const double geraet = m_radioModel->flexGeraeteFrequenzHz();
            if (!(geraet > 0.0)) { return; }
            if (qFuzzyCompare(geraet, s->frequency())) { return; }
            qCInfo(lcFlexGui).nospace()
                << "Flex: das Profil stellte " << (s->frequency() / 1.0e6)
                << " MHz her, die Maschine steht auf " << (geraet / 1.0e6)
                << " MHz — die Maschine gewinnt";
            // Die Wache setzen: sonst schickt die Scheibe die eben
            // uebernommene Frequenz als Befehl zurueck.
            m_flexApplyingRemoteState = true;
            s->setFrequency(geraet);
            m_flexApplyingRemoteState = false;

            // ── Und das Seitenband zum neuen Band (2026-10-23) ───────
            //
            // Betreiber: „80 und 160 sowie 40 muessen immer automatisch
            // bei bandauswahl LSB sein". Dass die Maschine hier die
            // Frequenz durchsetzt, IST eine Bandauswahl — nur eine, die
            // nicht ueber den Bandknopf laeuft und damit auch nicht
            // ueber `SliceModel::restoreFromSettings`, wo die Regel
            // sonst greift. Am Geraet kam 40 m deshalb in USB: das
            // Profil stand auf 20 m, die Flex zog auf 7,148 MHz, und
            // das USB des 20-m-Platzes blieb stehen.
            //
            // NICHT in `SliceModel::setFrequency` allgemein: `setDspMode`
            // setzt dort die Filterbreite auf die Vorgabe der neuen
            // Betriebsart zurueck und hat mitten im Wiederherstellen
            // drei Pruefstaende der Ablage umgeworfen.
            //
            // Ohne Wache: diese Betriebsart SOLL zur Maschine hinaus.
            const DSPMode gerichtet =
                BandDefaults::korrigiereSeitenband(
                    bandFromFrequency(geraet), s->dspMode());
            if (gerichtet != s->dspMode()) {
                qCInfo(lcFlexGui).nospace()
                    << "Flex: " << bandLabel(bandFromFrequency(geraet))
                    << " gehoert auf " << (gerichtet == DSPMode::LSB
                                           ? "LSB" : "USB")
                    << " — Seitenband gerichtet";
                s->setDspMode(gerichtet);
            }
        });

    // ── Betriebsart und Filterbreite, derselbe Weg ──────────────
        //
        // Bis zum 2026-10-22 ging nur die Frequenz hinaus — und auch
        // die nicht (siehe oben). Mit dem Abstimmen allein hoerte der
        // Betreiber Stationen, aber keine verstaendliche Sprache:
        // „als waere es eine andere modulation". Das war sie auch.
        // Longpath zeigte LSB, die Maschine stand auf USB, weil die
        // Betriebsart nur EINMAL beim Verbinden gesetzt wurde.
        //
        // Dieselbe Wache wie bei der Frequenz: ein Zustandsschub der
        // Maschine darf den Befehl nicht zurueckwerfen, und eine
        // Scheibe mit echter DDC-Bindung wird nie gefuettert.
        disconnect(m_flexArtBand);
        m_flexArtBand = connect(ziel, &SliceModel::dspModeChanged, this,
                [this](DSPMode art) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceMode(art);
        });

        disconnect(m_flexFilterBand);
        m_flexFilterBand = connect(ziel, &SliceModel::filterChanged, this,
                [this](int tief, int hoch) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceFilter(tief, hoch);
        });

        // Und einmal gleich jetzt, damit die Maschine mit dem
        // uebereinstimmt, was im Fenster steht — sonst gilt der
        // Unterschied bis zum ersten Drehen weiter.
        m_radioModel->setFlexSliceMode(ziel->dspMode());
        m_radioModel->setFlexSliceFilter(ziel->filterLow(), ziel->filterHigh());

        // ── Panadapter und Scheibe muessen beim VERBINDEN zusammen ──
        //
        // Beim Verbinden laufen zwei Dinge gegeneinander: Longpath
        // uebernimmt zuerst den Ausschnitt der Maschine, danach stellt
        // das Layoutprofil die gespeicherte Scheibe wieder her — und
        // die kann auf einem ganz anderen Band stehen.
        //
        // Am Geraet gemessen am 2026-10-23: die Maschine lieferte
        // 7.065–7.265 MHz, das Fenster stand nach dem Profil auf
        // 14.150–14.300. KEINE Ueberlappung, also hatte der
        // Panadapter nichts zu zeichnen (Bandmeldung: Rauschteppich
        // −600 dBm, Hub 0 dB) und der Wasserfall war einfarbig. Der
        // Betreiber: „oben alles schwarz".
        //
        // Darum einmal nachziehen, NACHDEM das Profil durch ist. Die
        // Scheibe gewinnt: sie ist das, was der Betreiber sieht und
        // hoert. 1500 ms, weil der Profilaufbau in sechs Schritten
        // laeuft und beim Messen rund 600 ms brauchte.
        QTimer::singleShot(1500, this, [this]() {
            if (m_shuttingDown) { return; }
            SliceModel* s = flexControllableSlice();
            if (!s || !m_radioModel) { return; }
            const double hz = s->frequency();
            if (!(hz > 0.0)) { return; }
            qCInfo(lcFlexGui).nospace()
                << "Flex: Panadapter auf die Scheibe gezogen — "
                << (hz / 1.0e6) << " MHz";
            m_radioModel->zieheFlexPanadapterAuf(hz);
        });
        if (AudioEngine* ae = m_radioModel->audioEngine()) {
            // ── Den Ausgang anwerfen (2026-10-22) ────────────────────
            //
            // `AudioEngine::start()` oeffnet den Lautsprecherausgang und
            // laesst den Abfluss aus dem MasterMixer anlaufen. Im ganzen
            // Programm steht der Aufruf an genau EINER Stelle: im
            // WDSP-Init-Rueckruf nach dem Verbinden mit einem EIGENEN
            // Funkgeraet (RadioModel.cpp). Beim Flex gibt es den nicht —
            // die Maschine rechnet selbst —, also lief der Ausgang nie
            // an.
            //
            // Gemessen am 2026-10-22 an Martins FLEX-8400: der Ton kam
            // bis in die Mischung und weiter nicht. Sonde am Abfluss,
            // alle fuenf Sekunden:
            //   Scheibe 0, mixed 256, Spitze 0,07, Vol 0,82,
            //   Stumm false, Leerdrains 0, Bus FALSE
            // Alles richtig bis zum letzten Schritt, und dort kein
            // offener Ausgang. Der Betreiber hoerte eine Stunde lang
            // nichts, obwohl die Maschine 30 987 Tonpakete geschickt
            // hatte.
            //
            // SunSDR hat denselben Griff und dieselbe Begruendung
            // (MainWindow_SunSdr.cpp, 2026-08-24) — beim Flex ist er
            // schlicht vergessen worden. `start()` schuetzt sich gegen
            // doppeltes Anlaufen (`if (m_running) return;`), ist hier
            // also gefahrlos: laeuft der Ausgang schon durch ein eigenes
            // Geraet, geschieht nichts.
            ae->start();
            // Opportunistisch im Mix: ein Netzaussetzer zum Flex darf
            // nicht den ganzen Ton anhalten.
            ae->setFlexAudioSourceEnabled(m_flexTargetSliceId, true);
        }
        qCInfo(lcFlexGui) << "Flex-Ton auf Scheibe" << m_flexTargetSliceId;

        // ── Senden vorbereiten, NICHT tasten (2026-10-23, Stufe 2) ───
        //
        // Drei Dinge muessen stehen, bevor ein `xmit` ueberhaupt Sinn
        // hat, und keines davon sendet:
        //
        //   1. Die Maschine muss wissen, WELCHE Scheibe sendet —
        //      ohne `slice set <n> tx=1` weist sie `xmit` ab.
        //   2. Der Sendetonstrom (`stream create type=dax_tx`) muss
        //      angelegt sein; seine Kennung gehoert in jedes Tonpaket.
        //   3. Der Senderzweig muss auf DAX stehen. Sonst nimmt die
        //      Maschine das Mikrofon an ihrer eigenen Buchse und wirft
        //      jedes unserer Pakete weg — getastet wird, gesendet wird
        //      Stille (AetherSDR AudioEngine.cpp:8954 [@d58e2b8a]).
        //
        // Punkt 3 steht hier bewusst NICHT: `dax=1` nimmt dem
        // Betreiber das Mikrofon an der Maschine weg, und das gehoert
        // nicht ins Verbinden. Es wird erst beim Tasten gesetzt.
        m_radioModel->setzeFlexSendescheibe(
            QString::number(m_flexTargetSliceId));
        m_radioModel->legeFlexSendetonstromAn();
        verdrahteFlexSenden();

        // ── Die Maschine folgt dem Blick des Betreibers (2026-10-27) ─
        //
        // Betreiber: „immer beim verschieben kommt es zu den
        // problemen". Und so entstehen sie: schiebt er das Bild, wandert
        // `m_centerHz` — der Wasserfall folgt mit (die Kachel wird ueber
        // die Frequenz abgebildet), die Bild-FFT aber deckt weiterhin
        // nur den Ausschnitt der MASCHINE. Je weiter er schiebt, desto
        // weniger ueberlappen die beiden, und am Ende steht im
        // Wasserfall etwas, wovon das Spektrum nichts weiss.
        //
        // Richtig ist, dass die Maschine dem Blick folgt: bei SmartSDR
        // IST der Panadapter das Fenster. Also seine Mitte hinausgeben.
        //
        // Entprellt: beim Ziehen kommt `centerChanged` je Bildpunkt,
        // und jeder Befehl waere ein Umlauf zur Maschine. 250 ms nach
        // dem letzten Ruck genuegt — der Bildstrom laeuft derweil
        // weiter, nur eben noch auf dem alten Ausschnitt.
        if (!m_flexPanNachzug) {
            m_flexPanNachzug = new QTimer(this);
            m_flexPanNachzug->setSingleShot(true);
            m_flexPanNachzug->setInterval(250);
            connect(m_flexPanNachzug, &QTimer::timeout, this, [this]() {
                if (m_shuttingDown || !m_radioModel) { return; }
                if (m_flexTargetSliceId < 0) { return; }
                SpectrumWidget* sw = m_radioModel->spectrumWidget();
                if (!sw) { return; }
                const double mitte = sw->centerFrequency();
                const double panMitte =
                    (m_flexSpanLowHz + m_flexSpanHighHz) * 0.5;
                // ── Das Totband muss KLEIN sein (2026-10-27) ─────
                //
                // Hier stand ein Zehntel der Breite, „eine
                // Verschiebung, die man ohnehin nicht sieht". Man
                // sieht sie doch — nur nicht als Verschiebung,
                // sondern als DEHNUNG.
                //
                // Gemessen: Fenster 7,0961 MHz, Panadapter der
                // Maschine 7,0788 — 17,3 kHz Unterschied, knapp unter
                // dem alten Totband von 20 kHz. Das Fenster ragt damit
                // oben ueber die Bild-FFT hinaus, `visibleBinRange`
                // klemmt den Bin-Ausschnitt am letzten Bin ab, und der
                // geklemmte Rest (182,7 statt 200 kHz) wird ueber die
                // GANZE Breite gezogen. Das ergibt einen Maszstab von
                // 200/182,7 = 1,095 mit dem Drehpunkt am linken Rand —
                // gemessen 1,072, linke Haelfte 42 Punkte Versatz,
                // rechte 83. Genau die Form, die der Betreiber als
                // „nicht auf allen frequenzen gleich" beschreibt, und
                // genau der Fall „immer beim verschieben".
                //
                // Ein Prozent reicht: 2 kHz Rest ergeben einen
                // Maszstab von 1,01, also am Bildrand ein Kilohertz.
                const double breite = m_flexSpanHighHz - m_flexSpanLowHz;
                if (!(breite > 0.0)
                    || std::abs(mitte - panMitte) < breite * 0.01) {
                    return;
                }
                qCInfo(lcFlexGui).nospace()
                    << "Flex: Panadapter folgt dem Bild — " << (mitte / 1.0e6)
                    << " MHz (war " << (panMitte / 1.0e6) << ")";
                m_radioModel->zieheFlexPanadapterAuf(mitte);
            });
        }
        disconnect(m_flexBlickBand);
        if (SpectrumWidget* sw = m_radioModel->spectrumWidget()) {
            m_flexBlickBand = connect(sw, &SpectrumWidget::centerChanged,
                                      this, [this](double) {
                if (m_shuttingDown || m_flexTargetSliceId < 0) { return; }
                if (m_flexPanNachzug) { m_flexPanNachzug->start(); }
            });
        }

        // ── Die Maske geht von selbst zu (2026-10-22) ───────────────
        //
        // Der Selbstschluss haengt an `onConnectionStateChanged`, und
        // das kommt von der OpenHPSDR-Verbindung. Die Flex hat keine,
        // also blieb die Maske stehen, und hinter ihr lagen ALLE
        // schwebenden Fenster samt Panadapter. Der Betreiber:
        // „warum muss ich immer close klicken um auf das funkgeraet zu
        // kommen". Hier ist die Stelle, an der die Flex wirklich steht.
        QTimer::singleShot(1000, this, [this]() {
            if (m_shuttingDown) { return; }
            if (m_connectionPanel && m_connectionPanel->isVisible()
                && m_radioModel && m_radioModel->istIrgendwieVerbunden()) {
                qCInfo(lcFlexGui) << "Verbindungsmaske schliesst sich selbst";
                m_connectionPanel->accept();
            }
        });

        // ── Den Messwert-Umlauf anwerfen (2026-10-22) ────────────────
        //
        // Er wird sonst NUR aus `WdspEngine::initializedChanged`
        // gestartet, und auch dort nur, wenn `rxChannelForSlice(0)`
        // etwas liefert. Beim Flex gibt es diese Kette nicht — die
        // Maschine rechnet selbst —, also lief der Umlauf nie, und das
        // S-Meter blieb ohne Zeiger stehen, obwohl die Maschine den
        // Pegel dreissigmal in der Sekunde schickt.
        //
        // `start()` auf einem schon laufenden Zeitgeber ist folgenlos;
        // die fremde Quelle (MainWindow, setFremdeEmpfangsquelle) gibt
        // ohne Flex-Messwert `nullopt`, der WDSP-Weg bleibt also
        // unberuehrt.
        if (m_meterPoller) { m_meterPoller->start(); }
        if (m_radioModel->meterPoller() == nullptr && m_meterPoller) {
            m_radioModel->setMeterPoller(m_meterPoller);
        }

        // Der Panadapter zeigt ab jetzt die fremde Quelle; Bins vom
        // (nicht vorhandenen) Geraet werden damit verworfen.
        if (SpectrumWidget* sw = m_radioModel->spectrumWidget()) {
            sw->setKiwiDisplaySource(true);
        }
    });

    // ── Frequenz, beide Richtungen ──────────────────────────────────
    //
    // Bis zum 2026-10-22 ging hier gar nichts: `flexSliceFrequencyChanged`
    // wurde ausgesendet und von niemandem gehoert. Der VFO zeigte nie,
    // worauf die Maschine stand, und drehen liess sie sich nicht.
    //
    // Die Sicherheitsregel gilt auch hier, nicht nur fuer Ton und Bild:
    // gesteuert wird ausschliesslich ueber `flexControllableSlice()`,
    // also nie eine Scheibe mit echter DDC-Bindung
    // (`streamIndex() >= 0`). Der SunSDR-Durchgang vom 2026-08-24 wurde
    // genau deshalb nachgeschaerft — eine Fassung, die nur die Steuerung
    // absicherte, war zu wenig.
    connect(m_radioModel, &RadioModel::flexSliceFrequencyChanged, this,
            [this](int, double hz) {
        if (m_shuttingDown) { return; }
        SliceModel* slice = flexControllableSlice();
        if (!slice) { return; }
        // Von der Maschine nach innen: dabei die Wache setzen, sonst
        // schickt die Scheibe die eben empfangene Frequenz zurueck.
        m_flexApplyingRemoteState = true;
        slice->setFrequency(hz);
        m_flexApplyingRemoteState = false;
    });

    // ── Rückweg: die Maschine sagt, was wirklich an ist ─────────────
    //
    // Nötig, weil der Schalter auch an der Maschine selbst oder in
    // SmartSDR umgelegt werden kann — und weil beim Verbinden gilt,
    // was dort steht, nicht was im Fenster steht. Die Wache
    // `m_flexApplyingRemoteState` hält den Hinweg an, solange wir den
    // Zustand der Maschine übernehmen; sonst käme unser eigenes Echo
    // als neuer Befehl zurück.
    connect(m_radioModel, &RadioModel::flexSliceNoiseChanged, this,
            [this](bool nr, bool nb, bool anf) {
        if (m_shuttingDown) { return; }
        SliceModel* slice = flexControllableSlice();
        if (!slice) { return; }
        m_flexApplyingRemoteState = true;
        // AUS ist eindeutig, AN nicht: die Maschine nennt ihr
        // Verfahren nicht, und Longpath hat vier. Steht hier schon
        // eines, bleibt es stehen — nur der leere Fall bekommt eine
        // Marke, damit der Knopf überhaupt leuchtet. NR2 ist die, die
        // das RX-Feld auch sonst als Erstes anbietet.
        // Läuft ein Verfahren von Longpath (NR3, MNR), steht der
        // Schalter der Maschine mit Absicht auf 0 — ihr `nr=0` darf
        // die Wahl des Betreibers dann NICHT ausknipsen. Ohne diese
        // Ausnahme schaltete sich NR3 im selben Atemzug wieder ab, in
        // dem es eingeschaltet wurde: wir stellen die Maschine aus,
        // sie meldet aus, und die Meldung loeschte die Marke
        // (2026-10-28).
        const NrSlot steht = slice->activeNr();
        const bool eigenesVerfahren =
            (steht == NrSlot::NR3 || steht == NrSlot::MNR);
        if (!nr) {
            if (!eigenesVerfahren) { slice->setActiveNr(NrSlot::Off); }
        } else if (steht == NrSlot::Off) {
            slice->setActiveNr(NrSlot::NR2);
        }
        if (!nb) {
            slice->setNbMode(NbMode::Off);
        } else if (slice->nbMode() == NbMode::Off) {
            slice->setNbMode(NbMode::NB);
        }
        slice->setAnfEnabled(anf);
        m_flexApplyingRemoteState = false;
    });

    // ── Ton ─────────────────────────────────────────────────────────
    connect(m_radioModel, &RadioModel::flexAudioSamplesReady, this,
            [this](quint32, const QVector<float>& verschraenkt) {
        if (verschraenkt.isEmpty()) { return; }
        SliceModel* slice = flexControllableSlice();
        if (!slice) { return; }
        AudioEngine* ae = m_radioModel ? m_radioModel->audioEngine() : nullptr;
        if (!ae) { return; }
        // Zwei Werte je Rahmen (Stereo verschraenkt), gemessen.
        ae->feedFlexAudioData(slice->sliceIndex(), verschraenkt.constData(),
                              verschraenkt.size() / 2, kFlexAudioRateHz);
    });

    // ── Bild ────────────────────────────────────────────────────────
    //
    // Die FFT-Bins kommen schon in dBm (FlexVitaStream rechnet sie mit
    // dem Maszstab um, den RadioModel am Geraet gesetzt hat — die
    // Maschine meldet `y_pixels` nie zurueck). Der Ausschnitt steht
    // NICHT im FFT-Paket: nur die Wasserfall-Kachel traegt ihre
    // Frequenzen. Bis eine Zeile durch ist, gibt es also keinen
    // Ausschnitt, und dann wird nichts gezeichnet — lieber kein Bild
    // als ein Bild an der falschen Frequenz.
    // ── Der Ausschnitt kommt vom Panadapter der Maschine ────────────
    //
    // Bis zum 2026-10-22 kam er aus der WASSERFALLKACHEL, und die hat
    // ihre eigene Bin-Zahl und ihren eigenen Rand: am Geraet gemessen
    // 230,6 kHz, waehrend der Panadapter 200 kHz breit war. Ueber den
    // falschen Ausschnitt gespannt steht jedes Signal bis zu 15 kHz
    // neben seiner Frequenz.
    connect(m_radioModel, &RadioModel::flexAusschnittGeaendert, this,
            [this](double tiefHz, double hochHz) {
        if (m_shuttingDown || !(hochHz > tiefHz)) { return; }
        m_flexSpanLowHz  = tiefHz;
        m_flexSpanHighHz = hochHz;

        // Das Fenster folgt dem Ausschnitt der Maschine — auch wenn er
        // sich spaeter aendert (Bandwechsel, Zoom am Geraet, oder weil
        // `tuneFlexSlice` den Panadapter nachgezogen hat).
        if (SpectrumWidget* sw = m_radioModel
                ? m_radioModel->spectrumWidget() : nullptr) {
            const double mitte  = (tiefHz + hochHz) * 0.5;
            const double breite = hochHz - tiefHz;

            // ── Den Zoom des Betreibers nicht wegwerfen (2026-10-23) ──
            //
            // Hier stand `setFrequencyRange(mitte, breite)` immer. Die
            // Maschine schickt ihren Ausschnitt aber auch dann neu,
            // wenn sich nur die MITTE geaendert hat — nach jedem
            // Abstimmen, weil `tuneFlexSlice` den Panadapter nachzieht.
            // Damit sprang das Fenster bei jedem Doppelklick im
            // Panadapter auf die volle Breite zurueck. Betreiber:
            // „verkleinert sich das display und ich muss wieder mit
            // plus vergroeszern".
            //
            // Also: Breite nur uebernehmen, wenn die Maschine WIRKLICH
            // eine andere schickt (oder beim ersten Mal). Sonst nur um
            // dieselbe Strecke mitwandern und die eigene Breite
            // behalten.
            const double alteBreite = m_flexFensterBreiteHz;
            if (!m_flexAusschnittUebernommen
                || alteBreite <= 0.0
                || std::abs(breite - alteBreite) > 1.0) {
                sw->setFrequencyRange(mitte, breite);
            }
            // Sonst: NICHTS. Eine reine Mittenverschiebung der
            // Maschine kommt fast immer davon, dass der Betreiber
            // gerade selbst abgestimmt hat — ihm das Fenster
            // hinterherzuziehen hiesze, seinen Zoom wegzuwerfen UND
            // (ohne CTUN, wo die Fenstermitte die Frequenz IST) ihn
            // ein zweites Mal zu verstimmen. Im Protokoll stand das
            // als Kette von sieben `tuneFlexSlice` in 150 ms.
            m_flexFensterMitteHz  = mitte;
            m_flexFensterBreiteHz = breite;

            if (!m_flexAusschnittUebernommen) {
                m_flexAusschnittUebernommen = true;
                if (!sw->waterfallNFAGCEnabled()) {
                    sw->setWaterfallNFAGCEnabled(true);
                }
            }
            qCInfo(lcFlexGui).nospace()
                << "Flex-Bild: Ausschnitt der Maschine uebernommen — "
                << (breite / 1.0e3) << " kHz um " << (mitte / 1.0e6)
                << " MHz";
        }
    });

    // ── Der Wasserfall kommt von der Maschine (2026-10-23) ──────────
    //
    // Die FLEX-8400 fuehrt ZWEI Bildstroeme, und AetherSDR zeichnet
    // auch aus beiden (docs/protocols/flex-wasserfall-aethersdr.md):
    //   0x8003  Spektrum, Bildzeilen -> dBm ueber y_pixels
    //   0x8004  Wasserfall, qint16/128 -> INTENSITAET, mit eigenem
    //           Frequenzbereich und eigenem Takt (`LineDurationMS`)
    //
    // Longpath zog bisher nur den ersten und baute den Wasserfall aus
    // den FFT-Bins nach. Der Betreiber am 2026-10-22: „das rot ist
    // eigenartig gewesen, auch die zeitliche laenge, bis der
    // wasserfall anfaengt". Beides folgt aus fremdem Takt und fremden
    // Schwellen.
    //
    // `FlexVita::tileColourValue()` gab es in Longpath schon —
    // geschrieben, nie benutzt.
    connect(m_radioModel, &RadioModel::flexWaterfallRowReady, this,
            [this](quint32, const QVector<quint16>& roh,
                   double lowFreqMhz, double highFreqMhz, quint32) {
        if (m_shuttingDown || roh.isEmpty()) { return; }
        if (!(highFreqMhz > lowFreqMhz)) { return; }
        SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget()
                                          : nullptr;
        if (!sw || !sw->kiwiDisplaySource()) { return; }

        QVector<float> intensitaet(roh.size());
        for (int i = 0; i < roh.size(); ++i) {
            intensitaet[i] = FlexVita::tileColourValue(roh[i]);
        }
        sw->zeigeFremdeWasserfallzeile(intensitaet, lowFreqMhz * 1.0e6,
                                       highFreqMhz * 1.0e6);

        // ── KEIN Nachziehen der Mitte (2026-10-23, zurueckgebaut) ────
        //
        // Hier stand ein Nachzug der Panadaptermitte auf die
        // Kachelmitte. Er hat den gemessenen Versatz von 7,4 kHz
        // scheinbar geheilt (0 bis 1 Punkt auf 15 m) — aber nur,
        // solange er AUCH das Fenster mitzog. Ohne das sprang der
        // Versatz auf +106 Punkte = +18,7 kHz, also schlechter als
        // vorher. Und das Fenster darf er nicht anfassen: ohne CTUN
        // ist die Fenstermitte die Empfangsfrequenz, und der Nachzug
        // hat den Betreiber nachweislich verstimmt (sieben
        // `tuneFlexSlice` in 150 ms).
        //
        // Damit ist die Annahme widerlegt, der Zustandsschub der
        // Maschine sei schuld: die Bild-FFT deckt den Ausschnitt des
        // ZUSTANDS, nicht den der Kachel. Was gemessen wurde, war
        // etwas anderes — und die Messung selbst ist nicht zuverlaessig
        // genug fuer so einen Schluss: dieselbe Lage ergab im Abstand
        // von 30 s einmal -8 und einmal +107 Punkte. Auf einem Band
        // mit vielen aehnlichen Signalen rastet die Kreuzkorrelation
        // auf dem falschen Nachbarn ein.
        //
        // Die Messung bleibt im Protokoll, aber sie meldet jetzt auch,
        // wie DEUTLICH ihr bestes Ergebnis ist; erst daran laesst sich
        // ein echter Versatz von einem Fehlgriff unterscheiden.
    });

    connect(m_radioModel, &RadioModel::flexSpectrumReady, this,
            [this](quint32, const QVector<float>& dbm) {
        if (m_shuttingDown || dbm.isEmpty()) { return; }

        // 2026-10-21: bis hierher war jeder Befund belegt — die Pakete
        // kommen an, die Rahmen entstehen, der Maszstab steht, die
        // Maschine hat `display pan set ... 1024 x 1000` mit Code 0
        // quittiert. Gezeichnet wurde trotzdem nichts, und WELCHES der
        // drei Tore unten zuschlaegt, war von auszen nicht zu sehen.
        // Also sagt es das Protokoll — einmal je Grund, nicht je
        // Rahmen (es kommen 25 in der Sekunde).
        const auto meldeEinmal = [this](FlexBildGrund grund, const char* text) {
            if (m_flexBildGrund == grund) { return; }
            m_flexBildGrund = grund;
            qCInfo(lcFlexGui) << "Flex-Bild:" << text;
        };

        if (m_flexSpanHighHz <= m_flexSpanLowHz) {
            meldeEinmal(FlexBildGrund::KeinAusschnitt,
                        "kein Ausschnitt — es kam noch keine Wasserfallzeile "
                        "mit Frequenzen, also waere jedes Bild an der "
                        "falschen Stelle");
            return;
        }
        SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget() : nullptr;
        if (!sw) {
            meldeEinmal(FlexBildGrund::KeinFenster,
                        "kein Panadapter-Fenster da (RadioModel::spectrumWidget "
                        "ist leer) — es gibt nichts, worauf gezeichnet wird");
            return;
        }
        if (!sw->kiwiDisplaySource()) {
            meldeEinmal(FlexBildGrund::FremdquelleAus,
                        "das Fenster nimmt keine fremde Quelle an "
                        "(kiwiDisplaySource ist aus)");
            return;
        }
        if (m_flexBildGrund != FlexBildGrund::Zeichnet) {
            // Einmal, beim ersten gezeichneten Rahmen: WOHIN gemalt
            // wird. „Wird gezeichnet" und „ist zu sehen" sind zweierlei
            // — ein unsichtbares Fenster, eine Flaeche der Groesze null
            // oder ein Ausschnitt neben den Daten sehen alle gleich aus,
            // naemlich schwarz.
            const auto mhz = [](double hz) { return hz / 1.0e6; };
            qCInfo(lcFlexGui).nospace()
                << "Flex-Bild: " << dbm.size() << " Bins, Ausschnitt "
                << mhz(m_flexSpanLowHz) << "–" << mhz(m_flexSpanHighHz)
                << " MHz; Fenster zeigt " << mhz(sw->centerFrequency())
                << " MHz Mitte, " << mhz(sw->bandwidth()) << " MHz breit; "
                << "sichtbar=" << sw->isVisible()
                << " Groesze=" << sw->width() << "x" << sw->height();

            // Die Elternkette, wenn die Flaeche unsichtbar ist.
            //
            // `isVisible()` ist falsch, sobald IRGENDEIN Vorfahr
            // versteckt ist — die Flaeche selbst kann dabei tadellos
            // dastehen. Welche Ebene zu ist, sieht man nur, wenn man
            // die Kette hinauflaeuft; am 2026-10-21 hat das Raten
            // daran eine Stunde gekostet (memory
            // longpath-pruefstand-fensterbreite-xvfb sagt dasselbe:
            // Elternkette melden, nicht die Endzahl).
            if (!sw->isVisible()) {
                int ebene = 0;
                for (QWidget* w = sw; w; w = w->parentWidget(), ++ebene) {
                    qCInfo(lcFlexGui).nospace()
                        << "Flex-Bild: Elternkette " << ebene << ": "
                        << w->metaObject()->className()
                        << " \"" << w->objectName() << "\""
                        << " sichtbar=" << w->isVisible()
                        << " versteckt=" << w->isHidden()
                        << " Fenster=" << w->isWindow()
                        << " Lage=" << w->geometry();
                }
            }
        }
        meldeEinmal(FlexBildGrund::Zeichnet, "wird gezeichnet");

        // ── Clarity muss diese Bins auch sehen (2026-10-22) ──────────
        //
        // Clarity bekam seine Bins bis heute AUSSCHLIESZLICH aus
        // `primaryFftEngine()`, also aus dem eigenen Funkgeraet. Bei
        // der Flex gibt es die nicht — sie rechnet selbst. Clarity war
        // damit eingeschaltet (`ClarityEnabled=True` in Martins
        // Profil) und ohne Boden, und `composeWaterfallActiveThresholds`
        // liesz die Schwellen auf dem Stand stehen, den das letzte
        // eigene Geraet hinterlassen hatte.
        //
        // Folge am Geraet: der Wasserfall war durchgehend EINFARBIG
        // ROT — Rot ist bei Farbschema 0 die oberste Palettenstufe,
        // also der Anschlag. Dieselbe Fehlerklasse ist in
        // `composeWaterfallActiveThresholds` schon beschrieben
        // („an einer SunSDR2 QRP ohne Antenne … der Wasserfall wurde
        // ganz rot"), nur kam sie dort von einem FALSCHEN Boden und
        // hier von gar keinem.
        if (m_clarityController) {
            m_clarityController->feedBins(dbm);
        }

        sw->updateKiwiSpectrumDbm(dbm, m_flexSpanLowHz, m_flexSpanHighHz);
    });

    // ── Aufraeumen beim Trennen ─────────────────────────────────────
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState st) {
        if (st == ConnectionState::Connected || m_shuttingDown) { return; }
        if (m_flexTargetSliceId < 0) { return; }
        if (AudioEngine* ae = m_radioModel ? m_radioModel->audioEngine() : nullptr) {
            // Den Wandler mit wegwerfen: ein alter Filterzustand auf
            // einem neuen Empfaenger gibt beim ersten Block ein Knacken.
            ae->removeFlexAudioSource(m_flexTargetSliceId);
        }
        if (SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget() : nullptr) {
            sw->setKiwiDisplaySource(false);
        }
        qCInfo(lcFlexGui) << "Flex-Ton von Scheibe" << m_flexTargetSliceId
                          << "geloest";
        // Nur die eigene wieder wegraeumen. Eine Scheibe, die der
        // Betreiber selbst hatte, bleibt stehen.
        if (m_flexSliceWasCreated && m_radioModel) {
            m_radioModel->removeSlice(m_flexTargetSliceId);
        }
        // Senden abbauen (Stufe 2). Die Baender bleiben haengen, ihre
        // Lambdas kehren aber ohne Zielscheibe sofort um; die Pumpe
        // muss ausdruecklich stehen, sonst zieht sie weiter Mikrofonton
        // und schickt ihn an eine Kennung, die es nicht mehr gibt.
        if (m_flexSendePumpe) { m_flexSendePumpe->stop(); }
        m_flexSelbstGetastet = false;

        m_flexSliceWasCreated = false;
        m_flexAusschnittUebernommen = false;
        m_flexTargetSliceId = -1;
        m_flexSpanLowHz = 0.0;
        m_flexSpanHighHz = 0.0;
    });
}

// ── Senden ueber die Flex (2026-10-23, Stufe 2) ──────────────────────
//
// Bis heute stand im Kopf von FlexRadioConnection.h: „Was Stufe 1
// trotzdem NICHT tut: senden." Hier haengt es jetzt zusammen — und
// zwar so, dass nichts davon von selbst tastet.
//
// Was anders ist als bei einem OpenHPSDR-Geraet: dort rechnet Longpath
// das Sendesignal (WDSP, TxWorkerThread, I/Q hinaus). Die Flex rechnet
// selbst. Es geht also KEIN fertiges Signal hinaus, sondern roher
// Mikrofonton, und die Maschine macht daraus SSB. Deshalb haengt hier
// auch nichts am TX-Kanal, sondern direkt an `AudioEngine::pullTxMic`.
void MainWindow::verdrahteFlexSenden()
{
    if (!m_radioModel) { return; }

    // ── Die Mikrofonpumpe ────────────────────────────────────────────
    //
    // 5 ms, wie die Pumpe des OpenHPSDR-Wegs (TxWorkerThread). Bei
    // 48 kHz sind das 240 Rahmen je Durchgang; die Maschine will
    // Pakete zu 128 Rahmen, der Rest bleibt in RadioModel liegen und
    // geht beim naechsten Mal mit.
    if (!m_flexSendePumpe) {
        m_flexSendePumpe = new QTimer(this);
        m_flexSendePumpe->setInterval(5);
        m_flexSendePumpe->setTimerType(Qt::PreciseTimer);
        connect(m_flexSendePumpe, &QTimer::timeout, this, [this]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            AudioEngine* ae = m_radioModel->audioEngine();
            if (!ae) { return; }

            constexpr int kRahmen = 480;   // 10 ms Vorrat bei 48 kHz
            m_flexMikMono.resize(kRahmen);
            const int geholt = ae->pullTxMic(m_flexMikMono.data(), kRahmen);
            if (geholt <= 0) { return; }

            // Mono hinein, Stereo hinaus: die Maschine erwartet zwei
            // Kanaele je Rahmen (float32, Big-Endian im Paket).
            m_flexMikStereo.resize(geholt * 2);
            for (int i = 0; i < geholt; ++i) {
                m_flexMikStereo[i * 2]     = m_flexMikMono[i];
                m_flexMikStereo[i * 2 + 1] = m_flexMikMono[i];
            }
            m_radioModel->sendeFlexMikrofonton(m_flexMikStereo.constData(),
                                               geholt);
        });
    }

    // ── MOX ──────────────────────────────────────────────────────────
    //
    // Reihenfolge: erst den Senderzweig auf DAX, dann tasten. Umgekehrt
    // sendet die erste Zehntelsekunde das Mikrofon an der Maschine —
    // oder Stille, wenn dort keines steckt.
    //
    // `dax=0` wird beim Loesen NICHT geschickt. Die Vorlage sagt
    // ausdruecklich warum (AetherSDR TciServer.cpp:3414 [@d58e2b8a]):
    // der Zustandsschub der Maschine kommt verzoegert, und ein
    // sofortiges Zuruecksetzen kreuzt ihn.
    disconnect(m_flexMoxBand);
    m_flexMoxBand = connect(&m_radioModel->transmitModel(),
                            &TransmitModel::moxChanged, this,
                            [this](bool an) {
        qCInfo(lcFlexGui).nospace()
            << "MOX gedrueckt: " << (an ? "an" : "aus")
            << ", Zielscheibe " << m_flexTargetSliceId;
        if (m_shuttingDown || !m_radioModel) { return; }
        if (m_flexTargetSliceId < 0) {
            qCWarning(lcFlexGui) << "MOX verworfen: keine Flex-Zielscheibe";
            return;
        }

        // ── Nicht waehrend des Abstimmens tasten (2026-10-27) ────
        //
        // `transmit tune 1` laesst die Maschine ihren eigenen Traeger
        // machen und tastet sich dabei selbst. Kaeme jetzt noch
        // `xmit 1` samt Mikrofonpumpe dazu, lagen Traeger und Sprache
        // gleichzeitig am Sender. Wer TUNE drueckt, will den Traeger.
        if (an && m_radioModel->transmitModel().isTune()) {
            qCInfo(lcFlexGui) << "MOX waehrend TUNE ignoriert";
            return;
        }

        if (an) {
            m_radioModel->sendeFlexBefehl(
                FlexCommandWire::setDaxRouteCommand(true));
            m_flexSelbstGetastet = m_radioModel->flexTasten(true);
            if (m_flexSelbstGetastet && m_flexSendePumpe) {
                m_flexSendePumpe->start();
            } else {
                // Die Bedienung zeigt sonst „sendet", waehrend nichts
                // hinausgeht. Der Grund steht im Protokoll.
                m_radioModel->transmitModel().setMox(false);
            }
        } else {
            if (m_flexSendePumpe) { m_flexSendePumpe->stop(); }
            m_radioModel->flexTasten(false);
            m_flexSelbstGetastet = false;
        }
    });

    // ── TUNE ─────────────────────────────────────────────────────────
    //
    // Der Abstimmtraeger der Maschine, nicht unserer: er taktet mit
    // `tunepower` und braucht weder Mikrofon noch DAX.
    disconnect(m_flexTuneBand);
    m_flexTuneBand = connect(&m_radioModel->transmitModel(),
                             &TransmitModel::tuneChanged, this,
                             [this](bool an) {
        qCInfo(lcFlexGui).nospace()
            << "TUNE gedrueckt: " << (an ? "an" : "aus")
            << ", Zielscheibe " << m_flexTargetSliceId
            << ", Abbau " << m_shuttingDown;
        if (m_shuttingDown || !m_radioModel) { return; }
        if (m_flexTargetSliceId < 0) {
            qCWarning(lcFlexGui) << "TUNE verworfen: keine Flex-Zielscheibe";
            return;
        }
        if (an && !m_radioModel->flexAbstimmen(true)) {
            m_radioModel->transmitModel().setTune(false);
        } else if (!an) {
            m_radioModel->flexAbstimmen(false);
        }
    });

    // ── Leistung ─────────────────────────────────────────────────────
    //
    // Die Maschine will Prozent, Longpath fuehrt Watt. Bei einer
    // FLEX-8400 (100 W) ist das dieselbe Zahl; bei einem anderen Geraet
    // waere es das nicht, und dann gehoert hier eine Umrechnung hin.
    disconnect(m_flexLeistungBand);
    m_flexLeistungBand = connect(&m_radioModel->transmitModel(),
                                 &TransmitModel::powerChanged, this,
                                 [this](int watt) {
        if (m_shuttingDown || !m_radioModel || m_flexTargetSliceId < 0) {
            return;
        }
        m_radioModel->setzeFlexSendeleistung(qBound(0, watt, 100));
    });

    disconnect(m_flexAbstimmleistungBand);
    m_flexAbstimmleistungBand =
        connect(&m_radioModel->transmitModel(),
                &TransmitModel::tunePowerChanged, this,
                [this](int watt) {
        if (m_shuttingDown || !m_radioModel || m_flexTargetSliceId < 0) {
            return;
        }
        m_radioModel->setzeFlexAbstimmleistung(qBound(0, watt, 100));
    });

    // ── Was die Maschine selbst meldet ───────────────────────────────
    //
    // Sie sendet auch, wenn jemand die Taste am Mikrofon drueckt oder
    // der Fusschalter betaetigt wird. Dann haben WIR nicht getastet,
    // und die Mikrofonpumpe darf nicht laufen — sonst mischt sich
    // unser Ton in seinen.
    disconnect(m_flexSendetBand);
    m_flexSendetBand = connect(m_radioModel, &RadioModel::flexSendetGeaendert,
                               this, [this](bool sendet) {
        if (m_shuttingDown || !m_radioModel) { return; }
        if (!sendet && m_flexSendePumpe) { m_flexSendePumpe->stop(); }
        qCInfo(lcFlexGui).nospace()
            << "Flex " << (sendet ? "SENDET" : "empfaengt")
            << (m_flexSelbstGetastet ? " (von uns getastet)"
                                     : " (nicht von uns)");
    });

    qCInfo(lcFlexGui) << "Flex-Senden verdrahtet — es wurde nichts getastet";
}

}  // namespace Longpath
