// =================================================================
// src/core/RigctldClient.cpp  (Longpath)
// =================================================================
//
// Longpath-original — siehe RigctldClient.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "RigctldClient.h"

#include <QLoggingCategory>
#include <QTimer>

namespace Longpath {

namespace {

Q_LOGGING_CATEGORY(lcRigctld, "longpath.rigctld.client")

// Wieviele Takte zwischen den selteneren Abfragen. Die Frequenz wird
// jeden Takt geholt (sie aendert sich unter der Hand des Betreibers),
// PTT jeden zweiten, der Modus jeden achten -- den dreht niemand im
// Viertelsekundentakt.
constexpr quint64 kPttEveryNthTick  = 2;
constexpr quint64 kModeEveryNthTick = 8;

// Eine Zeile ist eine RPRT-Zeile, wenn sie so anfaengt. Das ist die
// Antwort auf einen Befehl -- und bei einem Geraet, das eine Abfrage
// nicht beherrscht, auch die Antwort auf eine Abfrage.
bool istReport(const QByteArray& zeile)
{
    return zeile.trimmed().startsWith(QByteArrayLiteral("RPRT"));
}

} // namespace

RigctldClient::RigctldClient(QObject* parent) : QObject(parent)
{
    m_poll = new QTimer(this);
    m_poll->setInterval(m_pollMs);
    connect(m_poll, &QTimer::timeout, this, &RigctldClient::poll);

    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_retry->setInterval(kRetryAfterDropMs);
    connect(m_retry, &QTimer::timeout, this, [this]() {
        if (m_wanted && m_state == State::Disconnected) { connectToRig(); }
    });

    // Antwortwache. Ohne sie bleibt m_awaiting bei einem rigctld, das
    // einfriert, ohne die Verbindung zu schliessen, fuer immer gesetzt:
    // die „ein Befehl auf einmal"-Sperre im Takt kommt nie wieder
    // durch, und der Client ist stumm tot, bis jemand von Hand neu
    // verbindet. Die Verbindung zu kappen schickt die Erholung ueber
    // den Trennungsweg, der den Wiederanlauf schon kennt.
    m_nachlesen = new QTimer(this);
    m_nachlesen->setSingleShot(true);
    m_nachlesen->setInterval(50);
    connect(m_nachlesen, &QTimer::timeout, this, [this]() {
        if (!isConnected()) { return; }
        for (const QByteArray& frage : std::as_const(m_rohListe)) {
            send(QByteArrayLiteral("w ") + frage + '\n', Pending::Roh);
        }
    });

    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    m_deadline->setInterval(kReplyTimeoutMs);
    connect(m_deadline, &QTimer::timeout, this, [this]() {
        if (m_awaiting == Pending::None) { return; }
        emit errorOccurred(
            QStringLiteral("The rig stopped answering — reconnecting"));
        // abort() schickt kein disconnected(); darum hier abbauen und
        // den Wiederanlauf die Verbindung zurueckholen lassen.
        m_socket.abort();
        m_poll->stop();
        m_queue.clear();
        m_buffer.clear();
        m_awaiting = Pending::None;
        setState(State::Disconnected);
        emit replyTimedOut();
        if (m_wanted) { m_retry->start(kRetryAfterDropMs); }
    });

    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        m_buffer.clear();
        m_queue.clear();
        m_awaiting = Pending::None;
        m_tick = 0;
        setState(State::Connected);
        m_poll->start();
        // Nicht auf den ersten Takt warten: der Betreiber hat gerade
        // auf Verbinden gedrueckt und will eine Zahl sehen.
        send(QByteArrayLiteral("f\n"), Pending::Frequency);
    });

    connect(&m_socket, &QTcpSocket::readyRead,
            this, &RigctldClient::onReadyRead);

    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_poll->stop();
        m_deadline->stop();
        m_awaiting = Pending::None;
        setState(State::Disconnected);
        if (m_wanted) { m_retry->start(kRetryAfterDropMs); }
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        fail(m_socket.errorString());
        // Ein abgewiesener Verbindungsversuch schickt kein
        // disconnected() -- Qt meldet das nur fuer eine Verbindung, die
        // stand. Ohne diesen Zweig bliebe der Client in Connecting
        // stehen, mit „Connection refused" auf dem Schirm und nichts
        // weiter. Derselbe Fall wie beim Rotor am 2026-09-16: die
        // Ablehnung ist der normale erste Ausgang, weil das rigctld,
        // das dieses Programm gerade gestartet hat, noch nicht lauscht.
        if (m_state == State::Connecting) {
            m_socket.abort();
            setState(State::Disconnected);
            if (m_wanted) { m_retry->start(kRetryAfterRefusedMs); }
        }
    });
}

RigctldClient::~RigctldClient() = default;

void RigctldClient::setTarget(const QString& host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
}

void RigctldClient::setPollIntervalMs(int ms)
{
    m_pollMs = qBound(20, ms, 5000);
    m_poll->setInterval(m_pollMs);
}

bool RigctldClient::hasFreshFrequency() const
{
    if (!m_lastFrequency.isValid()) { return false; }
    return m_lastFrequency.msecsTo(QDateTime::currentDateTimeUtc())
           < kStaleAfterMs;
}

QString RigctldClient::description() const
{
    if (m_host.isEmpty()) { return QStringLiteral("no rig set"); }
    return QStringLiteral("rigctld %1:%2").arg(m_host).arg(m_port);
}

void RigctldClient::connectToRig()
{
    if (m_host.isEmpty()) {
        emit errorOccurred(QStringLiteral("No rig address set"));
        return;
    }
    m_wanted = true;
    if (m_socket.state() != QAbstractSocket::UnconnectedState) { return; }

    m_retry->stop();
    setState(State::Connecting);
    m_socket.connectToHost(m_host, m_port);
}

void RigctldClient::disconnectFromRig()
{
    m_wanted = false;
    m_retry->stop();
    m_poll->stop();
    m_deadline->stop();
    m_nachlesen->stop();
    m_queue.clear();
    m_buffer.clear();
    m_awaiting = Pending::None;
    m_socket.abort();
    setState(State::Disconnected);
}

void RigctldClient::setFrequencyHz(qint64 hz)
{
    if (!isConnected() || hz <= 0) { return; }
    send(setFrequencyCommand(hz), Pending::Report);
}

QByteArray RigctldClient::setModeCommand(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m.isEmpty()) { return {}; }
    for (const QChar c : m) {
        if (!c.isLetterOrNumber()) { return {}; }
    }
    return QStringLiteral("M %1 0\n").arg(m).toLatin1();
}

void RigctldClient::setRohAbfragen(const QList<QByteArray>& liste,
                                   const QByteArray& haeufig,
                                   const QByteArray& beimSenden)
{
    m_rohListe      = liste;
    m_rohHaeufig    = haeufig;
    m_rohBeimSenden = beimSenden;
    m_rohIndex      = 0;
    m_geraderTakt   = 0;
}

void RigctldClient::schickeZeile(const QByteArray& zeile)
{
    if (!isConnected() || zeile.isEmpty()) { return; }
    // `W` meldet kein RPRT: es kommt nur ein Nullbyte zurueck.
    send(zeile, zeile.startsWith("W ") ? Pending::Nullbyte : Pending::Report);
}

void RigctldClient::frageRohListeNach()
{
    if (isConnected()) { m_nachlesen->start(); }
}

void RigctldClient::schalteZwischenspeicherAus()
{
    if (!isConnected()) { return; }
    send(QByteArrayLiteral("\\set_cache 0\n"), Pending::Report);
}

void RigctldClient::setMode(const QString& mode)
{
    if (!isConnected()) { return; }
    const QByteArray befehl = setModeCommand(mode);
    if (befehl.isEmpty()) { return; }
    send(befehl, Pending::Report);
}

void RigctldClient::poll()
{
    // Ein ausstehender Befehl auf einmal. Steht die letzte Antwort noch
    // aus, wird nichts nachgelegt -- rigctld antwortet in der Reihenfolge
    // der Befehle, und eine Schlange alter Abfragen verzoegert nur den
    // naechsten Abstimmbefehl dahinter.
    if (m_awaiting != Pending::None || !m_queue.isEmpty()) { return; }

    ++m_tick;

    // Mit Rohabfragen: die Frequenz behaelt jeden zweiten Takt, PTT jeden
    // vierten, und dazwischen wechseln sich die haeufige Abfrage und die
    // Liste ab.
    if (!m_rohListe.isEmpty() && m_tick % 2 == 0) {
        const int schritt = m_geraderTakt++ % 4;
        if (schritt == 0) {
            send(QByteArrayLiteral("t\n"), Pending::Ptt);
        } else if (schritt == 1 || schritt == 3) {
            if (schritt == 3 && !m_ptt) {
                // Die Betriebsart jedes dritte Mal: sie soll dem Knopf am
                // Geraet folgen und nicht auf die ganze Liste warten.
                if (++m_rohRunde % 3 == 0) {
                    send(QByteArrayLiteral("m\n"), Pending::Mode);
                    return;
                }
                if (m_rohIndex >= m_rohListe.size()) { m_rohIndex = 0; }
                send(QByteArrayLiteral("w ") + m_rohListe.at(m_rohIndex++) + '\n',
                     Pending::Roh);
            } else if (schritt == 3) {
                send(QByteArrayLiteral("w ") + m_rohBeimSenden + '\n', Pending::Roh);
            } else {
                send(QByteArrayLiteral("w ") + m_rohHaeufig + '\n', Pending::Roh);
            }
        } else {
            send(QByteArrayLiteral("f\n"), Pending::Frequency);
        }
        return;
    }

    if (m_tick % kModeEveryNthTick == 0) {
        send(QByteArrayLiteral("m\n"), Pending::Mode);
        return;
    }
    if (m_tick % kPttEveryNthTick == 0) {
        send(QByteArrayLiteral("t\n"), Pending::Ptt);
        return;
    }
    send(QByteArrayLiteral("f\n"), Pending::Frequency);
}

void RigctldClient::send(const QByteArray& command, Pending expect)
{
    m_queue.enqueue(Command{command, expect});
    pump();
}

void RigctldClient::pump()
{
    if (m_awaiting != Pending::None) { return; }
    if (m_queue.isEmpty()) { return; }
    if (m_socket.state() != QAbstractSocket::ConnectedState) { return; }

    const Command c = m_queue.dequeue();
    m_awaiting = c.expect;
    m_socket.write(c.bytes);
    m_deadline->start(kReplyTimeoutMs);
}

int RigctldClient::expectedLines(Pending p)
{
    // Nur die Modusantwort ist zweizeilig (Modus, dann Durchlass).
    return p == Pending::Mode ? 2 : 1;
}

void RigctldClient::onReadyRead()
{
    m_buffer += m_socket.readAll();

    while (m_awaiting != Pending::None) {
        if (m_awaiting == Pending::Nullbyte) {
            const qsizetype null = m_buffer.indexOf('\0');
            const qsizetype zeile = m_buffer.indexOf('\n');
            if (null < 0 && zeile < 0) { return; }
            const qsizetype ende = (null >= 0 && (zeile < 0 || null < zeile)) ? null : zeile;
            m_buffer.remove(0, ende + 1);
            m_awaiting = Pending::None;
            m_deadline->stop();
            pump();
            continue;
        }

        // Eine Rohantwort endet nicht mit einem Zeilenumbruch: rigctld gibt
        // weiter, was das Geraet schickt, "PA10;" und eine Null dahinter.
        if (m_awaiting == Pending::Roh) {
            const qsizetype ende = m_buffer.indexOf(';');
            const qsizetype zeile = m_buffer.indexOf('\n');
            if (ende < 0 && zeile < 0) { return; }
            qsizetype bis = (ende >= 0 && (zeile < 0 || ende < zeile)) ? ende + 1 : zeile + 1;
            while (bis < m_buffer.size()
                   && (m_buffer.at(bis) == '\0' || m_buffer.at(bis) == '\n')) {
                ++bis;
            }
            const QByteArray antwort = m_buffer.left(bis);
            m_buffer.remove(0, bis);
            m_awaiting = Pending::None;
            m_deadline->stop();
            if (ende >= 0) { emit rohAntwort(antwort); }
            pump();
            continue;
        }

        const int noetig = expectedLines(m_awaiting);

        // Eine RPRT-Zeile beendet das Warten SOFORT, auch wenn zwei
        // Zeilen erwartet waren. Das ist der Fall, an dem die Vorlage
        // im Contestprogramm haengen bliebe: ein Geraet ohne
        // Modusabfrage antwortet einzeilig mit „RPRT -11", und wer
        // unbedingt auf den zweiten Umbruch wartet, wartet bis zur
        // Antwortwache -- vier Sekunden fuer jede Modusabfrage.
        int bisZeile = 0;
        int gefunden = 0;
        bool reportZuerst = false;
        for (int i = 0; i < m_buffer.size(); ++i) {
            if (m_buffer.at(i) != '\n') { continue; }
            ++gefunden;
            if (gefunden == 1 && istReport(m_buffer.left(i))) {
                reportZuerst = true;
            }
            bisZeile = i + 1;
            if (reportZuerst || gefunden >= noetig) { break; }
        }
        if (!reportZuerst && gefunden < noetig) { return; }

        const QByteArray antwort = m_buffer.left(bisZeile);
        m_buffer.remove(0, bisZeile);

        const Pending war = m_awaiting;
        m_awaiting = Pending::None;
        m_deadline->stop();

        switch (war) {
        case Pending::Frequency: {
            qint64 hz = 0;
            if (parseFrequency(antwort, hz)) {
                m_lastFrequency = QDateTime::currentDateTimeUtc();
                if (hz != m_frequencyHz || m_meldeFrequenz) {
                    m_meldeFrequenz = false;
                    m_frequencyHz = hz;
                    emit frequencyChanged(hz);
                }
            } else {
                int code = 0;
                if (parseReport(antwort, code) && code != 0) {
                    emit errorOccurred(
                        QStringLiteral("Reading the frequency failed: %1")
                            .arg(describeReport(code)));
                }
            }
            break;
        }
        case Pending::Mode: {
            QString modus;
            int durchlass = 0;
            if (parseMode(antwort, modus, durchlass)) {
                if (modus != m_mode || durchlass != m_passbandHz || m_meldeModus) {
                    m_meldeModus = false;
                    m_mode = modus;
                    m_passbandHz = durchlass;
                    emit modeChanged(m_mode, m_passbandHz);
                }
            }
            // Ein Geraet ohne Modusabfrage ist kein Fehler, den der
            // Betreiber lesen muss -- die Frequenz ist, worauf es
            // ankommt. Ins Protokoll, nicht auf den Schirm.
            else {
                qCDebug(lcRigctld) << "mode query not answered:" << antwort;
            }
            break;
        }
        case Pending::Ptt: {
            bool getastet = false;
            if (parsePtt(antwort, getastet) && getastet != m_ptt) {
                m_ptt = getastet;
                emit pttChanged(m_ptt);
            }
            break;
        }
        case Pending::Report: {
            int code = 0;
            if (parseReport(antwort, code) && code != 0) {
                emit errorOccurred(describeReport(code));
            }
            break;
        }
        case Pending::Roh:     // oben erledigt, kommt hier nie an
        case Pending::Nullbyte:
        case Pending::None:
            break;
        }

        pump();
    }

    // Was nach der letzten erwarteten Antwort noch kommt, gehoert
    // niemandem. Es wegzuwerfen ist richtiger, als es dem naechsten
    // Befehl als Antwort zu geben.
    if (m_awaiting == Pending::None && m_queue.isEmpty()
        && !m_buffer.isEmpty()) {
        qCDebug(lcRigctld) << "unasked-for bytes dropped:" << m_buffer;
        m_buffer.clear();
    }
}

void RigctldClient::setState(State s)
{
    if (m_state == s) { return; }
    m_state = s;
    if (s != State::Connected) {
        m_lastFrequency = QDateTime();
    }
    emit stateChanged(s);
}

void RigctldClient::fail(const QString& why)
{
    // Socketfehler kommen zusammen mit disconnected(); denselben
    // Fehlschlag nicht zweimal melden.
    if (m_state != State::Disconnected) {
        emit errorOccurred(why);
    }
}

// ── Protokoll ───────────────────────────────────────────────────────

bool RigctldClient::parseFrequency(const QByteArray& reply, qint64& hz)
{
    const QByteArray z = reply.trimmed();
    if (z.isEmpty() || istReport(z)) { return false; }

    bool ok = false;
    const qint64 wert = z.split('\n').first().trimmed().toLongLong(&ok);
    if (!ok || wert <= 0) { return false; }
    hz = wert;
    return true;
}

bool RigctldClient::parseMode(const QByteArray& reply, QString& mode,
                              int& passbandHz)
{
    const QByteArray z = reply.trimmed();
    if (z.isEmpty() || istReport(z)) { return false; }

    const QList<QByteArray> zeilen = z.split('\n');
    const QString m = QString::fromLatin1(zeilen.first().trimmed());
    if (m.isEmpty()) { return false; }

    // Ein Modus ist ein Wort. Steht dort eine Zahl, hat jemand die
    // Antworten verwechselt, und ein „2400" als Modus anzuzeigen waere
    // schlimmer als nichts anzuzeigen.
    bool istZahl = false;
    m.toInt(&istZahl);
    if (istZahl) { return false; }

    mode = m;
    passbandHz = 0;
    if (zeilen.size() >= 2) {
        bool ok = false;
        const int breite = zeilen.at(1).trimmed().toInt(&ok);
        if (ok && breite >= 0) { passbandHz = breite; }
    }
    return true;
}

bool RigctldClient::parsePtt(const QByteArray& reply, bool& keyed)
{
    const QByteArray z = reply.trimmed();
    if (z.isEmpty() || istReport(z)) { return false; }

    bool ok = false;
    const int wert = z.split('\n').first().trimmed().toInt(&ok);
    if (!ok || wert < 0) { return false; }
    keyed = wert != 0;
    return true;
}

bool RigctldClient::parseReport(const QByteArray& reply, int& code)
{
    const QByteArray z = reply.trimmed();
    if (!istReport(z)) { return false; }

    const QByteArray erste = z.split('\n').first().trimmed();
    bool ok = false;
    const int wert = erste.mid(4).trimmed().toInt(&ok);
    if (!ok) { return false; }
    code = wert;
    return true;
}

QString RigctldClient::describeReport(int code)
{
    // Hamlibs rig_errcode_e, soweit ein Betreiber davon etwas hat.
    switch (code) {
    case 0:   return QStringLiteral("OK");
    case -1:  return QStringLiteral("invalid parameter");
    case -2:  return QStringLiteral("invalid configuration");
    case -3:  return QStringLiteral("out of memory");
    case -4:  return QStringLiteral("the rig does not support this");
    case -5:  return QStringLiteral("communication timed out");
    case -6:  return QStringLiteral("I/O error — check the cable and the port");
    case -7:  return QStringLiteral("internal Hamlib error");
    case -8:  return QStringLiteral("protocol error");
    case -9:  return QStringLiteral("the rig rejected the command");
    case -10: return QStringLiteral("the rig returned something unexpected");
    case -11: return QStringLiteral("this rig cannot be asked that");
    case -12: return QStringLiteral("value out of range for this rig");
    case -13: return QStringLiteral("function not available");
    case -14: return QStringLiteral("VFO not targetable");
    default:
        return QStringLiteral("Hamlib error %1").arg(code);
    }
}

QByteArray RigctldClient::setFrequencyCommand(qint64 hz)
{
    // Ganze Hertz, ohne Trennzeichen, ohne Gebietsschema.
    return QByteArrayLiteral("F ") + QByteArray::number(hz)
           + QByteArrayLiteral("\n");
}

} // namespace Longpath
