// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. "Wo werde ich gehoert" (Entwurf B + C,
// 2026-11-25): bis zu fuenf Kiwis hoeren beim Senden mit.
//
// ── Warum Mithoerer KEINE Scheibe bekommen ──────────────────────────
//
// Der Kiwi von "Mein Signal" haengt (wie jeder Kiwi mit Ton) an einer
// Scheibe, und die Scheibe haengt an der Sicherheitsschranke
// kiwiControllableSlice. Fuenf Kiwis gleichzeitig an fuenf Scheiben
// haetten fuenf neue VFOs im Bild, fuenf Toene in der Mischung und fuenf
// Stellen, an denen die Schranke gelten muss. Ein Mithoerer braucht davon
// nichts: er soll nur ein Bild liefern. Er wird darum ueber
// KiwiSdrManager::setProfileListener verbunden (ohne Zuordnung, ohne Ton)
// und bekommt Frequenz und Ansicht ueber primeProfileTracking.
// Folge: dieser Weg kann keine Scheibe fuettern und kein Funkgeraet
// beruehren -- er schreibt nur an den Kiwi selbst.
//
// ── Hoeflichkeit ────────────────────────────────────────────────────
//
// Je Mithoerer EIN Kanal beim Betreiber, kein Dauerpolling des
// Verzeichnisses (es wird nur auf Klick geholt), hoechstens fuenf Kiwis
// insgesamt. Nichts wird gespeichert oder beim Start verbunden.

#include "gui/MainWindow.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/KiwiGroup.h"
#include "core/KiwiOwnSignal.h"
#include "core/KiwiRecording.h"
#include "core/Maidenhead.h"
#include "core/KiwiSdrManager.h"
#include "core/LogCategories.h"
#include "gui/KiwiHearingDialog.h"
#include "gui/KiwiWaterfallPanel.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/KiwiOwnSignalStrip.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/KiwiWaterfallStripWidget.h"
#include "gui/KiwiPublicReceiverPicker.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QPointer>
#include <QGuiApplication>
#include <QTimer>

#include <algorithm>

namespace Longpath {

namespace {
// Das KIWI-WASSERFAELLE-Fenster nach vorn holen und auf den Schirm zwingen:
// "setVisible(true)" tut nichts, wenn es schon offen ist, aber hinter
// anderen Fenstern oder winzig liegt (Betreiber 2026-11-25: "ich sehe keine
// am Bildschirm", "wenn ich klicke geht kein Fenster auf").
void kiwiFensterNachVorn(QWidget* panel, QWidget* haupt)
{
    if (!panel) { return; }
    QWidget* w = panel->window();
    if (!w || w == haupt) { return; }
    w->show();
    const QSize mind = panel->minimumSizeHint();
    if (w->width() < mind.width() || w->height() < mind.height()) {
        w->resize(qMax(w->width(), mind.width() + 4), qMax(w->height(), mind.height() + 30));
    }
    if (haupt && !haupt->frameGeometry().intersects(w->frameGeometry())) {
        const QPoint c = haupt->frameGeometry().center();
        w->move(c.x() - w->width() / 2, c.y() - w->height() / 2);
    }
    w->raise();
    w->activateWindow();
}
}  // namespace


namespace {
// Die Kennungen, mit denen Mithoerer beim Client "verfolgt" werden. Der
// Client braucht nur eine nicht-negative Zahl; eine Scheibe dazu gibt es
// nicht (und darf es nicht geben).
constexpr int kListenerTrackingBase = 900000;
constexpr double kMinViewHz = 10000.0;
constexpr double kMaxViewHz = 200000.0;
}

void MainWindow::wireKiwiGroup()
{
    if (!m_kiwiSdrManager || !m_kiwiSdrApplet) { return; }

    connect(m_kiwiSdrApplet, &KiwiSdrApplet::hearingRequested, this,
            [this]() { openKiwiHearingDialog(); });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::soloToggled, this,
            [this](bool on) { setKiwiSolo(on); });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::radioMuteToggled, this,
            [this](bool on) { setRadioMuted(on); });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::soundRequested, this,
            [this](const QString& id, bool on) { setKiwiSound(id, on); });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::volumeChanged, this,
            [this](const QString& id, int db) { setKiwiVolumeDb(id, db); });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::openWindowRequested, this,
            [this](const QString& id) {
        if (!m_kiwiSdrManager || !m_kiwiSdrManager->hasProfile(id)) { return; }
        m_kiwiSdrManager->setWaterfallPreviewEnabled(id, true);
        if (m_appletVis) { m_appletVis->ensureVisible(QStringLiteral("KiwiWaterfalls")); }
        kiwiFensterNachVorn(m_kiwiWaterfallPanel, this);
    });
    m_kiwiSoloTimer = new QTimer(this);
    m_kiwiSoloTimer->setSingleShot(true);
    m_kiwiSoloTimer->setInterval(30000);
    connect(m_kiwiSoloTimer, &QTimer::timeout, this, [this]() { setKiwiSolo(false); });
    // Solo ohne Ton waere Stille: faellt der Kiwi aus (Fehler, getrennt), geht
    // Solo von selbst aus und die Flex ist wieder da.
    connect(m_kiwiSdrManager, &KiwiSdrManager::profileStateChanged, this,
            [this](const QString& id, KiwiSdrClient::State st, const QString&) {
        if (m_kiwiSoloOn && id == m_kiwiOwnProfileId
            && (st == KiwiSdrClient::State::Error
                || st == KiwiSdrClient::State::Disconnected)) {
            setKiwiSolo(false);
            showToast(tr("Solo off: the Kiwi stopped delivering audio."),
                      ToastSeverity::Warning, 8000);
        }
    });
    // "Radio stumm" ohne einen Kiwi, der liefert, waere nur Stille: dann ist
    // das Funkgeraet wieder zu hoeren.
    connect(m_kiwiSdrManager, &KiwiSdrManager::profileStateChanged, this,
            [this](const QString&, KiwiSdrClient::State st, const QString&) {
        if (!m_radioMuted || m_kiwiReplayMutedRadio
            || (st != KiwiSdrClient::State::Error
                && st != KiwiSdrClient::State::Busy
                && st != KiwiSdrClient::State::Disconnected)) {
            return;
        }
        for (const auto& pr : m_kiwiSdrManager->profiles()) {
            const KiwiSdrClient::State other = m_kiwiSdrManager->state(pr.id);
            if (m_kiwiSdrManager->assignedSliceForProfile(pr.id) >= 0
                && !m_kiwiUserMuted.contains(pr.id)
                && (KiwiSdrClient::stateHasReceiveAudio(other)
                    || other == KiwiSdrClient::State::Connecting)) {
                return;
            }
        }
        setRadioMuted(false);
        showToast(tr("Radio sound is on again: no Kiwi is delivering audio."),
                  ToastSeverity::Warning, 8000);
    });
    // Fiel die Scheibe des Kiwi weg, ist Solo aus -- sonst bliebe alles stumm.
    connect(m_kiwiSdrManager, &KiwiSdrManager::sliceAssignmentChanged, this,
            [this](int, const QString&) {
        if (m_kiwiSoloOn
            && !kiwiControllableSlice(
                   m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId))) {
            setKiwiSolo(false);
        }
    });

    // Ton je Kiwi (Schalter im Streifen der KIWI-WASSERFAELLE).
    if (m_kiwiWaterfallPanel) {
        // Ein leeres Fenster braucht niemand (Betreiber 2026-11-25): es geht
        // mit dem letzten Streifen zu und mit dem ersten auf.
        connect(m_kiwiWaterfallPanel, &KiwiWaterfallPanel::emptyChanged, this,
                [this](bool empty) {
            if (m_appletVis) {
                if (empty) { m_appletVis->setVisible(QStringLiteral("KiwiWaterfalls"), false); }
                else { m_appletVis->ensureVisible(QStringLiteral("KiwiWaterfalls")); }
            }
            if (!empty) { kiwiFensterNachVorn(m_kiwiWaterfallPanel, this); }
        });
        QTimer::singleShot(0, this, [this]() {
            refreshKiwiSoundButtons();
            if (m_appletVis && m_kiwiWaterfallPanel
                && m_kiwiWaterfallPanel->stripForTest(QString()) == nullptr
                && m_kiwiSdrManager) {
                bool any = false;
                for (const KiwiSdrAntennaProfile& pr : m_kiwiSdrManager->profiles()) {
                    if (pr.waterfallPreviewEnabled) { any = true; break; }
                }
                if (!any) { m_appletVis->setVisible(QStringLiteral("KiwiWaterfalls"), false); }
            }
        });
        connect(m_kiwiWaterfallPanel, &KiwiWaterfallPanel::soundToggled, this,
                [this](const QString& id, bool on) { setKiwiSound(id, on); });
        connect(m_kiwiWaterfallPanel, &KiwiWaterfallPanel::zoomRequested, this,
                [this](const QString& id, int dir) { zoomKiwiStrip(id, dir); });
        connect(m_kiwiWaterfallPanel, &KiwiWaterfallPanel::chooseOtherRequested, this,
                [this](const QString& id) {
            if (id == m_kiwiOwnProfileId) { chooseOtherKiwi(); }
            else { addKiwiToGroupFromPicker(id); }
        });
        connect(m_kiwiSdrManager, &KiwiSdrManager::sliceAssignmentChanged, this,
                [this](int, const QString&) { refreshKiwiSoundButtons(); });
        connect(m_kiwiSdrManager, &KiwiSdrManager::profileWaterfallAvailabilityChanged, this,
                [this](const QString&, bool, const QString&) { refreshKiwiSoundButtons(); });
        connect(m_kiwiSdrManager, &KiwiSdrManager::profileStateChanged, this,
                [this](const QString& id, KiwiSdrClient::State st, const QString& detail) {
            const int before = m_kiwiPrevState.value(id, -1);
            m_kiwiPrevState.insert(id, int(st));
            if (st == KiwiSdrClient::State::Busy
                && before == int(KiwiSdrClient::State::Connected)) {
                setKiwiEvictsPrograms(id, true);
            } else if (st == KiwiSdrClient::State::Connected) {
                // Haelt er deutlich laenger als die 10 s, ist der Hinweis alt.
                QTimer::singleShot(20000, this, [this, id]() {
                    if (m_kiwiPrevState.value(id, -1)
                        == int(KiwiSdrClient::State::Connected)) {
                        setKiwiEvictsPrograms(id, false);
                    }
                });
            }
            // Erst jetzt: die Anzeige haengt davon ab, ob er "trennt".
            refreshKiwiSoundButtons();
            // Warum ein Kiwi wegbleibt, soll sichtbar sein und nicht nur im Log.
            if (st == KiwiSdrClient::State::Busy && !detail.isEmpty()) {
                showToast(m_kiwiSdrManager->displayName(id) + QStringLiteral(": ") + detail,
                          ToastSeverity::Warning, 10000);
            }
        });
        connect(m_kiwiSdrManager, &KiwiSdrManager::waterfallPreviewEnabledChanged, this,
                [this](const QString&, bool) { refreshKiwiSoundButtons(); });
        connect(m_radioModel, &RadioModel::sliceRemoved, this, [this](int sliceId) {
            for (auto it = m_kiwiSoundSlices.begin(); it != m_kiwiSoundSlices.end();) {
                if (it.value() == sliceId) { it = m_kiwiSoundSlices.erase(it); } else { ++it; }
            }
        });
    }

    // Zeilen: an die Messung des Mithoerers und an die Kachel.
    connect(m_kiwiSdrManager, &KiwiSdrManager::waterfallRowReady, this,
            [this](const QString& id, const QString&, const QVector<float>& bins,
                   double lowMhz, double highMhz, quint32) {
        const bool inGroup = m_kiwiGroup.contains(id);
        if (!inGroup && id != m_kiwiOwnProfileId) { return; }
        const double lowHz = lowMhz * 1.0e6;
        const double highHz = highMhz * 1.0e6;
        if (inGroup) {
            m_kiwiGroup.feedRow(id, bins, lowHz, highHz, kiwiOwnNowMs());
        }
        if (m_kiwiHearingDialog) {
            m_kiwiHearingDialog->pushWaterfallRow(id, bins, lowHz, highHz);
        }
    });
    connect(m_kiwiSdrManager, &KiwiSdrManager::profileStreamReset, this,
            [this](const QString& id) {
        if (m_kiwiHearingDialog) { m_kiwiHearingDialog->clearWaterfall(id); }
    });
    // Mitschnitt je Kiwi (unabhaengig von jeder Stummschaltung).
    connect(m_kiwiSdrManager, &KiwiSdrManager::decodedAudioReady, this,
            [this](const QString& id, const QByteArray& pcm) {
        KiwiRecording* rec = m_kiwiGroupRecordings.value(id, nullptr);
        if (rec && rec->isRecording()) { rec->feedAudio(pcm); }
    });
    connect(m_kiwiSdrManager, &KiwiSdrManager::profilesChanged, this, [this]() {
        for (const QString& id : m_kiwiGroup.ids()) {
            if (!m_kiwiSdrManager->hasProfile(id)) {
                m_kiwiGroup.remove(id);
                if (m_kiwiGroupRecordings.contains(id)) { stopKiwiGroupRecording(id); }
            }
        }
        refreshKiwiHearingDialog();
    });

    // Takt: Frequenz nachfuehren (der Client weist Gleiches selbst ab) und
    // die Kachel auffrischen. Laeuft nur, solange es Mithoerer gibt.
    m_kiwiGroupTimer = new QTimer(this);
    m_kiwiGroupTimer->setInterval(1000);
    connect(m_kiwiGroupTimer, &QTimer::timeout, this, [this]() {
        // Der Kiwi von "Mein Signal" gehoert ins Fenster KIWI WASSERFAELLE (dort
        // stehen Ton, +/-, Vergroessern); ein leeres Fenster braucht niemand.
        if (m_kiwiSdrManager && !m_kiwiOwnProfileId.isEmpty() && m_kiwiOwnSignalOn
            && m_kiwiSdrManager->hasProfile(m_kiwiOwnProfileId)
            && !m_kiwiSdrManager->profile(m_kiwiOwnProfileId).waterfallPreviewEnabled) {
            m_kiwiSdrManager->setWaterfallPreviewEnabled(m_kiwiOwnProfileId, true);
        }
        if (m_appletVis && m_kiwiWaterfallPanel && m_kiwiWaterfallPanel->isEmptyForTest()
            && m_kiwiWaterfallPanel->isVisible()) {
            m_appletVis->setVisible(QStringLiteral("KiwiWaterfalls"), false);
        }
        sendKiwiGroupTracking();
        refreshKiwiHearingDialog();
    });
    // Laeuft immer: auch ein Kiwi, der nur per WF-Schalter verbunden ist, braucht
    // eine Abstimmung (siehe sendKiwiGroupTracking). Ohne Arbeit kostet ein Takt nichts.
    m_kiwiGroupTimer->start();
}

// "Nur Kiwi hoeren": der Lautsprecher-Mix bekommt nur die Scheibe des Kiwi von
// "Mein Signal". Nur Wiedergabe; die Scheibe des Funkgeraets bleibt
// unberuehrt (kein SliceModel-Mute, nichts an das Geraet). Sicherung: nach 30 s
// von selbst aus, ebenso beim Ende von "Mein Signal".
void MainWindow::setKiwiSolo(bool on)
{
    AudioEngine* audio = m_radioModel ? m_radioModel->audioEngine() : nullptr;
    if (on) {
        const int sliceId = m_kiwiSdrManager && m_kiwiOwnSignalOn
            ? m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId) : -1;
        if (!audio || !kiwiControllableSlice(sliceId)) {
            showToast(tr("Solo needs \"My Signal\" with a Kiwi switched on."),
                      ToastSeverity::Info, 5000);
            m_kiwiSoloOn = false;
            if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setSoloActive(false); }
            return;
        }
        // Nur, solange der Kiwi wirklich Ton liefern kann: ein Kiwi im Fehler
        // (kein Tonstrom, Passwort, getrennt) macht aus Solo nur Stille.
        const KiwiSdrClient::State st = m_kiwiSdrManager->state(m_kiwiOwnProfileId);
        if (st == KiwiSdrClient::State::Error) {
            showToast(tr("Solo needs a connected Kiwi — this one is not delivering audio."),
                      ToastSeverity::Info, 6000);
            m_kiwiSoloOn = false;
            if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setSoloActive(false); }
            return;
        }
        audio->setSoloSlice(sliceId);
        m_kiwiSoloOn = true;
        if (m_kiwiSoloTimer) { m_kiwiSoloTimer->start(); }
    } else {
        if (audio) { audio->setSoloSlice(-1); }
        m_kiwiSoloOn = false;
        if (m_kiwiSoloTimer) { m_kiwiSoloTimer->stop(); }
    }
    if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setSoloActive(m_kiwiSoloOn); }
}

// ── Ton je Kiwi ─────────────────────────────────────────────────────
//
// Ein Kiwi ist hoerbar, wenn er einer Scheibe zugeordnet ist (Profil->Scheibe-
// Bruecke, siehe MainWindow_KiwiSdr.cpp). Fuer einen Kiwi aus den KIWI-WASSER-
// FAELLEN legen wir dafuer EINE EIGENE Scheibe an -- ungebunden, ohne Funkgeraet
// (suppressAutoStreamBinding), also hinter der Sicherheitsschranke
// kiwiControllableSlice. Die Flex-Scheibe wird nie benutzt oder veraendert.
namespace {
const QString kKiwiEvictingKey = QStringLiteral("KiwiEvictingReceivers");
const QString kKiwiVolumesKey = QStringLiteral("KiwiVolumes");   // "id=dB;..."
}

bool MainWindow::kiwiEvictsPrograms(const QString& profileId) const
{
    return AppSettings::instance().value(kKiwiEvictingKey).toString()
        .split(QLatin1Char(';'), Qt::SkipEmptyParts).contains(profileId);
}

void MainWindow::setKiwiEvictsPrograms(const QString& profileId, bool evicts)
{
    QStringList ids = AppSettings::instance().value(kKiwiEvictingKey).toString()
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (ids.contains(profileId) == evicts) { return; }
    if (evicts) { ids.append(profileId); } else { ids.removeAll(profileId); }
    AppSettings::instance().setValue(kKiwiEvictingKey, ids.join(QLatin1Char(';')));
    refreshKiwiSdrAppletReceivers();
}

void MainWindow::zeigeKiwiFenster(const QString& profileId)
{
    // Der Streifen muss da sein, auch wenn die Vorschau schon vorher an war
    // und deshalb kein Aenderungssignal kam.
    if (!profileId.isEmpty() && m_kiwiWaterfallPanel && m_kiwiSdrManager
        && m_kiwiSdrManager->hasProfile(profileId)) {
        m_kiwiWaterfallPanel->setStripEnabled(
            profileId, true, m_kiwiSdrManager->displayName(profileId));
    }
    if (m_appletVis) {
        m_appletVis->ensureVisible(QStringLiteral("KiwiWaterfalls"));
    }
    // Ein EIGENES Fenster, nicht eine Kachel in der Applet-Spalte, wo man es
    // nicht findet (Betreiber 2026-11-25: "es oeffnet sich wieder kein
    // Fenster!!!!"): steht es weder im Seitenbereich noch schon als Fenster,
    // wird es hier herausgeloest.
    if (m_kiwiWaterfallPanel && m_appletPanel) {
        const QString pid = QStringLiteral("KiwiWaterfalls");
        // Ohne Grafikflaeche (offscreen-Pruefstaende: "No QRhi") stuerzt das
        // frisch geloeste Fenster beim Zeichnen ab; dort bleibt es in der Spalte.
        const bool offscreen =
            QGuiApplication::platformName() == QLatin1String("offscreen");
        if (!offscreen && !m_floatingApplets.contains(pid) && !sideAreaHas(pid)
            && m_appletPanel->applets().contains(m_kiwiWaterfallPanel)) {
            // Erst im naechsten Durchlauf der Schleife: wir koennen aus einem
            // Signal des Panels selbst heraus aufgerufen werden.
            QTimer::singleShot(0, this, [this, pid]() {
                if (!m_kiwiWaterfallPanel || !m_appletPanel
                    || m_floatingApplets.contains(pid) || sideAreaHas(pid)
                    || !m_appletPanel->applets().contains(m_kiwiWaterfallPanel)) {
                    kiwiFensterNachVorn(m_kiwiWaterfallPanel, this);
                    return;
                }
                detachApplet(m_kiwiWaterfallPanel,
                             m_appletPanel->appletPosition(m_kiwiWaterfallPanel));
                kiwiFensterNachVorn(m_kiwiWaterfallPanel, this);
            });
            return;
        }
    }
    kiwiFensterNachVorn(m_kiwiWaterfallPanel, this);
}

void MainWindow::setKiwiSound(const QString& profileId, bool on)
{
    if (!m_kiwiSdrManager || !m_radioModel || !m_kiwiSdrManager->hasProfile(profileId)) {
        return;
    }
    if (on) { zeigeKiwiFenster(profileId); }
    const int assigned = m_kiwiSdrManager->assignedSliceForProfile(profileId);

    // "Ton an" an einem getrennten Kiwi heisst: wieder verbinden (Betreiber
    // 2026-11-25: "haengt schon wieder" -- der Knopf stand an, nichts geschah).
    // Ein Klick, ein Versuch; von selbst wird nicht neu gewaehlt.
    if (on && assigned >= 0) {
        const KiwiSdrClient::State st = m_kiwiSdrManager->state(profileId);
        if (st == KiwiSdrClient::State::Disconnected
            || st == KiwiSdrClient::State::Busy
            || st == KiwiSdrClient::State::Error
            || st == KiwiSdrClient::State::CampDisconnected) {
            m_kiwiSdrManager->connectProfile(profileId);
        }
    }

    // Der Kiwi von "Mein Signal" behaelt seine Scheibe; sein Ton wird nur stumm/
    // laut geschaltet (Betreiber 2026-11-25: "das Ein- und Ausschalten geht nicht").
    if (profileId == m_kiwiOwnProfileId && assigned >= 0) {
        if (AudioEngine* audio = m_radioModel->audioEngine()) {
            if (on) { m_kiwiUserMuted.remove(profileId); }
            else { m_kiwiUserMuted.insert(profileId); }
            audio->setKiwiSdrAudioSourceEnabled(assigned, on);
        }
        refreshKiwiSoundButtons();
        return;
    }
    if (on) { m_kiwiUserMuted.remove(profileId); }

    if (on) {
        if (assigned >= 0) { refreshKiwiSoundButtons(); return; }   // schon hoerbar
        // Die aktive Scheibe merken: die neue ungebundene darf nichts verschieben.
        const int activeBefore = m_radioModel->activeSlice() ? m_radioModel->activeSlice()->sliceIndex() : -1;
        const int sid = m_radioModel->addSlice(QString(), true);
        if (activeBefore >= 0 && m_radioModel->activeSlice()
            && m_radioModel->activeSlice()->sliceIndex() != activeBefore) {
            m_radioModel->setActiveSliceById(activeBefore);
        }
        SliceModel* slice = m_radioModel->sliceById(sid);
        if (!slice) {
            showToast(tr("No slice could be created for this Kiwi."),
                      ToastSeverity::Warning, 6000);
            refreshKiwiSoundButtons();
            return;
        }
        slice->setSuppressAutoStreamBinding(true);
        // Die Frequenz der Sendescheibe als Start (nur gelesen).
        if (SliceModel* tx = kiwiOwnTxSlice()) {
            quint64 hz = m_radioModel->transmitFrequencyHz(tx);
            if (hz == 0) { hz = quint64(tx->frequency()); }
            if (hz > 0) { slice->setFrequency(double(hz)); }
            slice->setDspMode(tx->dspMode());
            slice->setFilter(tx->filterLow(), tx->filterHigh());
        }
        m_kiwiSdrManager->assignSliceToProfile(
            sid, profileId, slice->frequency() / 1.0e6,
            SliceModel::modeName(slice->dspMode()),
            slice->filterLow(), slice->filterHigh(), slice->panKey(),
            QString(), SliceModel::cwPitchHz());
        if (!kiwiControllableSlice(sid)) {
            // Schranke sagt nein: wieder zurueck, nichts bleibt haengen.
            m_kiwiSdrManager->clearSliceAssignment(sid);
            m_radioModel->removeSlice(sid);
            refreshKiwiSoundButtons();
            return;
        }
        m_kiwiSoundSlices.insert(profileId, sid);
        // Wer als Mithoerer lief, folgt jetzt seiner Scheibe (siehe sendKiwiGroupTracking).
    } else {
        if (profileId == m_kiwiOwnProfileId) {
            showToast(tr("This Kiwi carries My Signal — switch that off there."),
                      ToastSeverity::Info, 6000);
            refreshKiwiSoundButtons();
            return;
        }
        if (assigned < 0) { refreshKiwiSoundButtons(); return; }
        if (AudioEngine* audio = m_radioModel->audioEngine()) {
            audio->removeKiwiSdrAudioSource(assigned);
        }
        m_kiwiSdrManager->clearSliceAssignment(assigned);
        // Die Scheibe, die wir fuer den Ton angelegt haben, wieder wegraeumen;
        // eine fremde (Antennenwahl am Slice) bleibt stehen.
        if (m_kiwiSoundSlices.value(profileId, -1) == assigned) {
            m_kiwiSoundSlices.remove(profileId);
            if (SliceModel* s = m_radioModel->sliceById(assigned)) {
                s->setSuppressAutoStreamBinding(false);
            }
            m_radioModel->removeSlice(assigned);
        }
        // Ein Kiwi ohne Wasserfall-Kanal hat ausser dem Ton nichts zu zeigen:
        // Ton aus heisst getrennt (Betreiber 2026-11-25: "bleibt nach Ton aus
        // haengen").
        const KiwiSdrClient::State st = m_kiwiSdrManager->state(profileId);
        const bool up = st == KiwiSdrClient::State::Connected
                        || st == KiwiSdrClient::State::Connecting;
        if (up && !m_kiwiSdrManager->waterfallAvailable(profileId)) {
            m_kiwiSdrManager->setWaterfallPreviewEnabled(profileId, false);
            m_kiwiSdrManager->disconnectProfile(profileId);
        }
    }
    refreshKiwiSoundButtons();
}

// Verkleinern/Vergroessern des Kiwi-Bereichs: der Kiwi kennt nur ganzzahlige
// Zoomstufen und liefert unter etwa 10 kHz kein brauchbares Bild.
void MainWindow::zoomKiwiStrip(const QString& profileId, int direction)
{
    if (!m_kiwiSdrManager || !m_kiwiSdrManager->hasProfile(profileId)) { return; }
    constexpr double kMin = 5000.0;
    constexpr double kMax = 200000.0;
    // Der Kiwi von "Mein Signal" geht von der Achse des Panadapters aus.
    const bool own = m_kiwiOwnSignalOn && profileId == m_kiwiOwnProfileId;
    SpectrumWidget* pane = m_kiwiOwnPan.data();
    const double paneSpan = pane
        ? std::clamp(pane->bandwidth(), kMinViewHz, kMaxViewHz) : 40000.0;
    double span = m_kiwiViewSpanHz.value(profileId, 0.0);
    if (span <= 0.0 && own) { span = paneSpan; }
    if (span <= 0.0) {
        // Ausgangswert: was der Streifen gerade zeigt, sonst 40 kHz.
        auto* strip = m_kiwiWaterfallPanel ? m_kiwiWaterfallPanel->stripForTest(profileId) : nullptr;
        span = strip && strip->spanHz() > 0.0 ? strip->spanHz() : 40000.0;
    }
    span = std::clamp(direction > 0 ? span * 0.5 : span * 2.0, kMin, kMax);
    m_kiwiViewSpanHz.insert(profileId, span);
    if (own) {
        // Wieder so breit wie der Panadapter: dessen Achse gilt wieder.
        if (std::abs(span - paneSpan) < paneSpan * 0.05) {
            m_kiwiViewSpanHz.remove(profileId);
        }
        syncKiwiOwnSignal();
        return;
    }
    // Mit Scheibe (Ton an): die Ansicht gleich setzen; sonst nimmt sie der
    // naechste Takt von sendKiwiGroupTracking mit.
    const int sid = m_kiwiSdrManager->assignedSliceForProfile(profileId);
    if (SliceModel* slice = kiwiControllableSlice(sid)) {
        m_kiwiSdrManager->updateWaterfallView(
            sid, QStringLiteral("kiwi-group"),
            slice->frequency() / 1.0e6, span / 1.0e6,
            m_kiwiSdrManager->profile(profileId).waterfallRate);
    }
}

// Ton fuer alle gezeigten (und verbundenen) Kiwis ein oder aus.
void MainWindow::setAllKiwiSound(bool on)
{
    if (!m_kiwiSdrManager) { return; }
    int count = 0;
    for (const KiwiSdrAntennaProfile& pr : m_kiwiSdrManager->profiles()) {
        if (!pr.waterfallPreviewEnabled && m_kiwiSdrManager->assignedSliceForProfile(pr.id) < 0) {
            continue;
        }
        if (pr.id == m_kiwiOwnProfileId) { continue; }   // der von "Mein Signal" bleibt
        if (on) {
            const KiwiSdrClient::State st = m_kiwiSdrManager->state(pr.id);
            if (st == KiwiSdrClient::State::Disconnected || st == KiwiSdrClient::State::Error) {
                continue;   // kein Ton von etwas, das nicht laeuft
            }
            if (++count > kKiwiMaxTotal) { break; }   // hoechstens fuenf
        }
        setKiwiSound(pr.id, on);
    }
}

// "Radio stumm": nur Wiedergabe im Lautsprecher-Mix. Ohne hoerbaren Kiwi
// waere das Stille, also nur mit mindestens einem Kiwi mit Ton -- und es geht
// von selbst aus, wenn der letzte Kiwi-Ton wegfaellt.
void MainWindow::setRadioMuted(bool on)
{
    AudioEngine* audio = m_radioModel ? m_radioModel->audioEngine() : nullptr;
    if (!audio) { m_radioMuted = false; return; }
    m_radioMuted = on;
    audio->setMuteNonKiwi(on);
    qCInfo(lcKiwiSdr) << "Radio-Ton:" << (on ? "stumm" : "an")
                      << "(Schalter Panadapter-Kopf oder Kiwi-Fenster)";
    if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setRadioMutedActive(on); }
    if (m_panStack) {
        for (auto* a : m_panStack->allApplets()) {
            if (a) { a->setRadioMutedActive(on); }
        }
    }
}

QString MainWindow::kiwiLostTitle(const QString& profileId) const
{
    if (!m_kiwiSdrManager || !m_kiwiSdrManager->hasProfile(profileId)) { return QString(); }
    switch (m_kiwiSdrManager->state(profileId)) {
    case KiwiSdrClient::State::Busy:
        return kiwiEvictsPrograms(profileId) ? tr("Disconnected – server limit")
                                             : tr("Disconnected – receiver full");
    case KiwiSdrClient::State::Error:
        return tr("Disconnected – connection error");
    case KiwiSdrClient::State::Disconnected:
    case KiwiSdrClient::State::CampDisconnected:
        return tr("Disconnected");
    case KiwiSdrClient::State::Connecting:
    case KiwiSdrClient::State::Connected:
    case KiwiSdrClient::State::Waiting:
    case KiwiSdrClient::State::Camping:
        break;
    }
    return QString();
}

// Das X heisst schliessen (Betreiber 2026-11-27: "kiwis gehen mit X nicht
// zu"). Vorher schaltete es nur die Wasserfall-Vorschau ab: war die nie an
// (nur "Ton an"), geschah nichts; sonst blieben Ton und Verbindung, und bei
// "Mein Signal" holte der Sekundentakt den Streifen zurueck.
void MainWindow::closeKiwi(const QString& profileId)
{
    if (!m_kiwiSdrManager || !m_radioModel || profileId.isEmpty()) { return; }
    // Kopie: der Aufrufer reicht die Kennung aus dem Streifen, der gleich geht.
    const QString id = profileId;
    if (m_kiwiOwnSignalOn && id == m_kiwiOwnProfileId) { setKiwiOwnSignal(false); }
    if (m_kiwiGroup.contains(id)) { removeKiwiListener(id); }
    const int sliceId = m_kiwiSdrManager->assignedSliceForProfile(id);
    if (sliceId >= 0) {
        if (AudioEngine* audio = m_radioModel->audioEngine()) {
            audio->removeKiwiSdrAudioSource(sliceId);
        }
        m_kiwiSdrManager->clearSliceAssignment(sliceId);
        // Nur die fuer seinen Ton angelegte Scheibe wieder wegraeumen.
        if (m_kiwiSoundSlices.value(id, -1) == sliceId) {
            if (SliceModel* s = m_radioModel->sliceById(sliceId)) {
                s->setSuppressAutoStreamBinding(false);
            }
            m_radioModel->removeSlice(sliceId);
        }
    }
    m_kiwiSoundSlices.remove(id);
    m_kiwiUserMuted.remove(id);
    m_kiwiSdrManager->setWaterfallPreviewEnabled(id, false);
    m_kiwiSdrManager->disconnectProfile(id);
    // Auch wenn die Vorschau schon aus war und darum kein Signal kam.
    if (m_kiwiWaterfallPanel) {
        m_kiwiWaterfallPanel->setStripEnabled(id, false, QString());
    }
    refreshKiwiSoundButtons();
    refreshKiwiHearingDialog();
}

void MainWindow::retireKiwi(const QString& profileId)
{
    if (!m_kiwiSdrManager || profileId == m_kiwiOwnProfileId
        || !m_kiwiSdrManager->hasProfile(profileId)) {
        return;
    }
    if (m_kiwiSdrManager->assignedSliceForProfile(profileId) >= 0) {
        setKiwiSound(profileId, false);
    }
    m_kiwiSoundSlices.remove(profileId);
    if (m_kiwiGroup.contains(profileId)) { removeKiwiListener(profileId); }
    m_kiwiSdrManager->setWaterfallPreviewEnabled(profileId, false);
    m_kiwiSdrManager->disconnectProfile(profileId);
}

int MainWindow::kiwiVolumeDb(const QString& profileId) const
{
    const QStringList entries = AppSettings::instance().value(kKiwiVolumesKey).toString()
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QString prefix = profileId + QLatin1Char('=');
    for (const QString& e : entries) {
        if (e.startsWith(prefix)) {
            return std::clamp(e.mid(prefix.size()).toInt(),
                              KiwiSdrApplet::kVolumeMinDb, KiwiSdrApplet::kVolumeMaxDb);
        }
    }
    return 0;
}

void MainWindow::setKiwiVolumeDb(const QString& profileId, int db)
{
    if (profileId.isEmpty()) { return; }
    QStringList entries = AppSettings::instance().value(kKiwiVolumesKey).toString()
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QString prefix = profileId + QLatin1Char('=');
    entries.removeIf([&prefix](const QString& e) { return e.startsWith(prefix); });
    if (db != 0) { entries.append(prefix + QString::number(db)); }
    AppSettings::instance().setValue(kKiwiVolumesKey, entries.join(QLatin1Char(';')));
}

void MainWindow::refreshKiwiSoundButtons()
{
    if (m_kiwiStrip && m_kiwiSdrManager && !m_kiwiOwnProfileId.isEmpty()) {
        m_kiwiStrip->setLost(kiwiLostTitle(m_kiwiOwnProfileId),
                             m_kiwiSdrManager->stateDetail(m_kiwiOwnProfileId));
        m_kiwiStrip->setSoundActive(
            m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId) >= 0
            && !m_kiwiUserMuted.contains(m_kiwiOwnProfileId));
    }
    if (!m_kiwiWaterfallPanel || !m_kiwiSdrManager) { return; }
    for (const KiwiSdrAntennaProfile& pr : m_kiwiSdrManager->profiles()) {
        const bool audible = m_kiwiSdrManager->assignedSliceForProfile(pr.id) >= 0
                             && !m_kiwiUserMuted.contains(pr.id);
        m_kiwiWaterfallPanel->setSoundActive(pr.id, audible);
        const KiwiSdrClient::State st = m_kiwiSdrManager->state(pr.id);
        const bool up = st == KiwiSdrClient::State::Connected || st == KiwiSdrClient::State::Camping;
        QString notice;
        if (up && !m_kiwiSdrManager->waterfallAvailable(pr.id)) {
            notice = audible
                ? tr("This Kiwi has no free waterfall channel — sound only.")
                : tr("This Kiwi has no free waterfall channel — nothing to show. "
                     "Choose another Kiwi.");
        }
        m_kiwiWaterfallPanel->setNotice(pr.id, notice);
        m_kiwiWaterfallPanel->setLost(pr.id, kiwiLostTitle(pr.id),
                                      m_kiwiSdrManager->stateDetail(pr.id));
    }
}

void MainWindow::updateKiwiGroupCapacity()
{
    const bool primary = !m_kiwiOwnProfileId.isEmpty();
    m_kiwiGroup.setCapacity(kKiwiMaxTotal - (primary ? 1 : 0));
    // Der gewaehlte Kiwi von "Mein Signal" zaehlt nicht doppelt.
    if (primary && m_kiwiGroup.contains(m_kiwiOwnProfileId)) {
        removeKiwiListener(m_kiwiOwnProfileId);
    }
    while (m_kiwiGroup.size() > m_kiwiGroup.capacity()) {
        removeKiwiListener(m_kiwiGroup.members().last().id);
        showToast(tr("Only five Kiwis at once: the last listener was closed."),
                  ToastSeverity::Info, 6000);
    }
}

void MainWindow::openKiwiHearingDialog()
{
    qCInfo(lcKiwiSdr) << "Gehoert: Fenster wird geoeffnet";
    if (!m_kiwiHearingDialog) {
        auto* dlg = new KiwiHearingDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        m_kiwiHearingDialog = dlg;
        connect(dlg, &KiwiHearingDialog::addRequested, this,
                [this]() { addKiwiToGroupFromPicker(); });
        connect(dlg, &KiwiHearingDialog::removeRequested, this,
                [this](const QString& id) { removeKiwiListener(id); });
        connect(dlg, &KiwiHearingDialog::recordToggled, this,
                [this](const QString& id, bool on) {
            if (id == m_kiwiOwnProfileId) {
                if (on) { startKiwiRecording(); } else { stopKiwiRecording(); }
            } else if (on) {
                startKiwiGroupRecording(id);
            } else {
                stopKiwiGroupRecording(id);
            }
        });
    }
    refreshKiwiHearingDialog();
    m_kiwiHearingDialog->show();
    m_kiwiHearingDialog->raise();
    m_kiwiHearingDialog->activateWindow();
    qCInfo(lcKiwiSdr) << "Gehoert: Fenster sichtbar" << m_kiwiHearingDialog->isVisible()
                      << "Rahmen" << m_kiwiHearingDialog->frameGeometry()
                      << "Fenster" << (m_kiwiHearingDialog->windowHandle() != nullptr);
}

// Das Verzeichnis wie bei "Anderer Kiwi ...": ein Klick, eine Abfrage,
// danach aus dem Sitzungsspeicher.
void MainWindow::addKiwiToGroupFromPicker(const QString& replaceId)
{
    updateKiwiGroupCapacity();
    // Wer ersetzt, braucht keinen freien Platz.
    if (replaceId.isEmpty() && m_kiwiGroup.size() >= m_kiwiGroup.capacity()) {
        showToast(tr("All five places are taken."), ToastSeverity::Warning, 6000);
        return;
    }
    double freqHz = 0.0;
    if (SliceModel* tx = kiwiOwnTxSlice(); tx && m_radioModel) {
        freqHz = double(m_radioModel->transmitFrequencyHz(tx));
    }
    auto* picker = new KiwiPublicReceiverPicker(this);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->setWindowTitle(replaceId.isEmpty() ? tr("Add a Kiwi that listens along")
                                              : tr("Choose another Kiwi"));
    picker->setPreselection(AppSettings::stationGridSquare(), freqHz);
    QSet<QString> open;
    for (const QString& id : m_kiwiGroup.ids()) {
        open.insert(m_kiwiSdrManager->profile(id).endpoint);
    }
    if (!m_kiwiOwnProfileId.isEmpty()) {
        open.insert(m_kiwiSdrManager->profile(m_kiwiOwnProfileId).endpoint);
    }
    picker->setOpenEndpoints(open);
    const QPointer<KiwiPublicReceiverPicker> guard(picker);
    connect(picker, &QDialog::accepted, this, [this, guard, replaceId]() {
        if (!guard || !m_kiwiSdrManager) { return; }
        const QString endpoint = guard->selectedEndpoint();
        QString name = guard->selectedName();
        if (name.trimmed().isEmpty()) { name = endpoint; }
        QString id = m_kiwiSdrManager->profileIdForEndpoint(endpoint);
        if (id.isEmpty()) { id = m_kiwiSdrManager->addProfile(name, endpoint); }
        if (id.isEmpty()) { return; }
        double lat = 0.0, lon = 0.0;
        const bool has = guard->selectedLatLon(lat, lon);
        bool soundWanted = false;
        if (!replaceId.isEmpty() && replaceId != id) {
            // Der Ersatz uebernimmt: war der Alte zu hoeren, ist es der Neue.
            soundWanted = m_kiwiSdrManager->assignedSliceForProfile(replaceId) >= 0
                          && !m_kiwiUserMuted.contains(replaceId);
            const bool listened = m_kiwiGroup.contains(replaceId);
            retireKiwi(replaceId);
            if (!listened) {
                // Er lief nur als Wasserfall: der Neue ebenso.
                m_kiwiSdrManager->setWaterfallPreviewEnabled(id, true);
                zeigeKiwiFenster(id);
                if (soundWanted) { setKiwiSound(id, true); }
                return;
            }
        } else if (!replaceId.isEmpty()) {
            // Derselbe noch einmal gewaehlt: ein neuer Versuch.
            m_kiwiSdrManager->connectProfile(id);
            return;
        }
        if (addKiwiListener(id, guard->selectedKm(), has, lat, lon)) {
            // Auch im Fenster KIWI WASSERFAELLE zeigen.
            m_kiwiSdrManager->setWaterfallPreviewEnabled(id, true);
            zeigeKiwiFenster(id);
            if (soundWanted) { setKiwiSound(id, true); }
        }
    });
    picker->show();
    picker->raise();
    picker->activateWindow();
}

bool MainWindow::addKiwiListener(const QString& profileId, double km,
                                 bool hasPos, double lat, double lon)
{
    if (!m_kiwiSdrManager || !m_kiwiSdrManager->hasProfile(profileId)) {
        return false;
    }
    if (profileId == m_kiwiOwnProfileId) {
        showToast(tr("That Kiwi is already the one of My Signal."),
                  ToastSeverity::Info, 5000);
        return false;
    }
    // Ein Kiwi, der schon eine Scheibe speist, bleibt bei ihr: wuerde er hier
    // auf die Sendefrequenz gestellt, wanderte der Ton der Scheibe mit.
    if (m_kiwiSdrManager->assignedSliceForProfile(profileId) >= 0) {
        showToast(tr("That Kiwi already feeds a slice."), ToastSeverity::Info, 5000);
        return false;
    }
    updateKiwiGroupCapacity();
    KiwiGroupMember m;
    m.id = profileId;
    m.name = m_kiwiSdrManager->displayName(profileId);
    m.km = km;
    m.lat = lat;
    m.lon = lon;
    m.hasPosition = hasPos;
    switch (m_kiwiGroup.add(m)) {
    case KiwiGroup::AddResult::Full:
        showToast(tr("All five places are taken."), ToastSeverity::Warning, 6000);
        return false;
    case KiwiGroup::AddResult::AlreadyThere:
        return false;
    case KiwiGroup::AddResult::Added:
        break;
    }
    if (hasPos) { m_kiwiPositions.insert(profileId, QPointF(lat, lon)); }
    m_kiwiGroup.setMarkHz(0.0);
    m_kiwiSdrManager->setProfileListener(profileId, true);
    // Schon senden? Dann zaehlt es als Beginn (wie bei "Mein Signal").
    if (m_radioModel) {
        const TransmitModel& tx = m_radioModel->transmitModel();
        if (tx.isMox() || tx.isTune() || m_radioModel->flexSendetGerade()) {
            m_kiwiGroup.txChanged(true, kiwiOwnNowMs());
        }
    }
    sendKiwiGroupTracking();
    refreshKiwiHearingDialog();
    return true;
}

void MainWindow::removeKiwiListener(const QString& profileId)
{
    if (m_kiwiGroupRecordings.contains(profileId)) {
        stopKiwiGroupRecording(profileId);
    }
    m_kiwiGroup.remove(profileId);
    if (m_kiwiSdrManager) { m_kiwiSdrManager->setProfileListener(profileId, false); }
    refreshKiwiHearingDialog();
}

void MainWindow::kiwiGroupTxEdge(bool sending)
{
    if (m_kiwiGroup.size() == 0) { return; }
    m_kiwiGroup.txChanged(sending, kiwiOwnNowMs());
    if (m_kiwiHearingDialog) { m_kiwiHearingDialog->setTransmitting(sending); }
    refreshKiwiHearingDialog();
}

// Frequenz, Betriebsart und Ansicht der Mithoerer: die SENDEfrequenz
// (dieselbe Ableitung wie beim Kiwi von "Mein Signal"). Nur gelesen von
// der Sendescheibe; geschrieben wird allein an die Kiwis.
void MainWindow::sendKiwiGroupTracking()
{
    if (!m_kiwiSdrManager || !m_radioModel) { return; }
    // Wer bekommt eine Abstimmung: die Mithoerer der Gruppe -- und jeder Kiwi, der
    // nur ueber den WF-Schalter laeuft (keine Scheibe, nicht der von "Mein
    // Signal"). Ein Kiwi ohne Abstimmung liefert oft keinen Tonstrom, und der
    // Client gibt nach 12 s mit "no SND audio frames arrived" auf -- genau das
    // sah der Betreiber bei mehreren WF-Kiwis (2026-11-25, nicht live bestaetigt).
    QStringList ids;
    for (const QString& gid : m_kiwiGroup.ids()) {
        // Ein Kiwi mit Scheibe (Ton an) folgt seiner Scheibe, nicht der Sendefrequenz.
        if (m_kiwiSdrManager->assignedSliceForProfile(gid) < 0) { ids << gid; }
    }
    for (const KiwiSdrAntennaProfile& pr : m_kiwiSdrManager->profiles()) {
        if ((m_kiwiOwnSignalOn && pr.id == m_kiwiOwnProfileId)
            || ids.contains(pr.id)) {
            continue;
        }
        if (!pr.waterfallPreviewEnabled) { continue; }
        if (m_kiwiSdrManager->assignedSliceForProfile(pr.id) >= 0) { continue; }
        const KiwiSdrClient::State st = m_kiwiSdrManager->state(pr.id);
        if (st == KiwiSdrClient::State::Disconnected || st == KiwiSdrClient::State::Error) {
            continue;   // nichts verbunden, nichts abzustimmen
        }
        ids << pr.id;
    }
    // Kiwis mit Ton laufen auf einer eigenen, ungebundenen Scheibe: die muss der
    // Sendefrequenz auch ueber Bandwechsel folgen (Betreiber 2026-11-25: "spring
    // auf ein anderes band nicht um"). Nur die Scheibe des Kiwi wird gesetzt,
    // nie die des Funkgeraets (kiwiControllableSlice).
    if (SliceModel* txs = kiwiOwnTxSlice()) {
        quint64 hz = m_radioModel->transmitFrequencyHz(txs);
        if (hz == 0) { hz = quint64(txs->frequency()); }
        if (hz > 0) {
            // Dazu der Kiwi von "Mein Signal", auch wenn dessen Streifen aus ist.
            QList<int> follow = m_kiwiSoundSlices.values();
            if (!m_kiwiOwnProfileId.isEmpty()) {
                const int os = m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId);
                if (os >= 0 && !follow.contains(os)) { follow << os; }
            }
            for (const int sliceId : std::as_const(follow)) {
                SliceModel* ks = kiwiControllableSlice(sliceId);
                if (!ks) { continue; }
                if (ks->frequency() != double(hz)) { ks->setFrequency(double(hz)); }
                if (ks->dspMode() != txs->dspMode()) { ks->setDspMode(txs->dspMode()); }
                m_kiwiSdrManager->updateSliceTracking(
                    sliceId, double(hz) / 1.0e6, SliceModel::modeName(txs->dspMode()),
                    txs->filterLow(), txs->filterHigh(), ks->panKey(),
                    QString(), SliceModel::cwPitchHz());
                // Auch das Bild zieht mit (Bandwechsel). Gleiches weist der
                // Client selbst ab. "Mein Signal" regelt seine Ansicht selbst.
                const QString pid =
                    m_kiwiSdrManager->assignedProfileForSlice(sliceId);
                if (m_kiwiOwnSignalOn && pid == m_kiwiOwnProfileId) { continue; }
                SpectrumWidget* tsw = spectrumForSlice(txs);
                const double view = m_kiwiViewSpanHz.value(
                    pid, tsw ? std::clamp(tsw->bandwidth(), kMinViewHz, kMaxViewHz)
                             : 40000.0);
                m_kiwiSdrManager->updateWaterfallView(
                    sliceId, QStringLiteral("kiwi-group"), double(hz) / 1.0e6,
                    view / 1.0e6,
                    m_kiwiSdrManager->profile(pid).waterfallRate);
            }
        }
    }
    // (Kein sendKiwiOwnSignalTracking() im Sekundentakt: es meldet dem Kiwi die
    // Panadapter-Ansicht, und jede kleine Verschiebung wuerde den Wasserfall-Strom
    // neu anfordern -- das Ruckeln. Es laeuft nur bei echten Aenderungen.)
    if (ids.isEmpty()) { return; }
    SliceModel* tx = kiwiOwnTxSlice();
    if (!tx) {
        static bool warnedNoTx = false;
        if (!warnedNoTx) {
            warnedNoTx = true;
            qCInfo(lcKiwiSdr) << "Abstimmung der Vorschau-Kiwis: keine Sendescheibe (kein Funkgeraet?)";
        }
        return;
    }
    quint64 txHz = m_radioModel->transmitFrequencyHz(tx);
    if (txHz == 0) { txHz = quint64(tx->frequency()); }
    if (txHz == 0) { return; }

    if (m_kiwiGroup.size() > 0) {
        m_kiwiGroup.setMarkHz(double(txHz));
        if (m_kiwiHearingDialog) { m_kiwiHearingDialog->setMarkHz(double(txHz)); }
    }

    SpectrumWidget* sw = spectrumForSlice(tx);
    const double bw = sw ? std::clamp(sw->bandwidth(), kMinViewHz, kMaxViewHz) : 40000.0;
    static QSet<QString> logged;
    for (int i = 0; i < ids.size(); ++i) {
        const KiwiSdrAntennaProfile profile = m_kiwiSdrManager->profile(ids[i]);
        if (!logged.contains(ids[i])) {
            logged.insert(ids[i]);
            qCInfo(lcKiwiSdr).noquote()
                << "Abstimmung" << profile.name << "auf" << double(txHz) / 1.0e6
                << "MHz" << SliceModel::modeName(tx->dspMode())
                << (m_kiwiGroup.contains(ids[i]) ? "(Mithoerer)" : "(Vorschau)");
        }
        m_kiwiSdrManager->primeProfileTracking(
            ids[i], kListenerTrackingBase + i,
            double(txHz) / 1.0e6, SliceModel::modeName(tx->dspMode()),
            tx->filterLow(), tx->filterHigh(),
            QStringLiteral("kiwi-group"), double(txHz) / 1.0e6,
            m_kiwiViewSpanHz.value(ids[i], bw) / 1.0e6,
            profile.waterfallRate, QString(), SliceModel::cwPitchHz());
    }
}

static QString stateTextFor(KiwiSdrClient::State s)
{
    switch (s) {
    case KiwiSdrClient::State::Connected:
    case KiwiSdrClient::State::Camping:         return MainWindow::tr("connected");
    case KiwiSdrClient::State::Connecting:      return MainWindow::tr("connecting …");
    case KiwiSdrClient::State::Busy:            return MainWindow::tr("busy");
    case KiwiSdrClient::State::Waiting:         return MainWindow::tr("waiting");
    case KiwiSdrClient::State::Error:           return MainWindow::tr("error");
    default:                                    return MainWindow::tr("not connected");
    }
}

void MainWindow::refreshKiwiHearingDialog()
{
    if (!m_kiwiHearingDialog || !m_kiwiSdrManager) { return; }
    QVector<KiwiHearingRow> rows;
    const auto fill = [this](const QString& id, double km) {
        KiwiHearingRow r;
        r.id = id;
        r.name = m_kiwiSdrManager->displayName(id);
        r.km = km;
        if (m_kiwiPositions.contains(id)) {
            r.hasPosition = true;
            r.lat = m_kiwiPositions.value(id).x();
            r.lon = m_kiwiPositions.value(id).y();
        }
        const KiwiSdrClient::State st = m_kiwiSdrManager->state(id);
        r.stateText = stateTextFor(st);
        r.connected = KiwiSdrClient::stateHasReceiveAudio(st);
        return r;
    };
    if (!m_kiwiOwnProfileId.isEmpty() && m_kiwiSdrManager->hasProfile(m_kiwiOwnProfileId)) {
        KiwiHearingRow r = fill(m_kiwiOwnProfileId, m_kiwiOwnKm);
        r.primary = true;
        r.result = m_kiwiOwnSignalOn ? m_kiwiHearMe->result() : KiwiHearMeMeter::Result();
        r.recording = m_kiwiRecording && m_kiwiRecording->isRecording();
        rows.append(r);
    }
    for (const KiwiGroupMember& m : m_kiwiGroup.members()) {
        KiwiHearingRow r = fill(m.id, m.km);
        r.result = m.meter.result();
        KiwiRecording* rec = m_kiwiGroupRecordings.value(m.id, nullptr);
        r.recording = rec && rec->isRecording();
        rows.append(r);
    }
    m_kiwiHearingDialog->setSlotsFree(
        kKiwiMaxTotal - int(rows.size()));
    m_kiwiHearingDialog->setRows(rows);

    double lat = 0.0, lon = 0.0;
    const QString grid = AppSettings::stationGridSquare();
    const bool valid = isValidGridSquare(grid);
    if (valid) { calculateLatLonFromGridSquare(grid, lat, lon); }
    m_kiwiHearingDialog->setHome(lat, lon, valid, grid.left(6).toUpper());
}

void MainWindow::startKiwiGroupRecording(const QString& profileId)
{
    if (!m_kiwiSdrManager || !m_kiwiGroup.contains(profileId)) { return; }
    KiwiRecording* rec = m_kiwiGroupRecordings.value(profileId, nullptr);
    if (!rec) {
        rec = new KiwiRecording(this);
        m_kiwiGroupRecordings.insert(profileId, rec);
        connect(rec, &KiwiRecording::aborted, this, [this, profileId](const QString& why) {
            showToast(tr("Recording ended: %1").arg(why), ToastSeverity::Warning, 8000);
            stopKiwiGroupRecording(profileId);
        });
    }
    if (rec->isRecording()) { return; }
    QString error;
    const QString folder = AppSettings::dataDir() + QStringLiteral("/recordings");
    if (!rec->start(folder, m_kiwiSdrManager->displayName(profileId),
                    QDateTime::currentDateTime(), &error)) {
        showToast(tr("Recording not started: %1").arg(error), ToastSeverity::Warning, 8000);
        refreshKiwiHearingDialog();
        return;
    }
    m_kiwiSdrManager->setListenerRecording(profileId, true);
    refreshKiwiHearingDialog();
}

void MainWindow::stopKiwiGroupRecording(const QString& profileId)
{
    KiwiRecording* rec = m_kiwiGroupRecordings.value(profileId, nullptr);
    if (!rec) { return; }
    if (m_kiwiSdrManager) { m_kiwiSdrManager->setListenerRecording(profileId, false); }
    if (!rec->isFinalized() && !rec->wavPath().isEmpty()) {
        const QImage img = m_kiwiHearingDialog ? m_kiwiHearingDialog->waterfallImage(profileId)
                                               : QImage();
        const QString wav = rec->stop(img);
        if (!wav.isEmpty()) {
            showToast(tr("Recording saved: %1").arg(QFileInfo(wav).fileName()),
                      ToastSeverity::Info, 6000);
        }
    }
    m_kiwiGroupRecordings.remove(profileId);
    rec->deleteLater();
    refreshKiwiHearingDialog();
}

} // namespace Longpath
