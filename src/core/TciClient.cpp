// SPDX-License-Identifier: GPL-3.0-or-later
//
// Siehe TciClient.h fuer Anlass und die zwei Messbefunde, denen diese
// Klasse absichtlich NICHT vertraut (Ratenfeld, Kanalfeld).
//
// =================================================================
// src/core/TciClient.cpp  (Longpath)
// =================================================================

#include "TciClient.h"
#include "LogCategories.h"

#include <QWebSocket>
#include <QUrl>

namespace Longpath {

namespace {

// Kanalzahl je Stromart — bewusst NICHT aus Kopf-Feld 28 (siehe
// TciBinaryFrame.h: dort unbrauchbar). Gemessen an ExpertSDR2 /
// SunSDR2 QRP, 2026-08-24 (docs/TCI-SunSDR-gemessen.md): I/Q und
// RX-Ton sind beide durchgaengig 2. Das deckt sich mit der
// TCI-Konvention: I/Q ist ein verschraenktes Paar, Audio ist Stereo.
// TX-Chrono traegt keine Nutzlast.
int assumedChannelsForStream(TciStreamType type)
{
    switch (type) {
    case TciStreamType::IqStream:
    case TciStreamType::RxAudioStream:
    case TciStreamType::TxAudioStream:
    case TciStreamType::LineoutStream:
        return 2;
    case TciStreamType::TxChrono:
        return 0;
    }
    return 2;
}

} // namespace

TciClient::TciClient(QObject* parent) : QObject(parent)
{
    // Vorsorglich, nicht weil hier heute eine Warteschlangen-Verbindung
    // besteht (es gibt keine — TciClient laeuft ausschliesslich auf dem
    // Hauptfaden, siehe Klassenkopf). Aber genau dieses Fehlen einer
    // Registrierung hat in diesem Projekt schon zweimal ein Signal still
    // verschluckt (AsrService.cpp, KiwiSdrManager.cpp) — beide Male erst
    // bemerkt, als spaeter eine Warteschlangen-Verbindung dazukam. Billige
    // Absicherung gegen einen bekannten, hier schon zweimal getretenen
    // Stolperstein, bevor er ein drittes Mal noetig wird.
    qRegisterMetaType<std::vector<float>>("std::vector<float>");
    qRegisterMetaType<Longpath::TciClient::State>("Longpath::TciClient::State");
}

TciClient::~TciClient()
{
    disconnectFromEndpoint();
}

void TciClient::resetSessionState()
{
    m_lastError.clear();
    m_deviceName.clear();
    m_iqRateFromText    = 0;
    m_audioRateFromText = 0;
    m_trxRunning  = false;
    m_sawRunState = false;
    m_sawReady    = false;
    m_ddcCenterHz.clear();
    m_vfoHz.clear();
    m_modulation.clear();
}

void TciClient::setState(State state, const QString& detail)
{
    if (m_state == state && detail.isEmpty()) { return; }
    m_state = state;
    emit stateChanged(state, detail);
}

void TciClient::connectToEndpoint(const QString& host, quint16 port)
{
    // disconnectFromEndpoint() setzt die Sitzung inzwischen selbst zurueck
    // (siehe dort) -- ein zweiter Aufruf hier waere nur Verdopplung.
    disconnectFromEndpoint();

    const QString trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty()) {
        setState(State::Error, tr("Keine Adresse angegeben."));
        return;
    }

    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_socket, &QWebSocket::connected, this, [this]() {
        // Verbunden, aber noch nicht beschrieben: ExpertSDR2 schickt
        // seine Selbstauskunft erst NACH dem Verbindungsaufbau, mit
        // "ready;" als Abschluss. Vor diesem Punkt sind die Raten aus
        // dem Textkanal noch nicht bekannt — Stroeme anzufordern waere
        // verfrueht (siehe startIqStream/startAudioStream).
        setState(State::Connecting, tr("verbunden, warte auf Selbstauskunft"));
    });
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &TciClient::handleTextMessage);
    connect(m_socket, &QWebSocket::binaryMessageReceived,
            this, &TciClient::handleBinaryMessage);
    connect(m_socket, &QWebSocket::disconnected, this, [this]() {
        // Ein Fehler hat schon seinen eigenen Zustand gesetzt (mit der
        // eigentlichen Fehlermeldung) — das nachfolgende disconnected()
        // darf den nicht mit einer nichtssagenden Zeile ueberschreiben.
        if (m_state != State::Error) {
            setState(State::Disconnected, tr("Verbindung beendet"));
        }
    });
    connect(m_socket, &QWebSocket::errorOccurred,
            this, &TciClient::handleSocketError);

    setState(State::Connecting, tr("verbinde ..."));
    const QUrl url(QStringLiteral("ws://%1:%2").arg(trimmedHost).arg(port));
    m_socket->open(url);
}

void TciClient::disconnectFromEndpoint()
{
    if (!m_socket) { return; }
    // Erst die Verbindungen zu diesem Objekt kappen: das bevorstehende
    // close() loest sonst disconnected() aus und wuerde den absichtlich
    // herbeigefuehrten Abbau als "Verbindung beendet" ueberschreiben.
    // Genau das Muster aus KiwiSdrClient::cleanupSockets().
    m_socket->disconnect(this);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
    }
    m_socket->deleteLater();
    m_socket = nullptr;

    // Gegengepruefter Befund (2026-08-24 Grundgeruest-Durchsicht): ohne
    // dies melden deviceName()/iqSampleRate()/isReceiverRunning() nach
    // einem Abbau weiter die Werte der toten Sitzung -- ununterscheidbar
    // von einer echten. Ein Abbau OHNE anschliessenden neuen Aufbau
    // (der ueber diese Methode laeuft, siehe connectToEndpoint()) blieb
    // bisher die einzige Luecke. Gleiches Muster wie KiwiSdrClient::
    // disconnectFromEndpoint() -> cleanupSockets(), das bei jedem Abbau
    // zuruecksetzt, nicht erst vor dem naechsten Aufbau.
    resetSessionState();

    if (m_state != State::Disconnected) {
        setState(State::Disconnected, QString());
    }
}

void TciClient::handleSocketError(QAbstractSocket::SocketError)
{
    // sender() ist der Sockel, der den Fehler tatsaechlich ausgeloest hat --
    // gemerkt, BEVOR errorOccurred() emittiert wird.
    QWebSocket* origin = qobject_cast<QWebSocket*>(sender());
    m_lastError = origin ? origin->errorString() : tr("unbekannter Fehler");
    qCWarning(lcTci) << "TciClient: Verbindungsfehler:" << m_lastError;
    emit errorOccurred(m_lastError);

    // Gegengepruefter Befund (2026-08-24 Grundgeruest-Durchsicht): ein
    // Beobachter von errorOccurred() koennte darauf SOFORT (gleicher
    // Thread, direkte Verbindung) mit connectToEndpoint() reagieren --
    // ein natuerliches automatisches Wiederverbinden. Das legt einen
    // neuen Sockel an und m_socket zeigt danach auf DEN, nicht mehr auf
    // den hier fehlgeschlagenen. Ohne diese Pruefung wuerde die Zeile
    // unten den frisch gesetzten Connecting-Zustand des neuen Versuchs
    // mit Error ueberschreiben -- ein Fehler fuer eine Verbindung, die
    // gar nicht mehr existiert. origin != m_socket heisst: waehrend des
    // obigen emit hat sich die Sitzung schon weiterbewegt, dieser Fehler
    // ist ueberholt.
    if (origin != m_socket) { return; }
    setState(State::Error, m_lastError);
}

void TciClient::sendCommand(const QString& command)
{
    if (!m_socket || m_state == State::Disconnected || m_state == State::Error) {
        qCWarning(lcTci) << "TciClient: Befehl verworfen, keine Verbindung:"
                          << command;
        return;
    }
    QString cmd = command.trimmed();
    if (!cmd.endsWith(QLatin1Char(';'))) { cmd += QLatin1Char(';'); }
    m_socket->sendTextMessage(cmd);
}

void TciClient::startIqStream(int receiver)
{
    if (m_state != State::Connected) {
        qCWarning(lcTci) << "TciClient: iq_start ignoriert, Selbstauskunft "
                             "noch nicht zu Ende (Empfaenger" << receiver << ")";
        return;
    }
    sendCommand(QStringLiteral("iq_start:%1").arg(receiver));
}

void TciClient::stopIqStream(int receiver)
{
    sendCommand(QStringLiteral("iq_stop:%1").arg(receiver));
}

void TciClient::startAudioStream(int receiver)
{
    if (m_state != State::Connected) {
        qCWarning(lcTci) << "TciClient: audio_start ignoriert, Selbstauskunft "
                             "noch nicht zu Ende (Empfaenger" << receiver << ")";
        return;
    }
    sendCommand(QStringLiteral("audio_start:%1").arg(receiver));
}

void TciClient::stopAudioStream(int receiver)
{
    sendCommand(QStringLiteral("audio_stop:%1").arg(receiver));
}

void TciClient::setVfoFrequency(int receiver, int channel, qint64 hz)
{
    if (m_state != State::Connected) {
        qCWarning(lcTci) << "TciClient: vfo ignoriert, Selbstauskunft "
                             "noch nicht zu Ende (Empfaenger" << receiver
                          << ", Kanal" << channel << ")";
        return;
    }
    sendCommand(QStringLiteral("vfo:%1,%2,%3").arg(receiver).arg(channel).arg(hz));
}

void TciClient::setModulation(int receiver, const QString& mode)
{
    if (m_state != State::Connected) {
        qCWarning(lcTci) << "TciClient: modulation ignoriert, Selbstauskunft "
                             "noch nicht zu Ende (Empfaenger" << receiver << ")";
        return;
    }
    sendCommand(QStringLiteral("modulation:%1,%2").arg(receiver).arg(mode));
}

void TciClient::handleTextMessage(const QString& message)
{
    for (const QString& part : message.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        handleTextLine(part.trimmed());
    }
}

void TciClient::handleTextLine(const QString& line)
{
    if (line.isEmpty()) { return; }

    if (line.startsWith(QStringLiteral("device:"))) {
        m_deviceName = line.section(QLatin1Char(':'), 1).trimmed();
        return;
    }

    // Die WAHREN Raten. Siehe TciClient.h: das Binaerkopf-Ratenfeld
    // folgt einer Umstellung nicht, diese Zeilen sind die einzige
    // verlaessliche Quelle.
    if (line.startsWith(QStringLiteral("iq_samplerate:"))) {
        bool ok = false;
        const int rate = line.section(QLatin1Char(':'), 1).trimmed().toInt(&ok);
        if (ok && rate > 0) { m_iqRateFromText = rate; }
        return;
    }
    if (line.startsWith(QStringLiteral("audio_samplerate:"))) {
        bool ok = false;
        const int rate = line.section(QLatin1Char(':'), 1).trimmed().toInt(&ok);
        if (ok && rate > 0) { m_audioRateFromText = rate; }
        return;
    }

    // Die wahre Mittenfrequenz des I/Q-Stroms. Siehe TciClient.h,
    // ddcCenterHz(). Gemessen gegen ExpertSDR2 (2026-08-24):
    // "vfo:R,V,hz" sind die beiden VFO-Anzeigen INNERHALB der
    // ZF-Durchlassbreite, "if:R,V,offsetHz" ist ihr Versatz dazu
    // (nachgerechnet und bestaetigt: dds 1910670 + if 9830 = vfo
    // 1920500). "dds:" allein ist die Mitte, um die der I/Q-Strom
    // selbst liegt -- fuer den Panadapter (Schritt "Bild") ist nur
    // diese eine Zahl noetig. "if:" bleibt ungelesen (aus dds:+vfo:
    // ableitbar, kein eigener Bedarf); "vfo:" wird seit Schritt
    // "Steuerung" unten separat gelesen, siehe dort.
    if (line.startsWith(QStringLiteral("dds:"))) {
        const QStringList parts =
            line.section(QLatin1Char(':'), 1).split(QLatin1Char(','));
        if (parts.size() == 2) {
            bool okReceiver = false;
            bool okHz = false;
            const int receiver = parts.at(0).trimmed().toInt(&okReceiver);
            const qint64 hz = parts.at(1).trimmed().toLongLong(&okHz);
            if (okReceiver && okHz) {
                m_ddcCenterHz[receiver] = hz;
                emit ddcCenterChanged(receiver, hz);
            }
        }
        return;
    }

    // Schritt "Steuerung" (2026-08-24): die tatsaechliche VFO-Anzeige --
    // wo der Bediener innerhalb der ZF-Durchlassbreite steht, siehe
    // vfoHz() in TciClient.h. Drei Felder, anders als dds: (zwei):
    // Empfaenger, VFO-Kanal (0=A, 1=B), Hz.
    if (line.startsWith(QStringLiteral("vfo:"))) {
        const QStringList parts =
            line.section(QLatin1Char(':'), 1).split(QLatin1Char(','));
        if (parts.size() == 3) {
            bool okReceiver = false;
            bool okChannel = false;
            bool okHz = false;
            const int receiver = parts.at(0).trimmed().toInt(&okReceiver);
            const int channel = parts.at(1).trimmed().toInt(&okChannel);
            const qint64 hz = parts.at(2).trimmed().toLongLong(&okHz);
            if (okReceiver && okChannel && okHz) {
                m_vfoHz[receiver][channel] = hz;
                emit vfoChanged(receiver, channel, hz);
            }
        }
        return;
    }

    // Betriebsart, klein geschrieben wie am Draht gemessen
    // ("modulation:0,lsb"), siehe modulation() in TciClient.h.
    if (line.startsWith(QStringLiteral("modulation:"))) {
        const QStringList parts =
            line.section(QLatin1Char(':'), 1).split(QLatin1Char(','));
        if (parts.size() == 2) {
            bool okReceiver = false;
            const int receiver = parts.at(0).trimmed().toInt(&okReceiver);
            const QString mode = parts.at(1).trimmed().toLower();
            if (okReceiver && !mode.isEmpty()) {
                m_modulation[receiver] = mode;
                emit modulationChanged(receiver, mode);
            }
        }
        return;
    }

    // Laufzustand: TCI meldet ihn als nackte Zeile "start"/"stop", ohne
    // Geraeteindex (siehe TciClient.h).
    if (line.compare(QStringLiteral("start"), Qt::CaseInsensitive) == 0) {
        m_trxRunning  = true;
        m_sawRunState = true;
        emit receiverRunStateChanged(true);
        return;
    }
    if (line.compare(QStringLiteral("stop"), Qt::CaseInsensitive) == 0) {
        m_trxRunning  = false;
        m_sawRunState = true;
        emit receiverRunStateChanged(false);
        return;
    }

    if (line.compare(QStringLiteral("ready"), Qt::CaseInsensitive) == 0) {
        m_sawReady = true;
        // Schnappschuss VOR jedem Emit (gegengepruefter Befund,
        // 2026-08-24 Grundgeruest-Durchsicht): setState(Connected, ...)
        // loest stateChanged() aus. Reagiert ein Beobachter darauf sofort
        // mit connectToEndpoint() (z.B. eine Namens-Pruefliste, die bei
        // einem unerwarteten Geraet anderswo neu verbindet), raeumt das
        // synchron die Sitzung ab -- inklusive m_deviceName -- bevor die
        // naechste Zeile unten laeuft. Die lokale Kopie ist dagegen immun.
        const QString describedDevice = m_deviceName;
        qCInfo(lcTci) << "TciClient: Selbstauskunft zu Ende, Geraet:"
                       << (describedDevice.isEmpty() ? QStringLiteral("(unbenannt)")
                                                      : describedDevice)
                       << "IQ-Rate:" << m_iqRateFromText
                       << "Ton-Rate:" << m_audioRateFromText;
        setState(State::Connected, describedDevice);
        emit deviceDescribed(describedDevice);
        return;
    }

    // Alles andere (Frequenz, Betriebsart, Pegel, Filter, ...) ist fuer
    // diese erste Stufe noch nicht gebraucht — die Steuerung baut
    // darauf in einem eigenen Schritt auf.
}

void TciClient::handleBinaryMessage(const QByteArray& frame)
{
    const TciStreamHeader h = TciBinaryFrame::parseStreamHeader(frame);
    if (!h.valid) {
        // Zu kurz fuer einen Kopf. Kommt bei TCI nicht vor, ausser bei
        // einer gestoerten Verbindung — still verwerfen waere hier
        // falsch, aber auch kein Fehler im engeren Sinn melden: dies
        // ist Netzrauschen, keine Protokollverletzung des Geraets.
        return;
    }
    if (!TciBinaryFrame::headerMatchesPayload(h)) {
        // Wertzahl und Nutzlast passen nicht zusammen: entweder ist der
        // Rahmen anders aufgebaut als angenommen, oder er ist
        // abgeschnitten. Entpacken waere geraten — verwerfen und
        // melden, nicht still weitermachen mit falschen Werten.
        qCWarning(lcTci) << "TciClient: Rahmen verworfen, Kopf und Nutzlast "
                             "passen nicht zusammen (Stromart" << h.streamType
                          << ", " << h.frameBytes << "Byte,"
                          << h.length << "Werte erwartet)";
        return;
    }

    const auto streamType = static_cast<TciStreamType>(h.streamType);
    const int channels = assumedChannelsForStream(streamType);
    if (channels > 0 && h.length % channels != 0) {
        qCWarning(lcTci) << "TciClient: Rahmen verworfen," << h.length
                          << "Werte sind nicht durch" << channels
                          << "Kanaele teilbar (Stromart" << h.streamType << ")";
        return;
    }

    switch (streamType) {
    case TciStreamType::IqStream: {
        const int rate = m_iqRateFromText > 0 ? m_iqRateFromText : h.sampleRate;
        if (m_iqRateFromText <= 0) {
            // Sollte nach Connected nicht vorkommen: die Selbstauskunft
            // nennt iq_samplerate immer vor "ready" (gemessen,
            // docs/TCI-SunSDR-gemessen.md). Wenn doch, ist der
            // Kopfwert die einzige Notloesung — und der luegt bei
            // jeder Umstellung. Laut melden, nicht still hinnehmen.
            qCWarning(lcTci) << "TciClient: IQ-Rate unbekannt, verwende "
                                 "unsicheren Kopfwert" << rate;
        }
        const std::vector<float> samples =
            TciBinaryFrame::decodeSamples(frame, 64, h.length, h.sampleType);
        emit iqFrameReady(h.receiver, rate, channels, samples);
        break;
    }
    case TciStreamType::RxAudioStream: {
        const int rate = m_audioRateFromText > 0 ? m_audioRateFromText : h.sampleRate;
        if (m_audioRateFromText <= 0) {
            qCWarning(lcTci) << "TciClient: Ton-Rate unbekannt, verwende "
                                 "unsicheren Kopfwert" << rate;
        }
        const std::vector<float> samples =
            TciBinaryFrame::decodeSamples(frame, 64, h.length, h.sampleType);
        emit audioFrameReady(h.receiver, rate, channels, samples);
        break;
    }
    default:
        // TX-Ton, TX-Takt, Lineout: fuer den Empfangs-Aufsatz (noch)
        // nicht gebraucht. Bewusst verworfen, nicht als Fehler gemeldet.
        break;
    }
}

} // namespace Longpath
