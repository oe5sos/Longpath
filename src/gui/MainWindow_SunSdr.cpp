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
            connect(m_sunSdrClient, &TciClient::iqFrameReady, engine,
                    [engine](int /*receiver*/, int /*sampleRate*/, int channels,
                            const std::vector<float>& interleaved) {
                if (channels != 2) { return; }
                const QVector<float> iq(interleaved.begin(), interleaved.end());
                engine->feedIQ(iq);
            });

            // Rate propagieren, aber nur bei einer echten Umstellung --
            // TCI erlaubt iq_samplerate jederzeit zu aendern (siehe
            // TciClient.h), doch das kommt selten vor, nicht auf jedem
            // Rahmen.
            connect(m_sunSdrClient, &TciClient::iqFrameReady, this,
                    [this, engine](int, int sampleRate, int,
                                   const std::vector<float>&) {
                if (sampleRate <= 0 || sampleRate == m_sunSdrLastAppliedIqRateHz) {
                    return;
                }
                m_sunSdrLastAppliedIqRateHz = sampleRate;
                QMetaObject::invokeMethod(engine, [engine, sampleRate]() {
                    engine->setSampleRate(static_cast<double>(sampleRate));
                }, Qt::QueuedConnection);
                applySunSdrStreamWindow();
            });

            connect(m_sunSdrClient, &TciClient::ddcCenterChanged, this,
                    [this](int /*receiver*/, qint64 hz) {
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
}

void MainWindow::applySunSdrStreamWindow()
{
    if (m_sunSdrTargetSliceId < 0 || !m_radioModel) { return; }
    SliceModel* slice = m_radioModel->sliceById(m_sunSdrTargetSliceId);
    const QString panId = panIdForSlice(slice);
    if (panId.isEmpty()) { return; }

    // dds: ist die wahre Mittenfrequenz (siehe TciClient.h) -- solange
    // sie noch nicht bekannt ist, die Scheibenfrequenz als Notloesung,
    // besser als 0 Hz oder der SliceModel-Konstruktor-Default.
    const double centreHz = m_sunSdrDdcCenterHz > 0
        ? static_cast<double>(m_sunSdrDdcCenterHz)
        : (slice ? slice->frequency() : 0.0);

    m_streamWindows.insert(kSunSdrPseudoStreamIndex,
                           StreamWindow{centreHz, m_sunSdrLastAppliedIqRateHz});
    applyStreamWindowToPan(panId, kSunSdrPseudoStreamIndex);
}

void MainWindow::reassertSunSdrRouterMapping()
{
    if (m_sunSdrTargetSliceId < 0 || !m_radioModel) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }
    SliceModel* slice = m_radioModel->sliceById(m_sunSdrTargetSliceId);
    const QString panId = panIdForSlice(slice);
    if (panId.isEmpty()) { return; }
    // mapPanToReceiver de-dupliziert selbst (FFTRouter.cpp:17) -- ein
    // wiederholter Aufruf hier ist folgenlos, wenn die Zuordnung schon
    // steht.
    router->mapPanToReceiver(panId, kSunSdrPseudoStreamIndex);
}

} // namespace Longpath
