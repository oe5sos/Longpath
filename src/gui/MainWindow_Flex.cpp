// =================================================================
// src/gui/MainWindow_Flex.cpp  (Longpath)
// =================================================================
//
// Flex-Treiber Stufe 2: Ton und Bild ins Fenster.
//
// Longpath-original. Der Zuschnitt folgt den zwei fremden Quellen, die
// es schon gibt — MainWindow_KiwiSdr.cpp (875 Zeilen) und
// MainWindow_SunSdr.cpp (849) — und ist deutlich kleiner als beide,
// weil ein Flex kein Profil-Verwaltungswerk braucht: es gibt genau
// eine Maschine, und ihre Stroeme tragen ihre Kennung im Kopf.
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
            qCWarning(lcFlexGui) << "keine freie Scheibe fuer den Flex-Ton — "
                                    "Bild laeuft, Ton nicht";
            return;
        }

        m_flexTargetSliceId = ziel->sliceIndex();
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
    });

    connect(m_radioModel, &RadioModel::flexSpectrumReady, this,
            [this](quint32, const QVector<float>& dbm) {
        if (m_shuttingDown || dbm.isEmpty()) { return; }
        if (m_flexSpanHighHz <= m_flexSpanLowHz) {
            return;   // noch kein Ausschnitt bekannt (siehe oben)
        }
        SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget() : nullptr;
        if (!sw || !sw->kiwiDisplaySource()) { return; }
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
        m_flexTargetSliceId = -1;
        m_flexSpanLowHz = 0.0;
        m_flexSpanHighHz = 0.0;
    });
}

}  // namespace Longpath
