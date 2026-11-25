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
#include "core/KiwiGroup.h"
#include "core/KiwiOwnSignal.h"
#include "core/KiwiRecording.h"
#include "core/Maidenhead.h"
#include "core/KiwiSdrManager.h"
#include "gui/KiwiHearingDialog.h"
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
#include <QTimer>

#include <algorithm>

namespace Longpath {

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
        sendKiwiGroupTracking();
        refreshKiwiHearingDialog();
    });
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
    if (!m_kiwiHearingDialog) {
        auto* dlg = new KiwiHearingDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
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
}

// Das Verzeichnis wie bei "Anderer Kiwi ...": ein Klick, eine Abfrage,
// danach aus dem Sitzungsspeicher.
void MainWindow::addKiwiToGroupFromPicker()
{
    updateKiwiGroupCapacity();
    if (m_kiwiGroup.size() >= m_kiwiGroup.capacity()) {
        showToast(tr("All five places are taken."), ToastSeverity::Warning, 6000);
        return;
    }
    double freqHz = 0.0;
    if (SliceModel* tx = kiwiOwnTxSlice(); tx && m_radioModel) {
        freqHz = double(m_radioModel->transmitFrequencyHz(tx));
    }
    auto* picker = new KiwiPublicReceiverPicker(this);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->setWindowTitle(tr("Add a Kiwi that listens along"));
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
    connect(picker, &QDialog::accepted, this, [this, guard]() {
        if (!guard || !m_kiwiSdrManager) { return; }
        const QString endpoint = guard->selectedEndpoint();
        QString name = guard->selectedName();
        if (name.trimmed().isEmpty()) { name = endpoint; }
        QString id = m_kiwiSdrManager->profileIdForEndpoint(endpoint);
        if (id.isEmpty()) { id = m_kiwiSdrManager->addProfile(name, endpoint); }
        if (id.isEmpty()) { return; }
        double lat = 0.0, lon = 0.0;
        const bool has = guard->selectedLatLon(lat, lon);
        addKiwiListener(id, guard->selectedKm(), has, lat, lon);
    });
    picker->setWindowFlag(Qt::WindowStaysOnTopHint, true);
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
    if (m_kiwiGroupTimer && !m_kiwiGroupTimer->isActive()) { m_kiwiGroupTimer->start(); }
    // Schon senden? Dann zaehlt es als Beginn (wie bei "Mein Signal").
    if (m_radioModel) {
        const TransmitModel& tx = m_radioModel->transmitModel();
        if (tx.isMox() || tx.isTune()) {
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
    if (m_kiwiGroup.size() == 0 && m_kiwiGroupTimer) { m_kiwiGroupTimer->stop(); }
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
    if (!m_kiwiSdrManager || !m_radioModel || m_kiwiGroup.size() == 0) { return; }
    SliceModel* tx = kiwiOwnTxSlice();
    if (!tx) { return; }
    quint64 txHz = m_radioModel->transmitFrequencyHz(tx);
    if (txHz == 0) { txHz = quint64(tx->frequency()); }
    if (txHz == 0) { return; }

    m_kiwiGroup.setMarkHz(double(txHz));
    if (m_kiwiHearingDialog) { m_kiwiHearingDialog->setMarkHz(double(txHz)); }

    SpectrumWidget* sw = spectrumForSlice(tx);
    const double bw = sw ? std::clamp(sw->bandwidth(), kMinViewHz, kMaxViewHz) : 40000.0;
    const QStringList ids = m_kiwiGroup.ids();
    for (int i = 0; i < ids.size(); ++i) {
        const KiwiSdrAntennaProfile profile = m_kiwiSdrManager->profile(ids[i]);
        m_kiwiSdrManager->primeProfileTracking(
            ids[i], kListenerTrackingBase + i,
            double(txHz) / 1.0e6, SliceModel::modeName(tx->dspMode()),
            tx->filterLow(), tx->filterHigh(),
            QStringLiteral("kiwi-group"), double(txHz) / 1.0e6, bw / 1.0e6,
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
