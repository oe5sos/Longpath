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
#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/KiwiSdrManager.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QTimer>

#include <QStringList>

namespace Longpath {

namespace {

// Wie lange das Aufheben der Sendesperre wartet. 1,2 s deckt die
// uebliche Strecke zu einem KiwiSDR im Netz samt dessen eigener
// Pufferung ab. Kein Einstellwert: wer das feiner braucht, merkt es an
// einem hoerbaren Nachklang und meldet sich — ein Regler, den niemand
// versteht, ist schlechter als eine Zahl mit Begruendung.
constexpr int kKiwiResumeDelayMs = 1200;

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



// ── Stufe 7a: die Sendesperre ────────────────────────────────────────
//
// Waehrend wir senden, muss der Ton des KiwiSDR aus. Der Grund ist
// nicht Rueckkopplung — der Kiwi steht ja irgendwo im Netz —, sondern
// dass ein Empfaenger in Reichweite die eigene Aussendung hoert und
// sie einem mit ein bis zwei Sekunden Verzug ins Ohr legt. Wer so
// arbeitet, hoert sich selbst nachplappern.
//
// ── Und warum das Aufheben WARTET ───────────────────────────────────
//
// Genau derselbe Verzug macht das Wiedereinschalten heikel. Beim
// Loslassen der Taste ist der Ton, der gerade unterwegs ist, noch die
// eigene Aussendung. Schaltet man sofort auf, hoert man deren Ende —
// also genau das, was die Sperre verhindern sollte.
//
// Aethers Profil hat dafuer zwei Felder, die wir mitportiert haben:
// keepAudioDuringTx (dieser Empfaenger bleibt hoerbar) und
// resumeAudioAfterTxDelay (das Aufheben wartet die Streckenlaufzeit
// ab). Beide werden hier zum ersten Mal beachtet.
//
// ── Was hier NICHT steht ────────────────────────────────────────────
//
// Aethers KiwiSdrTxMuteLatch unterscheidet Aussendungen, die DIESER
// Rechner ausgeloest hat, von fremden (VOX, CAT, ein anderer Client) —
// weil bei FLEX mehrere Clients an einem Geraet haengen koennen und
// nur die gemeldete Verriegelung von fremden Aussendungen weiss.
//
// Bei HPSDR gibt es diesen Fall nicht: wir sind der einzige Client,
// und MOX beziehungsweise TUNE IST der Sendezustand. Die Verriegelung
// hat keine zweite Quelle, die der Riegel gegeneinander abwaegen
// koennte. Er ist darum portiert und geprueft, aber hier bewusst
// nicht verdrahtet — ein Mechanismus ohne zweite Eingangsgroesse
// waere eine Verkomplizierung ohne Wirkung.
void MainWindow::syncKiwiSdrTransmitMute()
{
    if (!m_kiwiSdrManager || !m_radioModel) { return; }
    AudioEngine* audio = m_radioModel->audioEngine();
    if (!audio) { return; }

    const TransmitModel& tx = m_radioModel->transmitModel();
    const bool sending = tx.isMox() || tx.isTune();

    for (const KiwiSdrAntennaProfile& profile : m_kiwiSdrManager->profiles()) {
        const int sliceId =
            m_kiwiSdrManager->assignedSliceForProfile(profile.id);
        if (sliceId < 0) { continue; }

        if (sending) {
            if (profile.keepAudioDuringTx) { continue; }
            audio->setKiwiSdrAudioSourceEnabled(sliceId, false);
            continue;
        }

        // Nicht mehr am Senden. Aufheben — sofort oder verzoegert.
        if (!profile.resumeAudioAfterTxDelay) {
            audio->setKiwiSdrAudioSourceEnabled(sliceId, true);
            continue;
        }
        const QString id = profile.id;
        QTimer::singleShot(kKiwiResumeDelayMs, this, [this, id, sliceId]() {
            // Beim Ablauf noch einmal pruefen: in der Wartezeit kann
            // laengst wieder gesendet werden, und dann waere das
            // Aufheben genau falsch. Ebenso kann das Profil weg sein.
            if (!m_kiwiSdrManager || !m_radioModel) { return; }
            if (!m_kiwiSdrManager->hasProfile(id)) { return; }
            const TransmitModel& t = m_radioModel->transmitModel();
            if (t.isMox() || t.isTune()) { return; }
            if (AudioEngine* a = m_radioModel->audioEngine()) {
                a->setKiwiSdrAudioSourceEnabled(sliceId, true);
            }
        });
    }
}

// ── Einen Empfaenger aufnehmen, verbinden und zuordnen ───────────────
//
// Drei Schritte, die zusammengehoeren und die der Betreiber sonst
// einzeln machen muesste: anlegen, verbinden, an eine Scheibe haengen.
// Wer im Menue "Oeffentliche Empfaenger" waehlt, will hoeren — nicht
// eine Zeile in einer Liste.
//
// Die Zuordnung geht an die AKTIVE Scheibe. Das ist die einzige, bei
// der man sagen kann, was der Betreiber gemeint hat.
void MainWindow::addKiwiSdrReceiver(const QString& name,
                                    const QString& endpoint)
{
    if (!m_kiwiSdrManager || endpoint.trimmed().isEmpty()) {
        return;
    }

    const QString label = name.trimmed().isEmpty()
                              ? endpoint.trimmed()
                              : name.trimmed();
    const QString id = m_kiwiSdrManager->addProfile(label, endpoint.trimmed());
    if (id.isEmpty()) {
        qCWarning(lcKiwiSdr) << "KiwiSDR konnte nicht angelegt werden:"
                             << endpoint;
        return;
    }

    // ── Welche Scheibe? ─────────────────────────────────────────────
    //
    // Die aktive, wenn es eine gibt. Gibt es keine — und das ist der
    // Normalfall OHNE verbundenes Funkgeraet —, dann die erste.
    //
    // Der Rueckfall ist nicht Bequemlichkeit, sondern der halbe Zweck
    // der Sache: ein KiwiSDR ist gerade dann interessant, wenn kein
    // eigenes Geraet laeuft. Ohne ihn landete der Empfaenger bei
    // niemandem, und der Ton haette keinen Weg in die Mischung —
    // stillschweigend, denn verbinden wuerde er trotzdem. Genau das
    // hat die Pruefung tst_kiwi_tx_mute im ersten Lauf gezeigt.
    SliceModel* slice = m_radioModel ? m_radioModel->activeSlice() : nullptr;
    if (!slice && m_radioModel) {
        const QList<SliceModel*> slices = m_radioModel->slices();
        if (!slices.isEmpty()) {
            slice = slices.first();
        } else {
            // ── Ohne Funkgeraet gibt es GAR KEINE Scheibe ───────────
            //
            // Nachgemessen am 2026-08-23: ein frisches Hauptfenster
            // ohne Verbindung hat null Scheiben — sie entstehen erst
            // beim Verbinden. Der Rueckfall auf "die erste" lief also
            // ins Leere, und der Empfaenger verband sich, ohne dass
            // sein Ton je irgendwo ankam. Stillschweigend, denn die
            // Verbindung selbst gelang ja.
            //
            // Ein KiwiSDR IST ein Empfaenger, und ein Empfaenger
            // braucht einen Platz. Also wird einer angelegt. Das ist
            // gerade der Fall, um den es geht: ein Kiwi ist dann am
            // interessantesten, wenn kein eigenes Geraet laeuft.
            const int id = m_radioModel->addSlice();
            slice = m_radioModel->sliceById(id);
            qCInfo(lcKiwiSdr) << "Keine Scheibe vorhanden — fuer den "
                                 "KiwiSDR eine angelegt, Kennung" << id;
        }
    }
    if (slice) {
        m_kiwiSdrManager->assignSliceToProfile(
            slice->sliceIndex(), id,
            slice->frequency() / 1.0e6,
            SliceModel::modeName(slice->dspMode()),
            slice->filterLow(), slice->filterHigh(),
            slice->panKey(),
            QString(),   // Bandname: der Kiwi braucht ihn nur fuer den
                         // Bandrueckruf, und der ist Stufe 7.
            0);
    }

    m_kiwiSdrManager->connectProfile(id);
    refreshKiwiSdrAppletReceivers();

    qCInfo(lcKiwiSdr).nospace()
        << "KiwiSDR aufgenommen: " << label << " (" << endpoint << ")"
        << (slice ? QStringLiteral(" -> Scheibe %1").arg(slice->sliceIndex())
                  : QStringLiteral(" OHNE SCHEIBE — sein Ton hat keinen "
                                   "Weg in die Mischung"));
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

    // Die Sendesperre haengt an denselben zwei Signalen wie alles
    // andere Sendebezogene.
    connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
            this, [this](bool) { syncKiwiSdrTransmitMute(); });
    connect(&m_radioModel->transmitModel(), &TransmitModel::tuneChanged,
            this, [this](bool) { syncKiwiSdrTransmitMute(); });

    refreshKiwiSdrAppletReceivers();
}

} // namespace Longpath
