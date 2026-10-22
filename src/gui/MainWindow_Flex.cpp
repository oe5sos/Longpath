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

        // Und einmal gleich jetzt, damit die Maschine mit dem
        // uebereinstimmt, was im Fenster steht — sonst gilt der
        // Unterschied bis zum ersten Drehen weiter.
        m_radioModel->setFlexSliceMode(ziel->dspMode());
        m_radioModel->setFlexSliceFilter(ziel->filterLow(), ziel->filterHigh());
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
            sw->setFrequencyRange(mitte, breite);
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

    // Die Wasserfallkachel traegt die Zeilen; ihr Frequenzbereich wird
    // NICHT mehr gelesen (siehe `flexAusschnittGeaendert` oben). Sie
    // hat ihre eigene Bin-Zahl und ihren eigenen Rand — gemessen 230,6
    // kHz gegen 200 kHz Panadapterbreite, also bis zu 15 kHz Versatz
    // fuer jedes Signal.

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
        m_flexSliceWasCreated = false;
        m_flexAusschnittUebernommen = false;
        m_flexTargetSliceId = -1;
        m_flexSpanLowHz = 0.0;
        m_flexSpanHighHz = 0.0;
    });
}

}  // namespace Longpath
