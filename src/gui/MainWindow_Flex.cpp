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

        // Von innen nach aussen. Erst HIER verdrahtet, weil es die
        // Zielscheibe vorher nicht gibt; `UniqueConnection`, damit ein
        // zweites Anmelden nicht doppelt sendet.
        connect(ziel, &SliceModel::frequencyChanged, this,
                [this](double hz) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->tuneFlexSlice(hz);
        }, Qt::UniqueConnection);
        if (AudioEngine* ae = m_radioModel->audioEngine()) {
            // Opportunistisch im Mix: ein Netzaussetzer zum Flex darf
            // nicht den ganzen Ton anhalten.
            ae->setFlexAudioSourceEnabled(m_flexTargetSliceId, true);
        }
        qCInfo(lcFlexGui) << "Flex-Ton auf Scheibe" << m_flexTargetSliceId;

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
    connect(m_radioModel, &RadioModel::flexWaterfallRowReady, this,
            [this](quint32, const QVector<quint16>&,
                   double lowFreqMhz, double highFreqMhz, quint32) {
        if (m_shuttingDown) { return; }
        m_flexSpanLowHz  = lowFreqMhz * 1.0e6;
        m_flexSpanHighHz = highFreqMhz * 1.0e6;

        // Einmal je Verbindung die Breite der fremden Quelle
        // uebernehmen.
        //
        // 2026-10-22 am Geraet: die Flex lieferte 13,8692–14,3304 MHz,
        // also 461 kHz — und das Fenster stand auf 48 kHz. Der
        // Betreiber sah ein Zehntel des Ausschnitts, stark gezoomt,
        // und darin war nichts. Die Daten trugen 22 dB Hub.
        //
        // NUR einmal: danach gehoert der Zoom wieder ihm. Wer
        // hineinzoomt, soll nicht bei der naechsten Wasserfallzeile
        // wieder herausgerissen werden.
        if (!m_flexAusschnittUebernommen
            && m_flexSpanHighHz > m_flexSpanLowHz) {
            if (SpectrumWidget* sw = m_radioModel
                    ? m_radioModel->spectrumWidget() : nullptr) {
                m_flexAusschnittUebernommen = true;
                const double mitte  = (m_flexSpanLowHz + m_flexSpanHighHz) * 0.5;
                const double breite = m_flexSpanHighHz - m_flexSpanLowHz;
                sw->setFrequencyRange(mitte, breite);

                // Und die Wasserfallschwellen dem Rauschteppich
                // nachfuehren lassen.
                //
                // Die gespeicherten Schwellen stammen vom eigenen
                // Geraet; die Flex liefert ganz andere Pegel, und
                // dann faerbt der Wasserfall alles in einer Farbe —
                // am 2026-10-22 durchgehend rot. Die NF-AGC ist genau
                // dafuer gebaut. Sie wird gemeldet, nicht heimlich
                // gesetzt: es ist eine Anzeigeeinstellung des
                // Betreibers, und er soll wissen, dass sie an ist.
                if (!sw->waterfallNFAGCEnabled()) {
                    sw->setWaterfallNFAGCEnabled(true);
                    qCInfo(lcFlexGui)
                        << "Flex-Bild: Wasserfall-Automatik (NF-AGC) "
                           "eingeschaltet — die gespeicherten Schwellen "
                           "passen nicht zu den Pegeln der Flex";
                }
                qCInfo(lcFlexGui).nospace()
                    << "Flex-Bild: Fensterbreite auf den Ausschnitt gesetzt — "
                    << (breite / 1.0e3) << " kHz um "
                    << (mitte / 1.0e6) << " MHz";
            }
        }
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
