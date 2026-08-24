// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tools/tci_probe.cpp  (Longpath)
// =================================================================
//
// WERKZEUG, kein Programmteil: fragt einen TCI-Dienst, was er kann.
//
// ── Warum es das gibt ───────────────────────────────────────────────
//
// Der Betreiber hat einen SunSDR QRP von Expert Electronics und fragte
// am 2026-08-23, wie schwer es waere, Longpath dafuer herzurichten.
//
// Das Geraet spricht NICHT HPSDR. Unsere ganze Verbindungsschicht
// (P1RadioConnection, P2RadioConnection) redet Protokoll 1 und 2 —
// davon versteht der SunSDR kein Wort. Der einzige gangbare Weg fuehrt
// ueber TCI, also ueber ExpertSDR3 als Unterbau.
//
// Und dort haengt alles an EINER Frage: gibt ExpertSDR3 ueber TCI auch
// SPEKTRUM und TON heraus, oder nur Steuerbefehle?
//
// Steuerung ist sicher — Frequenz, Betriebsart, Sendetaste kann jedes
// TCI. Bei Thetis kamen Audio- und IQ-Stroeme erst mit 2.10.3.13; ob
// Expert Electronics das in ihrer eigenen Umsetzung hat, weiss ich
// nicht und will es nicht behaupten.
//
// Diese Sonde beantwortet das in zehn Sekunden, statt dass ich Wochen
// in eine Vermutung stecke. Sie baut nichts, sie aendert nichts — sie
// verbindet sich, hoert zu, fragt die Stroeme an und berichtet.
//
// ── Aufruf ──────────────────────────────────────────────────────────
//
//   tci_probe [ws://127.0.0.1:40001] [sekunden]
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <cstdio>

namespace {

QTextStream& out()
{
    static QTextStream s(stdout);
    return s;
}

// Der 64-Byte-Kopf eines TCI-Binaerrahmens. Aufbau zeichengetreu aus
// unserem eigenen TciBinaryFrame.h — dort steht er als BAUANLEITUNG,
// hier wird er gelesen.
struct StreamHeader {
    quint32 receiver = 0;
    quint32 sampleRate = 0;
    quint32 sampleType = 0;
    quint32 length = 0;
    quint32 streamType = 0;
    quint32 channels = 0;
    bool valid = false;
};

StreamHeader parseHeader(const QByteArray& frame)
{
    StreamHeader h;
    if (frame.size() < 64) { return h; }
    auto u32 = [&](int off) {
        return quint32(quint8(frame[off]))
             | (quint32(quint8(frame[off + 1])) << 8)
             | (quint32(quint8(frame[off + 2])) << 16)
             | (quint32(quint8(frame[off + 3])) << 24);
    };
    h.receiver   = u32(0);
    h.sampleRate = u32(4);
    h.sampleType = u32(8);
    h.length     = u32(20);
    h.streamType = u32(24);
    h.channels   = u32(28);
    h.valid = true;
    return h;
}

QString streamName(quint32 t)
{
    switch (t) {
    case 0: return QStringLiteral("IQ");
    case 1: return QStringLiteral("RX-Ton");
    case 2: return QStringLiteral("TX-Ton");
    case 3: return QStringLiteral("TX-Takt");
    case 4: return QStringLiteral("Lineout");
    default: return QStringLiteral("unbekannt(%1)").arg(t);
    }
}

QString sampleName(quint32 t)
{
    switch (t) {
    case 0: return QStringLiteral("Int16");
    case 1: return QStringLiteral("Int24");
    case 2: return QStringLiteral("Int32");
    case 3: return QStringLiteral("Float32");
    default: return QStringLiteral("unbekannt(%1)").arg(t);
    }
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const QString url = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                   : QStringLiteral("ws://127.0.0.1:40001");
    const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 12;

    out() << "TCI-Sonde — " << url << ", " << seconds << " Sekunden\n";
    out() << "------------------------------------------------------------\n";
    out().flush();

    QWebSocket sock;
    QStringList textLines;
    QMap<quint32, StreamHeader> streams;   // je Stromart der letzte Kopf
    QMap<quint32, int> frameCounts;
    bool sawReady = false;
    bool connected = false;
    // Ob der Dienst die Strombefehle BESTAETIGT hat. Das ist der
    // Unterschied zwischen "kennt sie nicht" und "kennt sie, hat
    // aber nichts zu senden" — und den hat die erste Fassung
    // dieser Sonde am 2026-08-24 verwischt.
    bool ackAudioStart = false;
    bool ackIqStart = false;
    bool sawStreamRates = false;

    QObject::connect(&sock, &QWebSocket::connected, [&]() {
        connected = true;
        out() << "verbunden.\n\n";
        out().flush();
    });

    QObject::connect(&sock, &QWebSocket::textMessageReceived,
                     [&](const QString& msg) {
        // TCI schickt seine Selbstauskunft als Folge von Befehlen,
        // abgeschlossen mit "ready;". ALLES davon ist interessant: dort
        // steht, welches Geraet, wieviele Empfaenger, welche Raten.
        for (const QString& part : msg.split(QLatin1Char(';'),
                                             Qt::SkipEmptyParts)) {
            const QString t = part.trimmed();
            if (t.isEmpty()) { continue; }
            textLines << t;
            out() << "  < " << t << "\n";
            if (t.startsWith(QStringLiteral("audio_start"))) { ackAudioStart = true; }
            if (t.startsWith(QStringLiteral("iq_start")))    { ackIqStart = true; }
            if (t.startsWith(QStringLiteral("iq_samplerate"))
                || t.startsWith(QStringLiteral("audio_samplerate"))) {
                sawStreamRates = true;
            }
            if (t.compare(QStringLiteral("ready"), Qt::CaseInsensitive) == 0) {
                sawReady = true;
                out() << "\n  -- Selbstauskunft zu Ende. Jetzt Stroeme "
                         "anfordern --\n";
                out().flush();
                // Genau die Befehle, die auch unser eigener TciServer
                // versteht — siehe TciServer.cpp.
                for (const QString& cmd : {
                         QStringLiteral("audio_samplerate:48000;"),
                         QStringLiteral("audio_start:0;"),
                         QStringLiteral("iq_start:0;") }) {
                    out() << "  > " << cmd << "\n";
                    sock.sendTextMessage(cmd);
                }
                out() << "\n";
            }
        }
        out().flush();
    });

    QObject::connect(&sock, &QWebSocket::binaryMessageReceived,
                     [&](const QByteArray& frame) {
        const StreamHeader h = parseHeader(frame);
        if (!h.valid) { return; }
        frameCounts[h.streamType]++;
        streams[h.streamType] = h;
    });

    QObject::connect(&sock, &QWebSocket::errorOccurred,
                     [&](QAbstractSocket::SocketError) {
        out() << "FEHLER: " << sock.errorString() << "\n";
        out().flush();
    });

    sock.open(QUrl(url));

    QTimer::singleShot(seconds * 1000, [&]() {
        out() << "\n------------------------------------------------------------\n";
        out() << "BEFUND\n\n";

        if (!connected) {
            out() << "  Keine Verbindung zu " << url << ".\n\n"
                  << "  Zu pruefen:\n"
                  << "   - laeuft ExpertSDR3?\n"
                  << "   - ist TCI eingeschaltet (Options -> TCI)?\n"
                  << "   - stimmt der Port? (Vorgabe 40001)\n";
            out().flush();
            app.exit(1);
            return;
        }

        out() << "  Verbindung:      steht\n";
        out() << "  Selbstauskunft:  " << textLines.size() << " Zeilen"
              << (sawReady ? QStringLiteral(", mit 'ready'")
                           : QStringLiteral(", OHNE 'ready' (unvollstaendig?)"))
              << "\n\n";

        if (frameCounts.isEmpty()) {
            // ── Zwei sehr verschiedene Faelle, nicht einer ──────────
            //
            // Die erste Fassung dieser Sonde schrieb hier pauschal
            // "liefert nur Steuerbefehle". Das war zu weit gegriffen
            // und haette den Betreiber am 2026-08-24 beinahe in die
            // Irre gefuehrt: sein ExpertSDR2 hat audio_start und
            // iq_start BESTAETIGT und die Abtastraten genannt — es
            // kennt die Stroeme also sehr wohl. Es lief nur ohne
            // angeschlossenes Geraet, und dann gibt es nichts zu
            // senden.
            //
            // Ein Werkzeug, das aus "keine Daten" auf "kann es nicht"
            // schliesst, beantwortet die Frage falsch.
            out() << "  Stroeme:         keine Daten angekommen.\n\n";
            if (ackAudioStart || ackIqStart || sawStreamRates) {
                out() << "  ABER: der Dienst hat die Strombefehle BESTAETIGT\n";
                if (ackAudioStart) { out() << "        - audio_start\n"; }
                if (ackIqStart)    { out() << "        - iq_start\n"; }
                if (sawStreamRates) {
                    out() << "        - und nennt Abtastraten fuer beide\n";
                }
                out() << "\n  Er KENNT die Stroeme also. Dass nichts kam, hat\n"
                      << "  vermutlich einen einfacheren Grund:\n\n"
                      << "   - ist das Funkgeraet angeschlossen und in Betrieb?\n"
                      << "   - laeuft der Empfang (nicht nur das Programm)?\n"
                      << "   - steht rx_enable auf true?\n\n"
                      << "  Bitte mit angeschlossenem Geraet wiederholen. Erst\n"
                      << "  dann ist die Frage beantwortet.\n";
            } else {
                out() << "  Der Dienst hat die Strombefehle NICHT bestaetigt und\n"
                      << "  keine Abtastraten genannt. Das spricht dafuer, dass\n"
                      << "  dieses TCI wirklich nur Steuerbefehle kann —\n"
                      << "  Longpath koennte das Geraet dann BEDIENEN, aber\n"
                      << "  kein Spektrum und keinen Ton zeigen.\n";
            }
        } else {
            out() << "  Stroeme:\n";
            for (auto it = frameCounts.cbegin(); it != frameCounts.cend(); ++it) {
                const StreamHeader& h = streams[it.key()];
                out() << "    " << streamName(it.key()).leftJustified(10)
                      << it.value() << " Rahmen"
                      << ", Empfaenger " << h.receiver
                      << ", " << h.sampleRate << " Hz"
                      << ", " << sampleName(h.sampleType)
                      << ", " << h.channels << " Kanaele\n";
            }
            out() << "\n  Das traegt. Longpath koennte damit Spektrum und Ton\n"
                     "  des SunSDR zeigen.\n";
        }

        out() << "\n  Die vollstaendige Selbstauskunft steht oben — bitte\n"
                 "  mitschicken, daraus ergibt sich der Rest.\n";
        out().flush();
        app.quit();
    });

    return app.exec();
}
