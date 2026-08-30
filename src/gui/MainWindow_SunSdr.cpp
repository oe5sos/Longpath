// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/MainWindow_SunSdr.cpp  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Verbindungsseite fuer den SunSDR2 QRP ────────────────────────
//
// Schritt 1 (TciClient, siehe src/core/TciClient.h) und Schritt 2a
// (der Ton-Haken in AudioEngine, siehe dort) standen schon. Hier
// kommt beides zusammen: eine Adresse eingeben, verbinden, Ton hoeren.
//
// Bewusst einfach gehalten (Betreiberentscheidung 2026-08-24, "1" von
// zwei angebotenen Wegen): EIN SunSDR, fest angeschlossen, kein
// Profilsystem wie bei KiwiSdrManager. Wer mehrere TCI-Geraete
// gleichzeitig verwalten will, braucht das hier NICHT als Vorlage --
// das waere ein eigener Entwurf.
//
// ── Welche Scheibe? ───────────────────────────────────────────────
//
// Derselbe Rueckfall wie beim KiwiSDR (siehe addKiwiSdrReceiver in
// MainWindow_KiwiSdr.cpp) und aus demselben, zweimal bestaetigten
// Grund: ein frisches MainWindow OHNE verbundenes Funkgeraet hat NULL
// Scheiben, sie entstehen erst beim Verbinden. Ein SunSDR ist gerade
// dann interessant, wenn kein eigenes Geraet laeuft -- also: aktive
// Scheibe, sonst die erste, sonst eine anlegen.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-24 — Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/MainWindow.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/FFTEngine.h"
#include "core/FFTRouter.h"
#include "core/LogCategories.h"
#include "core/TciClient.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QMessageBox>
#include <QMetaObject>

#include <cmath>

namespace Longpath {

namespace {

constexpr quint16 kDefaultSunSdrPort = 40001;
// TCI-seitig ist der Empfaenger immer 0 -- ein SunSDR2 QRP hat genau
// einen. Kein Feld dafuer in den Einstellungen: es gibt hier nichts zu
// waehlen.
constexpr int kSunSdrReceiverIndex = 0;

// Der Pseudo-"Stream-Index" fuer den SunSDR-Panadapter -- bewusst
// ausserhalb jedes realen DDC-Pools reserviert (Rechercheergebnis
// 2026-08-24). m_fftEngines, m_streamWindows, m_streamNoiseFloors und
// FFTRouter sind alle ueber denselben int indiziert wie echte
// DDC-Streams (0..userDdcCount-1, real hoechstens im niedrigen
// zweistelligen Bereich -- n1gp-Anvelina_PROIII Orion.v:958, NR bis 14
// -- und alle als QMap/QHash gefuehrt, kein Feld mit fester Groesse,
// darum ist ein grosser Wert hier unbedenklich). Wuerde SunSDR
// versehentlich einen Index nehmen, den ein spaeter verbundenes echtes
// Funkgeraet ebenfalls beansprucht, koennten sich zwei voellig
// unabhaengige I/Q-Quellen denselben FFTEngine/Panadapter teilen.
//
// Bewusst NICHT der Weg, der SunSDR-Scheibe selbst einen streamIndex
// zu geben, damit sie im bestehenden Router-Umbau (rebuildFftRouting)
// einfach mitlaeuft: SliceModel::streamIndex() >= 0 wird an mindestens
// neun Stellen in RadioModel.cpp als "diese Scheibe hat einen echten
// WDSP-Kanal" gelesen. Der SunSDR-Scheibe diesen Wert zu geben haette
// an all diesen Stellen unbekannte Nebenwirkungen ausloesen koennen,
// ohne jede einzelne zu pruefen. Die Scheibe bleibt darum bei -1
// (unveraendert), und reassertSunSdrRouterMapping() haelt die
// Router-Zuordnung von aussen am Leben (siehe dort).
constexpr int kSunSdrPseudoStreamIndex = 100000;

// Anders als KiwiSdrClient::parseEndpoint (privat, und verlangt einen
// Port) hat TCI einen bekannten Standardport -- eine blosse Adresse
// ohne ":Port" ist darum kein Fehler, sondern der Normalfall.
bool parseSunSdrEndpoint(const QString& endpoint, QString* host, quint16* port)
{
    const QString trimmed = endpoint.trimmed();
    if (trimmed.isEmpty()) { return false; }

    const int colon = trimmed.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0) {
        if (host) { *host = trimmed; }
        if (port) { *port = kDefaultSunSdrPort; }
        return true;
    }

    const QString parsedHost = trimmed.left(colon).trimmed();
    const QString parsedPortText = trimmed.mid(colon + 1).trimmed();
    if (parsedHost.isEmpty() || parsedPortText.isEmpty()) { return false; }

    bool ok = false;
    const int parsedPort = parsedPortText.toInt(&ok);
    if (!ok || parsedPort <= 0 || parsedPort > 65535) { return false; }

    if (host) { *host = parsedHost; }
    if (port) { *port = static_cast<quint16>(parsedPort); }
    return true;
}

// DSPMode -> TCI-Wortlaut, klein geschrieben wie am Draht gemessen
// ("modulations_list:am,sam,dsb,lsb,usb,cw,nfm,digl,digu,wfm,drm",
// tci_probe gegen ExpertSDR2, 2026-08-24).
//
// Bench-gefunden (OE5SOS, 2026-08-24): eine erste Fassung sendete
// "cwl"/"cwu" statt dem allgemeinen "cw" -- genau der Wortlaut, den die
// Selbstauskunft NICHT als eigenen Wert kennt (nur "cw" steht in der
// Liste). ExpertSDR2 hat den Modus darum nicht uebernommen, obwohl der
// Befehl korrekt am Draht ankam. CWL und CWU bilden darum beide auf das
// allgemeine "cw" ab -- ExpertSDR2 unterscheidet die Seitenbandwahl bei
// CW ueberhaupt nicht ueber TCI (siehe dspModeForTciModeString unten
// fuer die Kehrseite dieser Vereinfachung). SPEC/RADE_U/RADE_L haben
// keine TCI-Entsprechung -- leerer String, der Aufrufer verwirft dann
// statt zu senden (siehe wireSunSdrOutboundControl).
QString tciModeStringForDspMode(Longpath::DSPMode mode)
{
    switch (mode) {
    case Longpath::DSPMode::LSB:  return QStringLiteral("lsb");
    case Longpath::DSPMode::USB:  return QStringLiteral("usb");
    case Longpath::DSPMode::DSB:  return QStringLiteral("dsb");
    case Longpath::DSPMode::CWL:
    case Longpath::DSPMode::CWU:
        return QStringLiteral("cw");
    case Longpath::DSPMode::FM:   return QStringLiteral("nfm");
    case Longpath::DSPMode::AM:   return QStringLiteral("am");
    case Longpath::DSPMode::DIGU: return QStringLiteral("digu");
    case Longpath::DSPMode::DIGL: return QStringLiteral("digl");
    case Longpath::DSPMode::SAM:  return QStringLiteral("sam");
    case Longpath::DSPMode::DRM:  return QStringLiteral("drm");
    case Longpath::DSPMode::SPEC:
    case Longpath::DSPMode::RADE_U:
    case Longpath::DSPMode::RADE_L:
        return QString();
    }
    return QString();
}

// Kehrfunktion, klein -> DSPMode. "cwl"/"cwu" bleiben erkannt (falls sie
// je von einem ANDEREN TCI-Geraet als ExpertSDR2 kommen, siehe
// TciProtocol.cpp handleModulationCommand), aber das allgemeine "cw"
// braucht besondere Behandlung -- siehe applyRemoteSunSdrModulation:
// naiv immer auf CWL abzubilden wuerde nach dem oben verflachten
// ausgehenden "cw" die eigene CWU-Wahl bei jeder Rueckmeldung von
// ExpertSDR2 still auf CWL zurueckdrehen.
Longpath::DSPMode dspModeForTciModeString(const QString& mode, bool* ok)
{
    if (ok) { *ok = true; }
    if (mode == QStringLiteral("lsb"))  { return Longpath::DSPMode::LSB; }
    if (mode == QStringLiteral("usb"))  { return Longpath::DSPMode::USB; }
    if (mode == QStringLiteral("dsb"))  { return Longpath::DSPMode::DSB; }
    if (mode == QStringLiteral("cw"))   { return Longpath::DSPMode::CWL; }
    if (mode == QStringLiteral("cwl"))  { return Longpath::DSPMode::CWL; }
    if (mode == QStringLiteral("cwu"))  { return Longpath::DSPMode::CWU; }
    if (mode == QStringLiteral("nfm")
        || mode == QStringLiteral("fm")) { return Longpath::DSPMode::FM; }
    if (mode == QStringLiteral("am"))   { return Longpath::DSPMode::AM; }
    if (mode == QStringLiteral("sam"))  { return Longpath::DSPMode::SAM; }
    if (mode == QStringLiteral("digu")) { return Longpath::DSPMode::DIGU; }
    if (mode == QStringLiteral("digl")) { return Longpath::DSPMode::DIGL; }
    if (mode == QStringLiteral("drm"))  { return Longpath::DSPMode::DRM; }
    if (ok) { *ok = false; }
    return Longpath::DSPMode::USB;
}

// Nicht-blockierend (2026-08-24, nach einem Testabsturz): QMessageBox::
// information/warning sind bequeme Aufrufe um exec(), das die ganze
// Anwendung anhaelt, bis jemand klickt. Im automatisierten Testlauf klickt
// niemand -- tst_sunsdr_connect_wiring lief deshalb in eine 486-Sekunden-
// Zeitueberschreitung: genau diese Meldung ging mitten im Verbindungs-
// aufbau auf und wartete auf einen Klick, der nie kam. Eine Statusmeldung
// sollte ausserdem auch fuer einen echten Bediener nicht die ganze
// Anwendung einfrieren.
void showSunSdrNotice(QWidget* parent, QMessageBox::Icon icon,
                      const QString& title, const QString& text)
{
    auto* box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setModal(false);
    box->show();
}

} // namespace

int MainWindow::sunSdrPseudoStreamIndexForTest()
{
    return kSunSdrPseudoStreamIndex;
}

void MainWindow::connectSunSdr(const QString& endpoint)
{
    QString host;
    quint16 port = 0;
    if (!parseSunSdrEndpoint(endpoint, &host, &port)) {
        showSunSdrNotice(
            this, QMessageBox::Warning, QStringLiteral("SunSDR verbinden"),
            QStringLiteral("Adresse unlesbar: „%1“\n\n"
                           "Erwartet wird eine IP-Adresse oder ein "
                           "Rechnername, wahlweise mit :Port.")
                .arg(endpoint));
        return;
    }

    AppSettings::instance().setValue(
        QStringLiteral("SunSdrEndpoint"),
        QStringLiteral("%1:%2").arg(host).arg(port));

    if (!m_radioModel) { return; }

    // ── Welche Scheibe? Siehe Kopf dieser Datei. ─────────────────────
    //
    // Bench-gefunden (Grundgeruest-Durchsicht 2026-08-24, nach Bau von
    // "Steuerung"): der Rueckfall nahm die aktive Scheibe blind, auch
    // wenn sie eine echte DDC-Bindung hatte (streamIndex() >= 0 -- ein
    // ECHTES, moeglicherweise sendefaehiges Funkgeraet). Waere ein
    // solches Funkgeraet gerade aktiv verbunden, haette SunSDR dessen
    // Ton UND Panadapter still ueberschrieben -- die Steuerungsrichtung
    // war schon durch wireSunSdrOutboundControl abgesichert, Ton/Bild
    // aber nicht. Der Rueckfall ueberspringt darum jede echte Scheibe:
    // aktive Scheibe NUR wenn ungebunden, sonst die erste ungebundene,
    // sonst eine neue anlegen.
    SliceModel* slice = m_radioModel->activeSlice();
    if (slice && slice->streamIndex() >= 0) { slice = nullptr; }
    if (!slice) {
        for (SliceModel* candidate : m_radioModel->slices()) {
            if (candidate && candidate->streamIndex() < 0) {
                slice = candidate;
                break;
            }
        }
    }
    if (!slice) {
        // suppressAutoStreamBinding=true: this slice is about to be fed by
        // SunSDR's mirrored VFO, not an operator retuning a real radio --
        // see RadioModel::addSlice's parameter doc and
        // SliceModel::m_suppressAutoStreamBinding for the bug this avoids
        // (bench-found 2026-08-24: the very first applyRemoteSunSdrFrequency
        // call otherwise handed the fresh slice a real streamIndex(), and
        // the streamIndexChanged watch below evicted SunSDR from it within
        // the same event).
        const int id = m_radioModel->addSlice(QString(), true);
        slice = m_radioModel->sliceById(id);
        qCInfo(lcTci) << "SunSDR: keine ungebundene Scheibe vorhanden -- "
                         "eine angelegt, Kennung" << id;
    }
    if (!slice) {
        qCWarning(lcTci) << "SunSDR: keine Scheibe zu bekommen -- der Ton "
                            "haette keinen Weg in die Mischung";
        showSunSdrNotice(
            this, QMessageBox::Warning, QStringLiteral("SunSDR verbinden"),
            QStringLiteral("Es konnte keine Scheibe angelegt werden. "
                           "Der SunSDR wird nicht verbunden."));
        return;
    }
    // Auch fuer die beiden Rueckfall-Zweige oben (wiederverwendete aktive
    // oder erste ungebundene Scheibe) -- addSlice()s Parameter deckt nur
    // den dritten, neu angelegten Fall ab. Ohne dies wuerde jede
    // wiederverwendete, bereits vorhandene Scheibe (der haeufigere Fall:
    // die schon vorhandene, nie mit einem echten Funkgeraet verbundene
    // Scheibe A) beim ersten applyRemoteSunSdrFrequency()-Aufruf immer noch
    // in den echten Platzierungspfad laufen.
    slice->setSuppressAutoStreamBinding(true);
    m_sunSdrTargetSliceId = slice->sliceIndex();

    if (!m_sunSdrClient) {
        m_sunSdrClient = new TciClient(this);
        wireSunSdr();
    }

    // Schritt "Steuerung": die Ausgangsseite haengt an der KONKRETEN
    // Scheibe, nicht am Client -- muss darum bei jedem Verbindungsaufbau
    // neu gesetzt werden, weil sich die Ziel-Scheibe zwischen zwei
    // "Verbinden…"-Klicks aendern kann (siehe wireSunSdrOutboundControl).
    wireSunSdrOutboundControl(slice);

    qCInfo(lcTci).nospace() << "SunSDR: verbinde mit " << host << ":" << port
                            << " -> Scheibe " << m_sunSdrTargetSliceId;
    m_sunSdrClient->connectToEndpoint(host, port);
}

void MainWindow::disconnectSunSdr()
{
    if (m_sunSdrClient) {
        m_sunSdrClient->disconnectFromEndpoint();
    }
    if (m_radioModel && m_sunSdrTargetSliceId >= 0) {
        if (AudioEngine* audio = m_radioModel->audioEngine()) {
            audio->removeSunSdrAudioSource(m_sunSdrTargetSliceId);
        }
    }
    disconnect(m_sunSdrFreqOutConn);
    disconnect(m_sunSdrModeOutConn);
    disconnect(m_sunSdrStreamIndexWatchConn);
}

// Siehe MainWindow.h fuer den Zweck: die EINE Stelle, die "gibt es eine
// SunSDR-Zielscheibe und darf sie gerade gesteuert werden" beantwortet.
// Vorher pruefte jeder Verbraucher das von Hand -- Ton- und Panadapter-
// Pfad hatten die streamIndex()-Schranke schlicht vergessen (Bench-Fund
// 2026-08-24, Grundgeruest-Durchsicht). Ein neuer Verbraucher kann diesen
// Fehler jetzt nicht mehr machen, weil er keine Wahl hat.
SliceModel* MainWindow::sunSdrControllableSlice() const
{
    if (!m_radioModel || m_sunSdrTargetSliceId < 0) { return nullptr; }
    SliceModel* slice = m_radioModel->sliceById(m_sunSdrTargetSliceId);
    if (!slice || slice->streamIndex() >= 0) { return nullptr; }
    return slice;
}

void MainWindow::releaseSunSdrSlice(const QString& reason)
{
    if (m_sunSdrTargetSliceId < 0) { return; }
    qCWarning(lcTci) << "SunSDR: Scheibe" << m_sunSdrTargetSliceId
                     << "wird freigegeben --" << reason
                     << "-- erneut \"Verbinden…\" waehlen fuer eine neue "
                        "Zielscheibe.";
    if (m_radioModel) {
        if (AudioEngine* audio = m_radioModel->audioEngine()) {
            audio->removeSunSdrAudioSource(m_sunSdrTargetSliceId);
        }
        if (auto* router = m_radioModel->fftRouter()) {
            router->removeReceiver(kSunSdrPseudoStreamIndex);
        }
        // Explicit-disconnect path: bindUnboundSlices() already cleared
        // this when a real radio triggered the release (streamIndexChanged
        // fires from inside it, before this function runs), so this is a
        // harmless no-op there. Needed here for the other release reasons
        // -- an outright "Trennen" -- where the slice survives, unbound,
        // and must go back to answering an operator's own retune with a
        // real placement like any other slice, not stay silently suppressed.
        if (SliceModel* slice = m_radioModel->sliceById(m_sunSdrTargetSliceId)) {
            slice->setSuppressAutoStreamBinding(false);
        }
    }
    disconnect(m_sunSdrFreqOutConn);
    disconnect(m_sunSdrModeOutConn);
    disconnect(m_sunSdrStreamIndexWatchConn);
    m_sunSdrTargetSliceId = -1;
}

void MainWindow::wireSunSdrOutboundControl(SliceModel* slice)
{
    disconnect(m_sunSdrFreqOutConn);
    disconnect(m_sunSdrModeOutConn);
    disconnect(m_sunSdrStreamIndexWatchConn);
    if (!slice) { return; }

    // Sicherheitsschranke (gegengeprueft 2026-08-24 vor dem Bau, siehe
    // Recherche zur Frequenz-/Modusweiche in RadioModel.cpp): eine
    // Scheibe MIT echter DDC-Bindung (streamIndex() >= 0) gehoert einem
    // ECHTEN, moeglicherweise sendefaehigen Funkgeraet. Wuerde diese
    // Verdrahtung auch dort greifen, koennte ein fremdes TCI-Geraet
    // (SunSDR/ExpertSDR2) das echte Funkgeraet stumm umstimmen. Seit dem
    // Rueckfall in connectSunSdr() (siehe dort) sollte das hier nie mehr
    // zutreffen -- die Pruefung bleibt trotzdem als zweite, unabhaengige
    // Schranke stehen. "Technik Nereus, Design ich" ist eine
    // Gestaltungsregel -- diese hier ist keine, sie ist die
    // Sicherheitsgrenze aus CLAUDE.local.md: "Wo Zurueckhaltung und
    // Sicherheit sich widersprechen, gewinnt die Sicherheit."
    if (slice->streamIndex() >= 0) {
        qCInfo(lcTci) << "SunSDR: Ziel-Scheibe hat eine echte DDC-Bindung "
                         "-- Ausgangssteuerung bleibt aus, um kein echtes "
                         "Funkgeraet stumm mitzusteuern";
        return;
    }

    m_sunSdrFreqOutConn = connect(slice, &SliceModel::frequencyChanged, this,
                                  [this, slice](double hz) {
        if (m_sunSdrApplyingRemoteState || !m_sunSdrClient) { return; }
        // Erneut gepruefte Schranke, nicht nur beim Anschliessen (Bench-
        // Fund 2026-08-24): eine anfangs ungebundene Scheibe kann SPAETER
        // eine echte DDC-Bindung bekommen (bindUnboundSlices() beim
        // naechsten Verbinden eines echten Funkgeraets, RadioModel.cpp).
        // Der streamIndexChanged-Wachposten unten faengt das eigentlich
        // sofort ab und trennt diese Verbindung ganz -- diese Zeile ist
        // die zweite, unabhaengige Schranke fuer den Fall, dass beide
        // Signale in derselben Runde feuern und die Reihenfolge nicht
        // garantiert ist.
        if (slice->streamIndex() >= 0) { return; }
        m_sunSdrClient->setVfoFrequency(
            kSunSdrReceiverIndex, 0, qint64(std::llround(hz)));
    });
    m_sunSdrModeOutConn = connect(slice, &SliceModel::dspModeChanged, this,
                                  [this, slice](Longpath::DSPMode mode) {
        if (m_sunSdrApplyingRemoteState || !m_sunSdrClient) { return; }
        if (slice->streamIndex() >= 0) { return; }
        const QString tciMode = tciModeStringForDspMode(mode);
        if (tciMode.isEmpty()) {
            qCWarning(lcTci) << "SunSDR: Modus" << int(mode)
                             << "hat keine TCI-Entsprechung, nicht gesendet";
            return;
        }
        m_sunSdrClient->setModulation(kSunSdrReceiverIndex, tciMode);
    });

    // Uebernimmt spaeter ein echtes Funkgeraet diese Scheibe
    // (bindUnboundSlices() beim naechsten Verbinden), muss SunSDR sie
    // GANZ freigeben -- nicht nur die Steuerung stumm schalten, sondern
    // auch Ton und Panadapter (siehe releaseSunSdrSlice). Ohne diesen
    // Wachposten haette nur die Steuerungsrichtung eine Bremse gehabt;
    // Ton und Panadapter liefen an der jetzt echten Scheibe unbemerkt
    // weiter (Bench-Fund 2026-08-24, Grundgeruest-Durchsicht).
    m_sunSdrStreamIndexWatchConn = connect(
        slice, &SliceModel::streamIndexChanged, this, [this](int newIndex) {
        if (newIndex < 0) { return; }
        releaseSunSdrSlice(QStringLiteral(
            "ein echtes Funkgeraet hat sie inzwischen uebernommen"));
    });
}

void MainWindow::applyRemoteSunSdrFrequency(qint64 hz)
{
    SliceModel* slice = sunSdrControllableSlice();
    if (!slice) { return; }
    m_sunSdrApplyingRemoteState = true;
    slice->setFrequency(static_cast<double>(hz));
    m_sunSdrApplyingRemoteState = false;
}

void MainWindow::applyRemoteSunSdrModulation(const QString& mode)
{
    SliceModel* slice = sunSdrControllableSlice();
    if (!slice) { return; }

    // ExpertSDR2 kennt am Draht nur das allgemeine "cw", nicht cwl/cwu
    // (siehe tciModeStringForDspMode) -- ein zurueckgemeldetes "cw" nach
    // dem eigenen ausgehenden CWU-Befehl waere sonst genau das
    // Echo-Problem, das m_sunSdrApplyingRemoteState eigentlich verhindern
    // soll, nur eine Stufe subtiler: nicht derselbe Wert kommt zurueck,
    // sondern ein VERALLGEMEINERTER, der die eigene Seitenbandwahl
    // verwaesserte. Steht die Scheibe schon auf CWL/CWU, bleibt sie das
    // bei einem eingehenden "cw" -- nur ein FRISCHES cwl/cwu (von einem
    // TCI-Geraet, das die Unterscheidung tatsaechlich sendet) oder ein
    // "cw" auf eine Scheibe, die noch nicht in CW ist, aendert etwas.
    if (mode == QStringLiteral("cw")
        && (slice->dspMode() == Longpath::DSPMode::CWL
            || slice->dspMode() == Longpath::DSPMode::CWU)) {
        return;
    }

    bool ok = false;
    const Longpath::DSPMode dspMode = dspModeForTciModeString(mode, &ok);
    if (!ok) {
        qCWarning(lcTci) << "SunSDR: unbekannte Betriebsart" << mode
                         << "-- Scheibe unveraendert";
        return;
    }
    m_sunSdrApplyingRemoteState = true;
    slice->setDspMode(dspMode);
    m_sunSdrApplyingRemoteState = false;
}

// ── Die Verdrahtung ─────────────────────────────────────────────────
//
// Einmalig aufgerufen, wenn m_sunSdrClient zum ersten Mal angelegt
// wird (siehe connectSunSdr oben) -- nicht bei jedem Verbindungsaufbau
// neu, sonst haeuften sich die Verbindungen bei jedem erneuten
// "Verbinden" im Menue.
void MainWindow::wireSunSdr()
{
    if (!m_sunSdrClient) { return; }

    // Wird die Ziel-Scheibe geloescht, waehrend SunSDR verbunden ist,
    // muss die Zuordnung sofort weg -- sonst bliebe m_sunSdrTargetSliceId
    // auf einer Kennung stehen, die addSlice() (niedrigste freie Kennung
    // zuerst, siehe dort) einer voellig ANDEREN, spaeter angelegten
    // Scheibe wiedergeben koennte. Diese neue Scheibe wuerde dann
    // stillschweigend SunSDR-Ton/-Panadapter/-Steuerung bekommen, obwohl
    // niemand sie je damit verbunden hat (Bench-Fund 2026-08-24,
    // Grundgeruest-Durchsicht). Einmalig hier angeschlossen (wireSunSdr
    // laeuft nur beim allerersten Verbinden), liest m_sunSdrTargetSliceId
    // aber bei jedem Aufruf frisch -- deckt also auch spaetere
    // Wiederverbindungen ab, nicht nur die erste.
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::sliceRemoved, this,
                [this](int index) {
            if (index != m_sunSdrTargetSliceId) { return; }
            releaseSunSdrSlice(QStringLiteral(
                "sie wurde geloescht"));
        });
    }

    // Die Selbstauskunft ist zu Ende: jetzt Ton UND I/Q anfordern und die
    // Scheibe fuer SunSDR-Ton freischalten. TciClient::start{Audio,Iq}
    // Stream senden den Befehl unabhaengig davon, ob der Empfaenger in
    // ExpertSDR2 gerade laeuft (docs/TCI-SunSDR-gemessen.md) -- solange
    // er steht, kommt einfach kein Rahmen, und weder feedSunSdrAudioData
    // noch die FFTEngine (siehe Bild-Verdrahtung unten) werden aufgerufen.
    // Kein Sonderfall noetig.
    //
    // Bench-gefunden, 2026-08-24: dieser startIqStream()-Aufruf fehlte
    // in der ersten Fassung -- Ton kam an (startAudioStream stand schon
    // da), der Panadapter aber blieb schwarz. Die Mittenfrequenz stimmte
    // trotzdem, weil "dds:" unabhaengig vom Stream-Status aus der
    // Selbstauskunft kommt -- das verdeckte den fehlenden Aufruf beim
    // ersten Blick auf den Bediener-Bildschirm.
    connect(m_sunSdrClient, &TciClient::deviceDescribed, this,
            [this](const QString& deviceName) {
        qCInfo(lcTci) << "SunSDR beschrieben:" << deviceName;
        if (!m_sunSdrClient) { return; }
        m_sunSdrClient->startAudioStream(kSunSdrReceiverIndex);
        m_sunSdrClient->startIqStream(kSunSdrReceiverIndex);
        // sunSdrControllableSlice() statt der Kennung roh zu pruefen (Bench-
        // Fund 2026-08-24): eine Scheibe, die inzwischen ein echtes
        // Funkgeraet uebernommen hat, darf keinen SunSDR-Ton bekommen --
        // vorher fehlte diese Schranke hier komplett, obwohl sie fuer die
        // Steuerungsrichtung schon stand.
        if (sunSdrControllableSlice()) {
            if (AudioEngine* audio = m_radioModel->audioEngine()) {
                // AudioEngine::start() (Lautsprecher-Ausgang oeffnen, den
                // Abfluss aus MasterMixer anlaufen lassen) wird im ganzen
                // Programm nur an EINER Stelle aufgerufen: nach dem
                // erfolgreichen Verbinden mit einem ECHTEN Funkgeraet
                // (RadioModel.cpp, WDSP-Init-Rueckruf). Ohne verbundenes
                // Funkgeraet laeuft der Ausgang also nie an -- der
                // SunSDR-Ton wuerde korrekt in die Mischung geschrieben,
                // aber nie abgeholt. Genau der Fall, fuer den SunSDR (und
                // KiwiSDR) gebaut sind: interessant gerade OHNE eigenes
                // Geraet. start() schuetzt sich selbst gegen doppeltes
                // Anlaufen (if (m_running) { return; }), darum gefahrlos
                // hier zusaetzlich aufgerufen -- ist der Ausgang schon
                // durch ein echtes Funkgeraet gestartet, passiert nichts.
                audio->start();
                audio->setSunSdrAudioSourceEnabled(m_sunSdrTargetSliceId, true);
            }
        }
        // Erste sichtbare Rueckmeldung ueberhaupt (2026-08-24, nach OE5SOS'
        // "verbindet nicht" -- die Verbindung stand da tatsaechlich schon,
        // nur gab es NICHTS im Fenster, das das gezeigt haette). Kein
        // laestiges Wiederholen: deviceDescribed feuert genau einmal je
        // "Verbinden…"-Klick, TciClient baut nicht von selbst neu auf.
        showSunSdrNotice(
            this, QMessageBox::Information, QStringLiteral("SunSDR verbunden"),
            deviceName.isEmpty()
                ? QStringLiteral("Verbunden. Sobald der Empfaenger in "
                                 "ExpertSDR2 laeuft, kommt Ton.")
                : QStringLiteral("Verbunden mit „%1“.\n\nSobald der "
                                 "Empfaenger in ExpertSDR2 laeuft, kommt Ton.")
                      .arg(deviceName));
    });

    connect(m_sunSdrClient, &TciClient::audioFrameReady, this,
            [this](int receiver, int sampleRate, int channels,
                   const std::vector<float>& interleaved) {
        // Derselbe Empfaenger-Filter wie beim Panadapter (siehe dort) --
        // ExpertSDR2 sendet Binaerrahmen zwar nur fuer tatsaechlich
        // angeforderte Stroeme (gemessen: nie Empfaenger-1-Rahmen ohne
        // eigenen iq_start:1/audio_start:1), aber TCI ist ein
        // Rundruf-Protokoll -- ein ANDERER, gleichzeitig verbundener
        // TCI-Client (z.B. WSJT-X an Empfaenger 1), der seinerseits
        // Stroeme anfordert, wuerde dessen Rahmen ebenso an Longpath
        // liefern. Ohne Filter mischte sich dessen Ton in den SunSDR-
        // Lautsprecherausgang.
        if (receiver != kSunSdrReceiverIndex) { return; }
        if (!sunSdrControllableSlice()) { return; }
        if (channels != 2) {
            // feedSunSdrAudioData setzt verschraenktes Stereo voraus
            // (siehe AudioEngine.h). TciClient leitet die Kanalzahl aus
            // der Stromart ab und liefert bei RX-Ton immer 2 (siehe
            // TciClient.cpp, assumedChannelsForStream) -- dieser Zweig
            // sollte darum nie laufen. Trotzdem nicht blind halbieren,
            // falls sich das je aendert.
            qCWarning(lcTci) << "SunSDR: Tonrahmen mit" << channels
                             << "Kanaelen verworfen, erwartet werden 2";
            return;
        }
        AudioEngine* audio = m_radioModel->audioEngine();
        if (!audio) { return; }
        audio->feedSunSdrAudioData(m_sunSdrTargetSliceId, interleaved.data(),
                                   int(interleaved.size() / 2), sampleRate);
    });

    // Verbindung beendet oder fehlgeschlagen: der Ton hat keine Quelle
    // mehr. Die Scheibe muss das wissen, sonst bliebe sie
    // "opportunistisch" fuer einen Erzeuger, der nicht mehr liefert --
    // harmlos fuer sich (opportunistisch heisst ja "wenn vorhanden"),
    // aber ein alter Wandler wuerde beim naechsten Verbinden mit
    // moeglicherweise anderer Rate weiterleben, statt sauber neu
    // aufgebaut zu werden (siehe removeSunSdrAudioSource).
    connect(m_sunSdrClient, &TciClient::stateChanged, this,
            [this](TciClient::State state, const QString& detail) {
        // m_shuttingDown guard, same pattern closeEvent() already uses
        // elsewhere in this class (e.g. the auto-open-ConnectionPanel
        // slot) — found missing here via a real, ASAN-confirmed
        // heap-use-after-free, 2026-08-30 (see
        // docs/architecture/2026-08-29-sunsdr-tci-teardown-segfault-investigation.md).
        // QObject destroys a parent's children in insertion order, not
        // a safe one: when MainWindow itself is torn down,
        // m_radioModel can already be destroyed (freed, not merely
        // nulled — the `if (m_radioModel ...)` check below does NOT
        // catch this) by the time m_sunSdrClient's own destructor runs
        // disconnectFromEndpoint() -> setState() -> this signal,
        // synchronously, still inside the same teardown cascade.
        if (m_shuttingDown) { return; }
        if (state == TciClient::State::Error) {
            qCWarning(lcTci) << "SunSDR:" << detail;
            // Derselbe Grund wie bei der Erfolgsmeldung oben: sonst gibt
            // es GAR KEIN Zeichen im Fenster, dass etwas schiefging.
            // TciClient bleibt in State::Error stehen, bis der Betreiber
            // erneut "Verbinden…" waehlt (siehe TciClient.cpp) -- diese
            // Meldung feuert darum je Fehlversuch genau einmal, nicht in
            // einer Schleife.
            showSunSdrNotice(
                this, QMessageBox::Warning, QStringLiteral("SunSDR"),
                QStringLiteral("Verbindung fehlgeschlagen: %1")
                    .arg(detail.isEmpty()
                             ? QStringLiteral("(keine weitere Angabe)")
                             : detail));
        }
        if (state == TciClient::State::Error
            || state == TciClient::State::Disconnected) {
            if (m_radioModel && m_sunSdrTargetSliceId >= 0) {
                if (AudioEngine* audio = m_radioModel->audioEngine()) {
                    audio->removeSunSdrAudioSource(m_sunSdrTargetSliceId);
                }
                // Spiegelbild zum Ton: der Panadapter darf nicht auf
                // einer Zuordnung stehen bleiben, die niemand mehr
                // speist -- sonst wuerde reassertSunSdrRouterMapping()
                // (unten) sie bei jedem echten VFO-Tick munter wieder
                // herstellen, obwohl TciClient laengst getrennt ist.
                if (auto* router = m_radioModel->fftRouter()) {
                    router->removeReceiver(kSunSdrPseudoStreamIndex);
                }
            }
        }
    });

    // ── Bild: Panadapter/Wasserfall aus dem I/Q-Strom ────────────────
    //
    // TciClient liefert echtes, verschraenktes Roh-I/Q (anders als
    // KiwiSDR, das fertige dBm-Bins schickt und darum SpectrumWidget::
    // updateKiwiSpectrumDbm umgeht, siehe dort). Fuer SunSDR ist der
    // reale FFTEngine-Weg darum richtig: dieselbe Infrastruktur, die
    // auch ein echter Empfaenger benutzt (Fenster, Mittelung, Zoom),
    // statt einen zweiten Pfad nachzubauen (Rechercheergebnis
    // 2026-08-24).
    if (m_radioModel) {
        if (FFTEngine* engine = createFftEngineForStream(kSunSdrPseudoStreamIndex)) {
            // Kontextobjekt ist der Engine, nicht MainWindow -- dasselbe
            // Muster, das createFftEngineForStream selbst fuer
            // rawIqDataForStream benutzt (siehe dort). TciClient laeuft
            // auf dem Hauptfaden, der Engine auf m_fftThread; weil beide
            // Faeden verschieden sind, loest Qt das automatisch als
            // Warteschlangen-Verbindung auf. Ein roher engine->feedIQ()-
            // Aufruf direkt aus einer TciClient-Lambda waere dagegen ein
            // Datenwettlauf: er liefe synchron auf dem Hauptfaden und
            // griffe parallel zum FFT-Faden auf denselben Ringpuffer zu.
            // receiver-Filter: dieselbe Rundruf-Ueberlegung wie beim Ton
            // (siehe audioFrameReady oben) -- ein anderer, gleichzeitig
            // verbundener TCI-Client, der Empfaenger 1s I/Q anfordert,
            // wuerde dessen Rahmen sonst mit in die SunSDR-FFTEngine
            // mischen.
            connect(m_sunSdrClient, &TciClient::iqFrameReady, engine,
                    [engine](int receiver, int /*sampleRate*/, int channels,
                            const std::vector<float>& interleaved) {
                if (receiver != kSunSdrReceiverIndex) { return; }
                if (channels != 2) { return; }
                const QVector<float> iq(interleaved.begin(), interleaved.end());
                engine->feedIQ(iq);
            });

            // Rate propagieren, aber nur bei einer echten Umstellung --
            // TCI erlaubt iq_samplerate jederzeit zu aendern (siehe
            // TciClient.h), doch das kommt selten vor, nicht auf jedem
            // Rahmen.
            connect(m_sunSdrClient, &TciClient::iqFrameReady, this,
                    [this, engine](int receiver, int sampleRate, int,
                                   const std::vector<float>&) {
                if (receiver != kSunSdrReceiverIndex) { return; }
                if (sampleRate <= 0 || sampleRate == m_sunSdrLastAppliedIqRateHz) {
                    return;
                }
                m_sunSdrLastAppliedIqRateHz = sampleRate;
                QMetaObject::invokeMethod(engine, [engine, sampleRate]() {
                    engine->setSampleRate(static_cast<double>(sampleRate));
                }, Qt::QueuedConnection);
                applySunSdrStreamWindow();
            });

            // Bench-gefunden, 2026-08-24: ExpertSDR2 hat ZWEI Empfaenger-
            // Plaetze (trx_count:2) und meldet fuer BEIDE eine eigene
            // dds:-Zeile -- "dds:0,..." in der Selbstauskunft immer vor
            // "dds:1,...". Ohne den receiver-Filter hier gewann Empfaenger
            // 1s Wert (z.B. eine zweite, unbewegte 80m-Scheibe) jedes Mal
            // gegen Empfaenger 0s echten Wert, obwohl Longpath ausschliesslich
            // Empfaenger kSunSdrReceiverIndex (0) als I/Q-Strom abonniert
            // hat (siehe startIqStream-Aufruf oben). Der Panadapter blieb
            // dadurch auf RX1s Frequenz haengen, ganz gleich wohin RX0
            // umgestimmt wurde.
            connect(m_sunSdrClient, &TciClient::ddcCenterChanged, this,
                    [this](int receiver, qint64 hz) {
                if (receiver != kSunSdrReceiverIndex) { return; }
                m_sunSdrDdcCenterHz = hz;
                applySunSdrStreamWindow();
            });

            // Selbstheilend: rebuildFftRouting() (ausgeloest von
            // streamBindingsChanged, das laut eigenem Kommentar "auf
            // jedem VFO-Tick" einer echten Scheibe feuert) loescht bei
            // jedem Aufruf die Router-Zuordnung fuer JEDEN Panadapter
            // und baut sie nur aus echten, gebundenen Scheiben neu auf
            // (streamIndex() >= 0) -- die SunSDR-Pseudoscheibe bleibt
            // dabei bewusst aussen vor (siehe kSunSdrPseudoStreamIndex)
            // und wuerde also bei jedem solchen Umbau stumm verschwinden.
            //
            // Diese Verbindung wird HIER angeschlossen, lange NACH der
            // eingebauten (die in buildUI() steht, beim Programmstart) --
            // Qt ruft Empfaenger desselben Signals in Anschlussreihenfolge
            // auf, meine Zuordnung wird also bei jedem Umbau IMMER danach
            // erneut gesetzt, nie davor ueberschrieben.
            connect(m_radioModel, &RadioModel::streamBindingsChanged,
                    this, [this](int, const QVector<int>&) {
                reassertSunSdrRouterMapping();
            });

            reassertSunSdrRouterMapping();
        }
    }

    // ── Steuerung: eingehend ─────────────────────────────────────────
    //
    // "vfo:R,V,hz" ist die tatsaechliche VFO-Anzeige, anders als "dds:"
    // oben (das ist die DDC-Mitte fuer den Panadapter, siehe TciClient.h)
    // -- vfo: gibt her, wo der Bediener in ExpertSDR2 tatsaechlich steht,
    // und ist damit der richtige Wert fuer die Scheibenfrequenz. Nur
    // VFO-Kanal 0 (A): dieselbe "einfach gehalten"-Entscheidung wie beim
    // Rest des SunSDR-Zweigs, kein Split-Betrieb ueber VFO B.
    //
    // Derselbe Empfaenger-Filter wie oben bei dds: (Bench-Fund
    // 2026-08-24: ExpertSDR2 hat zwei Empfaenger-Plaetze, jeder mit
    // eigenen vfo:/modulation:-Zeilen) -- ohne ihn wuerde Empfaenger 1s
    // Frequenz/Modus genauso in die Scheibe durchschlagen wie es beim
    // Panadapter geschah, bevor der Filter dort ergaenzt wurde.
    connect(m_sunSdrClient, &TciClient::vfoChanged, this,
            [this](int receiver, int channel, qint64 hz) {
        if (receiver != kSunSdrReceiverIndex || channel != 0) { return; }
        applyRemoteSunSdrFrequency(hz);
    });
    connect(m_sunSdrClient, &TciClient::modulationChanged, this,
            [this](int receiver, const QString& mode) {
        if (receiver != kSunSdrReceiverIndex) { return; }
        applyRemoteSunSdrModulation(mode);
    });
}

void MainWindow::applySunSdrStreamWindow()
{
    // sunSdrControllableSlice() statt sliceById() roh (Bench-Fund
    // 2026-08-24): eine Scheibe mit echter DDC-Bindung darf ihren
    // Panadapter nicht mehr vom SunSDR-Pseudostrom bekommen -- diese
    // Schranke fehlte hier komplett, obwohl sie fuer die Steuerung schon
    // stand.
    SliceModel* slice = sunSdrControllableSlice();
    if (!slice) { return; }
    const QString panId = panIdForSlice(slice);
    if (panId.isEmpty()) { return; }

    // dds: ist die wahre Mittenfrequenz (siehe TciClient.h) -- solange
    // sie noch nicht bekannt ist, die Scheibenfrequenz als Notloesung,
    // besser als 0 Hz oder der SliceModel-Konstruktor-Default.
    const double centreHz = m_sunSdrDdcCenterHz > 0
        ? static_cast<double>(m_sunSdrDdcCenterHz)
        : slice->frequency();

    m_streamWindows.insert(kSunSdrPseudoStreamIndex,
                           StreamWindow{centreHz, m_sunSdrLastAppliedIqRateHz});
    applyStreamWindowToPan(panId, kSunSdrPseudoStreamIndex);
}

void MainWindow::reassertSunSdrRouterMapping()
{
    SliceModel* slice = sunSdrControllableSlice();
    if (!slice) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }
    const QString panId = panIdForSlice(slice);
    if (panId.isEmpty()) { return; }
    // mapPanToReceiver de-dupliziert selbst (FFTRouter.cpp:17) -- ein
    // wiederholter Aufruf hier ist folgenlos, wenn die Zuordnung schon
    // steht.
    router->mapPanToReceiver(panId, kSunSdrPseudoStreamIndex);
}

} // namespace Longpath
