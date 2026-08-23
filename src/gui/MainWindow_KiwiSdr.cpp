// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/gui/MainWindow_KiwiSdr.cpp [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Das KiwiSDR-Protokoll stammt von John Seamons (ZL/KF6VO),
// http://kiwisdr.com.
//
//   2026-08-23 — Portiert (Stufe 4: Bedienflaeche).
//
// ── Was hier steht, und was NOCH NICHT ──────────────────────────────
//
// Aethers Fassung hat 2429 Zeilen und rund dreissig
// MainWindow-Methoden. Der grosse Teil davon haengt an Dingen, die es
// bei uns noch nicht gibt: Tonwege in die Mischung
// (feedKiwiSdrAudioData), Wasserfallzeilen in den Panadapter
// (setKiwiSdrWaterfallProfile), virtuelle Antennen je Scheibe,
// Bandrueckruf, Sendesperre, Diversity.
//
// Hier steht die BRUECKE und sonst nichts: der Manager meldet einen
// Zustand, das Applet zeigt ihn. Damit sieht der Betreiber seine
// Empfaenger, ihren Verbindungszustand und die zugeordnete Scheibe —
// mehr nicht, aber das richtig.
//
// Was fehlt, steht ausdruecklich hier, damit es niemand fuer erledigt
// haelt:
//   Stufe 5 — Ton: decodedAudioReady in die Mischung.
//   Stufe 6 — Wasserfall: waterfallRowReady auf den Panadapter, dazu
//             der Umschalter Geraet <-> KiwiSDR je Panadapter.
//   Stufe 7 — Sendesperre (syncKiwiSdrTransmitMute), virtuelle
//             Antennen, Bandrueckruf.

#include "gui/MainWindow.h"

#include "core/AppSettings.h"
#include "core/KiwiSdrManager.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QStringList>

namespace Longpath {

namespace {

// Die vier Zusammenfassungen sind zeichengetreu aus Aether uebernommen
// (dort im anonymen Namensraum von MainWindow_KiwiSdr.cpp). Sie haengen
// nur an KiwiSdrProtocol, das wir vollstaendig haben.

QString kiwiApiPolicyText(KiwiSdrProtocol::ApiPolicy policy)
{
    switch (policy) {
    case KiwiSdrProtocol::ApiPolicy::Disabled:
        return QStringLiteral("API disabled");
    case KiwiSdrProtocol::ApiPolicy::Limited:
        return QStringLiteral("API limited");
    case KiwiSdrProtocol::ApiPolicy::Open:
        return QStringLiteral("API open");
    case KiwiSdrProtocol::ApiPolicy::Unknown:
        break;
    }
    return QStringLiteral("API policy unknown");
}

QString kiwiStreamSummary(const KiwiSdrProtocol::StreamCapability& capability)
{
    const bool hasProtocolData =
        capability.requested
        || capability.observed
        || capability.uncompressedRequested
        || capability.compressedRequested
        || capability.uncompressedObserved
        || capability.compressedObserved
        || !capability.supportedLayouts.isEmpty()
        || !capability.observedLayouts.isEmpty()
        || capability.lastObservedLayout != KiwiSdrProtocol::FrameLayout::Unknown
        || !capability.unsupportedReason.isEmpty();
    if (!hasProtocolData) {
        return QString();
    }

    QStringList parts;
    parts << KiwiSdrProtocol::streamModeName(capability.mode);
    if (capability.observed
        && capability.lastObservedLayout != KiwiSdrProtocol::FrameLayout::Unknown) {
        parts << KiwiSdrProtocol::frameLayoutName(capability.lastObservedLayout);
    } else if (capability.requested) {
        parts << QStringLiteral("requested");
    }
    if (capability.uncompressedRequested && !capability.uncompressedObserved) {
        parts << QStringLiteral("uncompressed requested");
    } else if (capability.uncompressedObserved) {
        parts << QStringLiteral("uncompressed observed");
    }
    if (capability.compressedRequested && !capability.compressedObserved) {
        parts << QStringLiteral("compressed requested");
    }
    if (capability.compressedObserved) {
        parts << QStringLiteral("compressed observed");
    }
    if (!capability.unsupportedReason.isEmpty()) {
        parts << QStringLiteral("unsupported: %1")
                     .arg(capability.unsupportedReason);
    }
    return parts.join(QStringLiteral(" "));
}

QString kiwiReceiverMetadataSummary(
    const KiwiSdrProtocol::ReceiverMetadata& metadata)
{
    QStringList parts;
    if (!metadata.serverVersion.isEmpty()) {
        parts << QStringLiteral("v%1").arg(metadata.serverVersion);
    }
    if (metadata.hasUsers && metadata.hasUsersMax) {
        parts << QStringLiteral("Users %1/%2")
                     .arg(metadata.users)
                     .arg(metadata.usersMax);
    } else if (metadata.hasUsers) {
        parts << QStringLiteral("Users %1").arg(metadata.users);
    }
    if (metadata.hasBusy && metadata.busy) {
        parts << QStringLiteral("Busy");
    }
    if (metadata.hasCampStatus
        && metadata.campStatus != KiwiSdrProtocol::CampStatus::Unknown) {
        switch (metadata.campStatus) {
        case KiwiSdrProtocol::CampStatus::Offered:
            parts << QStringLiteral("Monitor offered");
            break;
        case KiwiSdrProtocol::CampStatus::Queued:
            if (metadata.hasCampQueuePosition
                && metadata.hasCampQueueWaiters) {
                parts << QStringLiteral("Queue %1/%2")
                             .arg(metadata.campQueuePosition)
                             .arg(metadata.campQueueWaiters);
            } else {
                parts << QStringLiteral("Queued");
            }
            if (metadata.hasCampQueueReloadRecommended
                && metadata.campQueueReloadRecommended) {
                parts << QStringLiteral("Channel free");
            }
            break;
        case KiwiSdrProtocol::CampStatus::Accepted:
            parts << (metadata.hasCampReceiverChannel
                          ? QStringLiteral("Camping RX%1")
                                .arg(metadata.campReceiverChannel)
                          : QStringLiteral("Camping"));
            break;
        case KiwiSdrProtocol::CampStatus::Rejected:
            parts << QStringLiteral("Camping rejected");
            break;
        case KiwiSdrProtocol::CampStatus::AudioStopped:
            parts << QStringLiteral("Camp audio stopped");
            break;
        case KiwiSdrProtocol::CampStatus::Disconnected:
            parts << QStringLiteral("Camp disconnected");
            break;
        case KiwiSdrProtocol::CampStatus::Unknown:
            break;
        }
    }
    if (metadata.hasMaxCampers) {
        parts << QStringLiteral("Max campers %1").arg(metadata.maxCampers);
    }
    if (metadata.hasExtApi) {
        parts << QStringLiteral("%1 (%2)")
                     .arg(kiwiApiPolicyText(metadata.apiPolicy))
                     .arg(metadata.extApi);
    }
    if (metadata.hasGpsGood) {
        parts << (metadata.gpsGood ? QStringLiteral("GPS good")
                                   : QStringLiteral("GPS not good"));
    } else if (!metadata.gpsStatus.isEmpty()) {
        parts << QStringLiteral("GPS %1").arg(metadata.gpsStatus);
    }
    if (metadata.hasAdcClipping) {
        parts << (metadata.adcClipping ? QStringLiteral("ADC clipping")
                                       : QStringLiteral("ADC normal"));
    }
    if (metadata.hasCoverageCenter && metadata.hasCoverageBandwidth) {
        const double lowMhz =
            metadata.coverageCenterMhz - metadata.coverageBandwidthMhz * 0.5;
        const double highMhz =
            metadata.coverageCenterMhz + metadata.coverageBandwidthMhz * 0.5;
        parts << QStringLiteral("%1-%2 MHz")
                     .arg(lowMhz, 0, 'f', 3)
                     .arg(highMhz, 0, 'f', 3);
    } else if (metadata.hasReportedFrequency) {
        parts << QStringLiteral("%1 MHz")
                     .arg(metadata.reportedFrequencyKhz / 1000.0, 0, 'f', 3);
    }
    if (metadata.hasReceiverChannel && metadata.hasWaterfallChannels) {
        parts << QStringLiteral("RX slot %1, W/F %2")
                     .arg(metadata.receiverChannel + 1)
                     .arg(metadata.waterfallChannels);
    }
    return parts.join(QStringLiteral(" · "));
}

QString kiwiProtocolSummary(const KiwiSdrProtocol::ProtocolState& protocol)
{
    QStringList parts;
    if (protocol.authMode != KiwiSdrProtocol::AuthMode::Unknown) {
        parts << QStringLiteral("Auth %1")
                     .arg(KiwiSdrProtocol::authModeName(protocol.authMode));
    }
    const QString sound = kiwiStreamSummary(protocol.sound);
    if (!sound.isEmpty()) {
        parts << sound;
    }
    const QString waterfall = kiwiStreamSummary(protocol.waterfall);
    if (!waterfall.isEmpty()) {
        parts << waterfall;
    }
    if (!protocol.unsupportedFrames.isEmpty()) {
        const KiwiSdrProtocol::FrameObservation last =
            protocol.unsupportedFrames.last();
        if (!last.unsupportedReason.isEmpty()) {
            parts << QStringLiteral("Last skipped %1")
                         .arg(last.unsupportedReason);
        }
    }
    return parts.join(QStringLiteral(" · "));
}

} // namespace

// ── Manager -> Applet ───────────────────────────────────────────────
//
// Zeichengetreu aus Aether, bis auf einen Punkt: dort holt die Methode
// das Applet ueber m_appletPanel->kiwiSdrApplet(). Unser
// AppletPanelWidget hat keinen solchen Zugriff je Applet-Art, wir
// halten den Zeiger direkt am MainWindow.
void MainWindow::refreshKiwiSdrAppletReceivers()
{
    if (!m_kiwiSdrApplet) {
        return;
    }

    QVector<KiwiSdrReceiverStatus> receivers;
    if (m_kiwiSdrManager) {
        for (const KiwiSdrAntennaProfile& profile : m_kiwiSdrManager->profiles()) {
            KiwiSdrReceiverStatus receiver;
            receiver.id = profile.id;
            receiver.name = m_kiwiSdrManager->displayName(profile.id);
            receiver.state = m_kiwiSdrManager->state(profile.id);
            receiver.detail = m_kiwiSdrManager->stateDetail(profile.id);
            if (receiver.state == KiwiSdrClient::State::Connected
                && !m_kiwiSdrManager->waterfallAvailable(profile.id)) {
                receiver.detail = m_kiwiSdrManager->waterfallDetail(profile.id);
            }
            receiver.metadataSummary = kiwiReceiverMetadataSummary(
                m_kiwiSdrManager->receiverMetadata(profile.id));
            receiver.protocolSummary = kiwiProtocolSummary(
                m_kiwiSdrManager->protocolState(profile.id));
            const int sliceId = m_kiwiSdrManager->assignedSliceForProfile(profile.id);
            receiver.assignedSlice = m_radioModel ? m_radioModel->sliceById(sliceId)
                                                  : nullptr;
            receivers.append(receiver);
        }
    }

    m_kiwiSdrApplet->setReceivers(receivers);
}

// ── Die Verdrahtung ─────────────────────────────────────────────────
//
// Aethers wireKiwiSdr() ist rund vierhundert Zeilen und verbindet auch
// Ton, Wasserfall und Sendesperre. Hier steht nur, was bei uns ein
// Gegenueber hat. Jede einzelne Verbindung fuehrt auf dieselbe
// Auffrischung — das ist Absicht: die Bruecke liest den Manager jedes
// Mal vollstaendig neu, statt Teilzustaende mitzuschreiben, die
// auseinanderlaufen koennen.
void MainWindow::wireKiwiSdr()
{
    if (!m_kiwiSdrManager || !m_kiwiSdrApplet) {
        return;
    }

    // Aether holt das Rufzeichen vom RadioModel. Bei uns steht es nicht
    // dort, sondern seit jeher unter StationCallsign in den
    // Einstellungen — QsoMapWindow und RadioModel lesen es von genau
    // dort. Der KiwiSDR schickt es dem Betreiber des Empfaengers als
    // Kennung; leer ist zulaessig.
    m_kiwiSdrManager->setOperatorCallsign(
        AppSettings::instance()
            .value(QStringLiteral("StationCallsign"), QString{})
            .toString());

    connect(m_kiwiSdrManager, &KiwiSdrManager::profilesChanged,
            this, &MainWindow::refreshKiwiSdrAppletReceivers);
    connect(m_kiwiSdrManager, &KiwiSdrManager::profileStateChanged, this,
            [this](const QString&, KiwiSdrClient::State, const QString&) {
        refreshKiwiSdrAppletReceivers();
    });
    connect(m_kiwiSdrManager,
            &KiwiSdrManager::profileWaterfallAvailabilityChanged, this,
            [this](const QString&, bool, const QString&) {
        refreshKiwiSdrAppletReceivers();
    });
    connect(m_kiwiSdrManager, &KiwiSdrManager::profileTelemetryChanged, this,
            [this](const QString&, const KiwiSdrReceiverTelemetry&) {
        refreshKiwiSdrAppletReceivers();
    });
    connect(m_kiwiSdrManager, &KiwiSdrManager::sliceAssignmentChanged, this,
            [this](int, const QString&) {
        refreshKiwiSdrAppletReceivers();
    });

    // ── Stufe 6: die Wasserfallzeilen auf den Panadapter ─────────────
    //
    // Aether faedelt das ueber ReceivePresentationSync ein, um Geraet
    // und Kiwi im selben Takt zu zeigen. Das ist richtig, sobald BEIDE
    // gleichzeitig laufen — bei uns tut derzeit immer nur eine Quelle
    // etwas, denn der Panadapter verwirft die andere (siehe die Sperre
    // in updateSpectrumLinear). Solange das so ist, waere der
    // Gleichtakt ein Mechanismus ohne Aufgabe.
    //
    // Zugestellt wird an den Panadapter DER SCHEIBE, der dieses Profil
    // zugeordnet ist — nicht an den aktiven. Sonst landete der Kiwi im
    // Bild, sobald der Betreiber auf eine andere Scheibe klickt.
    connect(m_kiwiSdrManager, &KiwiSdrManager::waterfallRowReady, this,
            [this](const QString& profileId, const QString&,
                   const QVector<float>& binsDbm,
                   double lowFreqMhz, double highFreqMhz, quint32) {
        if (!m_kiwiSdrManager || profileId.isEmpty() || binsDbm.isEmpty()) {
            return;
        }
        const int sliceId = m_kiwiSdrManager->assignedSliceForProfile(profileId);
        SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId)
                                         : nullptr;
        if (!slice) { return; }
        SpectrumWidget* sw = spectrumForSlice(slice);
        if (!sw || !sw->kiwiDisplaySource()) { return; }
        sw->updateKiwiSpectrumDbm(binsDbm,
                                  lowFreqMhz * 1.0e6, highFreqMhz * 1.0e6);
    });

    refreshKiwiSdrAppletReceivers();
}

} // namespace Longpath
