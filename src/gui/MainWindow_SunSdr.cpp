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
#include "core/LogCategories.h"
#include "core/TciClient.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QMessageBox>

namespace Longpath {

namespace {

constexpr quint16 kDefaultSunSdrPort = 40001;
// TCI-seitig ist der Empfaenger immer 0 -- ein SunSDR2 QRP hat genau
// einen. Kein Feld dafuer in den Einstellungen: es gibt hier nichts zu
// waehlen.
constexpr int kSunSdrReceiverIndex = 0;

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
    SliceModel* slice = m_radioModel->activeSlice();
    if (!slice) {
        const QList<SliceModel*> slices = m_radioModel->slices();
        if (!slices.isEmpty()) {
            slice = slices.first();
        } else {
            const int id = m_radioModel->addSlice();
            slice = m_radioModel->sliceById(id);
            qCInfo(lcTci) << "SunSDR: keine Scheibe vorhanden -- eine "
                             "angelegt, Kennung" << id;
        }
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
    m_sunSdrTargetSliceId = slice->sliceIndex();

    if (!m_sunSdrClient) {
        m_sunSdrClient = new TciClient(this);
        wireSunSdr();
    }

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

    // Die Selbstauskunft ist zu Ende: jetzt den Ton anfordern und die
    // Scheibe fuer SunSDR-Ton freischalten. TciClient::startAudioStream
    // sendet den Befehl unabhaengig davon, ob der Empfaenger in
    // ExpertSDR2 gerade laeuft (docs/TCI-SunSDR-gemessen.md) -- solange
    // er steht, kommt einfach kein Rahmen, und feedSunSdrAudioData wird
    // schlicht nicht aufgerufen. Kein Sonderfall noetig.
    connect(m_sunSdrClient, &TciClient::deviceDescribed, this,
            [this](const QString& deviceName) {
        qCInfo(lcTci) << "SunSDR beschrieben:" << deviceName;
        if (!m_sunSdrClient) { return; }
        m_sunSdrClient->startAudioStream(kSunSdrReceiverIndex);
        if (m_radioModel && m_sunSdrTargetSliceId >= 0) {
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
            [this](int /*receiver*/, int sampleRate, int channels,
                   const std::vector<float>& interleaved) {
        if (!m_radioModel || m_sunSdrTargetSliceId < 0) { return; }
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
            }
        }
    });
}

} // namespace Longpath
