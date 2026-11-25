// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Entwurf C „Mein Signal am Kiwi“
// (~/Longpath/entwuerfe-kiwi-2026-11-24/C-streifen-hauptfenster.png).
//
//   2026-11-24 — Neu angelegt.
//
// Der Betreiber will einen KiwiSDR leicht hoeren, sein eigenes
// ausgesendetes Signal dort hoeren, aufnehmen und schoen im Bild sehen.
// Gebaut auf der bestehenden Profil->Scheibe-Bruecke
// (MainWindow_KiwiSdr.cpp); hier steht nur, was der Modus dazutut:
//
//   1. Der Kiwi folgt der SENDEfrequenz (Split, XIT, FM-Ablage -- dieselbe
//      Ableitung wie beim Senden: RadioModel::transmitFrequencyHz), nicht
//      der Empfangsfrequenz seiner eigenen Scheibe.
//   2. Sein Ton bleibt beim Senden an (kiwiKeepsAudioDuringTx) -- sonst
//      waere der Modus genau dann stumm, wenn man hinhoeren will.
//   3. Ein Wasserfallstreifen mit der Frequenzachse des Panadapters
//      darueber und einer Marke auf der Sendefrequenz.
//   4. Eine ehrliche Antwort auf "hoert er mich?" (KiwiHearMeMeter).
//   5. Aufnahme des Kiwi-Tons (KiwiRecording).
//
// ── Die Sicherheitsschranke gilt unveraendert ───────────────────────
//
// Der Kiwi ist weiter nur an eine Scheibe gebunden, die
// kiwiControllableSlice() freigibt (streamIndex() < 0). Die Sendescheibe
// wird hier nur GELESEN (Frequenz, Betriebsart, Filter) -- geschrieben
// wird ausschliesslich an die Kiwi-Scheibe und an den Kiwi selbst.
// Die Sendefrequenz zu kennen steuert nichts am Funkgeraet.

#include "gui/MainWindow.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/KiwiOwnSignal.h"
#include "core/KiwiRecording.h"
#include "core/KiwiSdrManager.h"
#include "core/LogCategories.h"
#include "gui/KiwiOwnSignalStrip.h"
#include "gui/KiwiPublicReceiverPicker.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QDateTime>
#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

namespace Longpath {

namespace {
constexpr const char* kKeyProfile = "KiwiOwnSignal/ProfileId";
constexpr const char* kKeyKm = "KiwiOwnSignal/Km";
// Der Kiwi zeigt nie weniger als dies (er hat nur ganzzahlige Zoomstufen
// und sein Bild wird ab etwa 10 kHz Breite unbrauchbar eng).
constexpr double kMinViewHz = 10000.0;
// Der Streifen zeigt hoechstens soviel -- breiter bringt dem Kiwi-Zoom
// nichts, und der Panadapter kann viel breiter sein.
constexpr double kMaxViewHz = 200000.0;
}

qint64 MainWindow::kiwiOwnNowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

// Die Scheibe, auf der der Betreiber sendet -- ausser einer, die ein Kiwi
// speist (deren Frequenz ist nicht seine Sendefrequenz).
SliceModel* MainWindow::kiwiOwnTxSlice() const
{
    if (!m_radioModel) { return nullptr; }
    const auto notKiwi = [this](SliceModel* s) {
        return s && (!m_kiwiSdrManager
                     || m_kiwiSdrManager->assignedProfileForSlice(
                            s->sliceIndex()).isEmpty());
    };
    for (SliceModel* s : m_radioModel->slices()) {
        if (s && s->isTxSlice() && notKiwi(s)) { return s; }
    }
    SliceModel* active = m_radioModel->activeSlice();
    if (notKiwi(active)) { return active; }
    for (SliceModel* s : m_radioModel->slices()) {
        if (notKiwi(s)) { return s; }
    }
    return nullptr;
}

void MainWindow::wireKiwiOwnSignal()
{
    if (!m_kiwiSdrManager || !m_kiwiSdrApplet || !m_panStack) { return; }

    m_kiwiHearMe = std::make_shared<KiwiHearMeMeter>();
    m_kiwiRecording = new KiwiRecording(this);

    // ── Der Streifen: direkt unter dem Panadapter ────────────────────
    m_kiwiStrip = new KiwiOwnSignalStrip(this);
    m_kiwiStrip->setObjectName(QStringLiteral("kiwiOwnSignalStrip"));
    m_kiwiStrip->hide();
    if (QWidget* pane = m_panStack->parentWidget()) {
        if (auto* box = qobject_cast<QVBoxLayout*>(pane->layout())) {
            m_kiwiStrip->setParent(pane);
            box->insertWidget(box->indexOf(m_panStack) + 1, m_kiwiStrip, 0);
        }
    }

    connect(m_kiwiStrip, &KiwiOwnSignalStrip::soundToggled, this,
            [this](bool on) {
        if (!m_kiwiOwnProfileId.isEmpty()) { setKiwiSound(m_kiwiOwnProfileId, on); }
    });
    connect(m_kiwiStrip, &KiwiOwnSignalStrip::chooseOtherRequested, this,
            [this]() { chooseOtherKiwi(); });
    connect(m_kiwiStrip, &KiwiOwnSignalStrip::zoomRequested, this,
            [this](int dir) {
        if (!m_kiwiOwnProfileId.isEmpty()) { zoomKiwiStrip(m_kiwiOwnProfileId, dir); }
    });

    // ── Das Applet ────────────────────────────────────────────────────
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::ownSignalToggled, this,
            [this](bool on) { setKiwiOwnSignal(on); });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::recordToggled, this,
            [this](bool on) {
        if (on) { startKiwiRecording(); } else { stopKiwiRecording(); }
    });
    connect(m_kiwiSdrApplet, &KiwiSdrApplet::otherReceiverRequested, this,
            [this]() { chooseOtherKiwi(); });

    // ── Zeilen und Ton vom Profil des Modus ──────────────────────────
    connect(m_kiwiSdrManager, &KiwiSdrManager::waterfallRowReady, this,
            [this](const QString& id, const QString&,
                   const QVector<float>& bins, double lowMhz, double highMhz,
                   quint32) {
        if (!m_kiwiOwnSignalOn || id != m_kiwiOwnProfileId || !m_kiwiStrip) {
            return;
        }
        const double lowHz = lowMhz * 1.0e6;
        const double highHz = highMhz * 1.0e6;
        m_kiwiStrip->setRowIntervalMs(
            AppSettings::instance()
                .value(QStringLiteral("KiwiWaterfall/RowIntervalMs"),
                       QStringLiteral("250"))
                .toString().toInt());
        m_kiwiStrip->pushRow(bins, lowHz, highHz);
        m_kiwiHearMe->feedRow(bins, lowHz, highHz, kiwiOwnNowMs());
        const KiwiHearMeMeter::Result r = m_kiwiHearMe->result();
        m_kiwiStrip->setVerdict(r);
        m_kiwiStrip->setDelay(r.delayMs, r.delayMeasured);
    });
    connect(m_kiwiSdrManager, &KiwiSdrManager::profileStreamReset, this,
            [this](const QString& id) {
        if (m_kiwiOwnSignalOn && id == m_kiwiOwnProfileId && m_kiwiStrip) {
            m_kiwiStrip->clearRows();
        }
    });
    // Unabhaengig von der Stummschaltung: stumm heisst nicht ungehoert.
    connect(m_kiwiSdrManager, &KiwiSdrManager::decodedAudioReady, this,
            [this](const QString& id, const QByteArray& pcm) {
        if (m_kiwiRecording && m_kiwiRecording->isRecording()
            && id == m_kiwiOwnProfileId) {
            m_kiwiRecording->feedAudio(pcm);
        }
    });
    connect(m_kiwiRecording, &KiwiRecording::aborted, this,
            [this](const QString& reason) {
        showToast(tr("Recording ended: %1").arg(reason),
                  ToastSeverity::Warning, 8000);
        stopKiwiRecording();
    });
    m_kiwiRecordTimer = new QTimer(this);
    m_kiwiRecordTimer->setInterval(1000);
    connect(m_kiwiRecordTimer, &QTimer::timeout, this, [this]() {
        if (m_kiwiStrip && m_kiwiRecording && m_kiwiRecording->isRecording()) {
            m_kiwiStrip->setRecording(
                true, m_kiwiRecording->elapsedMs(),
                QFileInfo(m_kiwiRecording->wavPath()).fileName());
        }
    });

    // ── Wer fiel weg? ────────────────────────────────────────────────
    connect(m_kiwiSdrManager, &KiwiSdrManager::profilesChanged, this, [this]() {
        if (!m_kiwiOwnProfileId.isEmpty()
            && !m_kiwiSdrManager->hasProfile(m_kiwiOwnProfileId)) {
            setKiwiOwnSignal(false);
            m_kiwiOwnProfileId.clear();
            m_kiwiSdrApplet->setOwnSignalReceiverName(QString());
        }
    });
    connect(m_kiwiSdrManager, &KiwiSdrManager::sliceAssignmentChanged, this,
            [this](int, const QString&) {
        // Wurde die Scheibe des Kiwi freigegeben (echtes Funkgeraet hat sie
        // uebernommen, Scheibe geloescht), ist der Modus ohne Grundlage.
        if (m_kiwiOwnSignalOn
            && m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId) < 0) {
            setKiwiOwnSignal(false);
        }
    });

    // Wo werde ich gehoert: die Mithoerer (MainWindow_KiwiGroup.cpp).
    wireKiwiGroup();

    // Gemerkter Kiwi der letzten Sitzung. Der Modus selbst startet nie
    // von allein: er haengt an einer Verbindung zu einem fremden Server.
    const QString saved = AppSettings::instance().value(kKeyProfile).toString();
    if (!saved.isEmpty() && m_kiwiSdrManager->hasProfile(saved)) {
        const QString la = AppSettings::instance().value("KiwiOwnSignal/Lat", "").toString();
        const QString lo = AppSettings::instance().value("KiwiOwnSignal/Lon", "").toString();
        if (!la.isEmpty() && !lo.isEmpty()) {
            m_kiwiPositions.insert(saved, QPointF(la.toDouble(), lo.toDouble()));
        }
        setKiwiOwnSignalProfile(
            saved, AppSettings::instance().value(kKeyKm, "-1").toDouble());
    }
}

void MainWindow::setKiwiOwnSignalProfile(const QString& profileId, double km)
{
    if (!m_kiwiSdrManager || !m_kiwiSdrManager->hasProfile(profileId)) { return; }
    const bool changed = profileId != m_kiwiOwnProfileId;
    if (changed && m_kiwiOwnSignalOn) {
        // Anderer Kiwi: der Modus beginnt am neuen von vorn.
        setKiwiOwnSignal(false);
    }
    m_kiwiOwnProfileId = profileId;
    m_kiwiOwnKm = km;
    // Auch im Fenster KIWI WASSERFAELLE, dort stehen +/-, Ton und Bereich.
    m_kiwiSdrManager->setWaterfallPreviewEnabled(profileId, true);
    AppSettings::instance().setValue(kKeyProfile, profileId);
    AppSettings::instance().setValue(kKeyKm, QString::number(km, 'f', 1));
    updateKiwiGroupCapacity();
    refreshKiwiHearingDialog();
    const QString name = m_kiwiSdrManager->displayName(profileId);
    if (m_kiwiStrip) {
        m_kiwiStrip->setKiwiName(name);
        m_kiwiStrip->setDistanceKm(km);
    }
    if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setOwnSignalReceiverName(name); }
    refreshKiwiSoundButtons();
}

// "Anderen Kiwi ...": das oeffentliche Verzeichnis, nach Entfernung zum
// eigenen Locator und nach der Sendefrequenz vorsortiert. Der Abruf ist
// wie immer ein Klick (der Dialog holt die Liste beim Oeffnen, danach aus
// dem Sitzungsspeicher) -- kein Hintergrundabruf.
void MainWindow::chooseOtherKiwi()
{
    qCInfo(lcKiwiSdr) << "Anderer Kiwi: Auswahl wird geoeffnet";
    double freqHz = 0.0;
    if (SliceModel* tx = kiwiOwnTxSlice(); tx && m_radioModel) {
        freqHz = double(m_radioModel->transmitFrequencyHz(tx));
    }
    auto* picker = new KiwiPublicReceiverPicker(this);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->setPreselection(AppSettings::stationGridSquare(), freqHz);
    const QPointer<KiwiPublicReceiverPicker> guard(picker);
    connect(picker, &QDialog::accepted, this, [this, guard]() {
        if (!guard || !m_kiwiSdrManager) { return; }
        double lat = 0.0, lon = 0.0;
        const bool has = guard->selectedLatLon(lat, lon);
        const QString before = m_kiwiOwnProfileId;
        const bool beforeLost = !kiwiLostTitle(before).isEmpty();
        adoptPickedKiwi(guard->selectedEndpoint(), guard->selectedName(),
                        guard->selectedKm(), has, lat, lon);
        // Ein ausgefallener Kiwi bleibt nicht als totes Bild im Fenster.
        if (beforeLost && before != m_kiwiOwnProfileId) { retireKiwi(before); }
    });
    picker->show();
    picker->raise();
    picker->activateWindow();
    qCInfo(lcKiwiSdr) << "Anderer Kiwi: Auswahl sichtbar" << picker->isVisible()
                      << "Rahmen" << picker->frameGeometry()
                      << "Fenster" << (picker->windowHandle() != nullptr);
}

// Der aus dem Verzeichnis gewaehlte Kiwi wird zum Kiwi von "Mein Signal" -- und
// erscheint ausserdem im Fenster KIWI WASSERFAELLE (Betreiber 2026-11-25: "wenn
// ich bei anderen Kiwi doppelklick ... erscheint der nicht im eigenen Kiwi-
// Fenster"): dieses Fenster zeigt nur Kiwis mit eingeschaltetem WF.
void MainWindow::adoptPickedKiwi(const QString& endpoint, const QString& nameIn,
                                 double km, bool hasPos, double lat, double lon)
{
    if (!m_kiwiSdrManager) { return; }
    QString name = nameIn;
    if (name.trimmed().isEmpty()) { name = endpoint; }
    QString id = m_kiwiSdrManager->profileIdForEndpoint(endpoint);
    if (id.isEmpty()) { id = m_kiwiSdrManager->addProfile(name, endpoint); }
    if (id.isEmpty()) { return; }
    if (hasPos) {
        m_kiwiPositions.insert(id, QPointF(lat, lon));
        AppSettings::instance().setValue("KiwiOwnSignal/Lat", QString::number(lat, 'f', 4));
        AppSettings::instance().setValue("KiwiOwnSignal/Lon", QString::number(lon, 'f', 4));
    }
    m_kiwiSdrManager->setWaterfallPreviewEnabled(id, true);
    setKiwiOwnSignalProfile(id, km);
    setKiwiOwnSignal(true);
    zeigeKiwiFenster(id);
}

void MainWindow::setKiwiOwnSignal(bool on)
{
    if (!m_kiwiSdrManager || !m_radioModel || !m_kiwiStrip) { return; }
    if (on == m_kiwiOwnSignalOn) {
        if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setOwnSignalActive(on); }
        return;
    }

    if (!on) {
        if (m_kiwiSoloOn) { setKiwiSolo(false); }
        m_kiwiOwnSignalOn = false;
        for (const QMetaObject::Connection& c : m_kiwiOwnConnections) {
            disconnect(c);
        }
        m_kiwiOwnConnections.clear();
        m_kiwiOwnPan.clear();
        m_kiwiStrip->hide();
        m_kiwiStrip->clearRows();
        m_kiwiHearMe->reset();
        m_kiwiStrip->setVerdict({});
        if (m_kiwiRecording && m_kiwiRecording->isRecording()) {
            stopKiwiRecording();
        }
        if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setOwnSignalActive(false); }
        // Der Kiwi folgt wieder seiner eigenen Scheibe, die Sendesperre
        // wieder dem Profil.
        const int sliceId =
            m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId);
        if (SliceModel* s = kiwiControllableSlice(sliceId)) {
            updateKiwiSdrTrackingForSlice(s);
        }
        syncKiwiSdrTransmitMute();
        return;
    }

    // ── Einschalten ──────────────────────────────────────────────────
    if (m_kiwiOwnProfileId.isEmpty()
        || !m_kiwiSdrManager->hasProfile(m_kiwiOwnProfileId)) {
        // Noch kein Kiwi gewaehlt: das Verzeichnis, nicht ein Rateversuch.
        if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setOwnSignalActive(false); }
        chooseOtherKiwi();
        return;
    }

    // Der Kiwi braucht eine Scheibe fuer Ton und Bild -- eine, die nicht
    // an ein echtes Funkgeraet gebunden ist (Sicherheitsschranke).
    int sliceId = m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId);
    if (!kiwiControllableSlice(sliceId)) {
        assignKiwiProfileToSlice(m_kiwiOwnProfileId);
        sliceId = m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId);
    }
    if (!kiwiControllableSlice(sliceId)) {
        showToast(tr("My Signal: no slice free for the Kiwi."),
                  ToastSeverity::Warning, 8000);
        if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setOwnSignalActive(false); }
        return;
    }

    m_kiwiOwnSignalOn = true;
    m_kiwiHearMe->reset();
    m_kiwiStrip->clearRows();
    m_kiwiStrip->setVerdict({});
    m_kiwiStrip->setKiwiName(m_kiwiSdrManager->displayName(m_kiwiOwnProfileId));
    m_kiwiStrip->setDistanceKm(m_kiwiOwnKm);
    m_kiwiStrip->show();
    if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setOwnSignalActive(true); }

    // Sendefrequenz, Panadapter-Achse und Tastenzustand verfolgen.
    const auto sync = [this]() { syncKiwiOwnSignal(); };
    const auto follow = [this, sync](SliceModel* s) {
        if (!s) { return; }
        m_kiwiOwnConnections << connect(s, &SliceModel::frequencyChanged, this, sync)
                             << connect(s, &SliceModel::xitEnabledChanged, this, sync)
                             << connect(s, &SliceModel::xitHzChanged, this, sync)
                             << connect(s, &SliceModel::dspModeChanged, this, sync)
                             << connect(s, &SliceModel::filterChanged, this, sync)
                             << connect(s, &SliceModel::txSliceChanged, this, sync);
    };
    for (SliceModel* s : m_radioModel->slices()) { follow(s); }
    m_kiwiOwnConnections << connect(m_radioModel, &RadioModel::sliceAdded, this,
                                    [this, follow](int index) {
        follow(m_radioModel->sliceById(index));
        syncKiwiOwnSignal();
    });

    syncKiwiOwnSignal();

    // Tastenzustand: schon ein laufendes Senden zaehlt als Beginn.
    const TransmitModel& tx = m_radioModel->transmitModel();
    kiwiOwnSignalTxEdge(tx.isMox() || tx.isTune());
    // Ton: im Modus bleibt er beim Senden (siehe kiwiKeepsAudioDuringTx).
    syncKiwiSdrTransmitMute();
}

// Alles, was an Sendefrequenz und Achse haengt, in einem Zug.
void MainWindow::syncKiwiOwnSignal()
{
    if (!m_kiwiOwnSignalOn || !m_kiwiStrip) { return; }

    // Achse des Panadapters der Sendescheibe.
    SliceModel* tx = kiwiOwnTxSlice();
    SpectrumWidget* sw = tx ? spectrumForSlice(tx) : activeSpectrumWidget();
    if (sw != m_kiwiOwnPan.data()) {
        m_kiwiOwnPan = sw;
        if (sw) {
            m_kiwiOwnConnections << connect(sw, &SpectrumWidget::frequencyRangeChanged,
                                            this, [this](double, double) {
                syncKiwiOwnSignal();
            });
        }
    }
    if (sw) {
        const double bw = std::clamp(sw->bandwidth(), kMinViewHz, kMaxViewHz);
        m_kiwiStrip->setView(sw->centerFrequency(), bw);
        // Die Achse fluchtet mit der Flaeche des Panadapters (ohne dessen
        // dBm-Leiste rechts), nicht mit dem ganzen Fenster.
        if (QWidget* pane = m_kiwiStrip->parentWidget()) {
            const QRect plot = sw->spectrumRect();
            // Ueber Bildschirmkoordinaten: der Panadapter kann ein schwebendes
            // Fenster sein und ist dann kein Nachkomme von `pane` (mapTo
            // warnte bei jedem Ziehen, Betreiberlog 2026-11-25).
            const int left = pane->mapFromGlobal(sw->mapToGlobal(plot.topLeft())).x();
            const int right = pane->width() - (left + plot.width());
            m_kiwiStrip->setPlotMargins(left, right);
        }
    }

    sendKiwiOwnSignalTracking();
}

// Dem Kiwi die Sendefrequenz zu hoeren geben. Liefert false, wenn nichts
// gesendet werden konnte (keine Kiwi-Scheibe, Schranke).
bool MainWindow::sendKiwiOwnSignalTracking()
{
    if (!m_kiwiOwnSignalOn || !m_kiwiSdrManager || !m_radioModel) { return false; }
    const int kiwiSlice =
        m_kiwiSdrManager->assignedSliceForProfile(m_kiwiOwnProfileId);
    SliceModel* ks = kiwiControllableSlice(kiwiSlice);
    if (!ks) { return false; }

    SliceModel* tx = kiwiOwnTxSlice();
    // Ohne Sendescheibe folgt der Kiwi seiner eigenen (Rueckfall, nicht Fehler).
    SliceModel* src = tx ? tx : ks;
    quint64 txHz = m_radioModel->transmitFrequencyHz(src);
    if (txHz == 0) { txHz = quint64(src->frequency()); }
    if (txHz == 0) { return false; }

    m_kiwiStrip->setMarkHz(double(txHz));
    m_kiwiHearMe->setMarkHz(double(txHz));

    SpectrumWidget* sw = m_kiwiOwnPan.data();
    const QString panId = src->panKey().isEmpty() ? QStringLiteral("own-signal")
                                                  : src->panKey();
    m_kiwiSdrManager->updateSliceTracking(
        kiwiSlice, double(txHz) / 1.0e6,
        SliceModel::modeName(src->dspMode()),
        src->filterLow(), src->filterHigh(), panId,
        QString(), SliceModel::cwPitchHz());
    // Die Ansicht des Kiwi-Wasserfalls: die Breite des Panadapters (der
    // Kiwi rundet auf seine Zoomstufen), um die Sendefrequenz.
    const double bw = sw ? std::clamp(sw->bandwidth(), kMinViewHz, kMaxViewHz)
                         : 40000.0;
    const double centerHz = sw ? sw->centerFrequency() : double(txHz);
    m_kiwiSdrManager->updateWaterfallView(
        kiwiSlice, panId, centerHz / 1.0e6, bw / 1.0e6,
        m_kiwiSdrManager->profile(m_kiwiOwnProfileId).waterfallRate);
    return true;
}

void MainWindow::kiwiOwnSignalTxEdge(bool sending)
{
    if (!m_kiwiOwnSignalOn || !m_kiwiHearMe || !m_kiwiStrip) { return; }
    m_kiwiHearMe->txChanged(sending, kiwiOwnNowMs());
    const KiwiHearMeMeter::Result r = m_kiwiHearMe->result();
    m_kiwiStrip->setVerdict(r);
    m_kiwiStrip->setDelay(r.delayMs, r.delayMeasured);
}

void MainWindow::startKiwiRecording()
{
    if (!m_kiwiRecording || !m_kiwiSdrManager) { return; }
    if (m_kiwiOwnProfileId.isEmpty()
        || !m_kiwiSdrManager->hasProfile(m_kiwiOwnProfileId)) {
        showToast(tr("Record: choose a Kiwi first (My Signal or Other "
                     "Kiwi …)."), ToastSeverity::Warning, 8000);
        if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setRecordingActive(false); }
        return;
    }
    if (m_kiwiRecording->isRecording()) { return; }
    QString error;
    const QString folder = AppSettings::dataDir() + QStringLiteral("/recordings");
    if (!m_kiwiRecording->start(
            folder, m_kiwiSdrManager->displayName(m_kiwiOwnProfileId),
            QDateTime::currentDateTime(), &error)) {
        showToast(tr("Recording not started: %1").arg(error),
                  ToastSeverity::Warning, 8000);
        if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setRecordingActive(false); }
        return;
    }
    if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setRecordingActive(true); }
    if (m_kiwiStrip) {
        m_kiwiStrip->setRecording(
            true, 0, QFileInfo(m_kiwiRecording->wavPath()).fileName());
    }
    if (m_kiwiRecordTimer) { m_kiwiRecordTimer->start(); }
}

void MainWindow::stopKiwiRecording()
{
    if (m_kiwiRecordTimer) { m_kiwiRecordTimer->stop(); }
    if (m_kiwiSdrApplet) { m_kiwiSdrApplet->setRecordingActive(false); }
    if (m_kiwiStrip) { m_kiwiStrip->setRecording(false, 0, QString()); }
    if (!m_kiwiRecording || m_kiwiRecording->wavPath().isEmpty()) { return; }
    if (m_kiwiRecording->isFinalized()) { return; }   // zweiter Aufruf
    const QImage img = m_kiwiStrip ? m_kiwiStrip->waterfallImage() : QImage();
    const QString wav = m_kiwiRecording->stop(img);
    if (!wav.isEmpty()) {
        showToast(tr("Recording saved: %1").arg(QFileInfo(wav).fileName()),
                  ToastSeverity::Info, 6000);
    }
}

} // namespace Longpath
