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
    int     frameBytes = 0;   // ganzer Rahmen, Kopf eingerechnet
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
    h.frameBytes = frame.size();
    h.valid = true;
    return h;
}

// Die wahre Rate: Textkanal wenn vorhanden, sonst notgedrungen der Kopf.
quint32 effectiveRate(quint32 streamType, quint32 headerRate,
                      quint32 iqRate, quint32 audioRate)
{
    if (streamType == 0 && iqRate > 0)    { return iqRate; }
    if (streamType != 0 && audioRate > 0) { return audioRate; }
    return headerRate;
}

int sampleWidthBytes(quint32 sampleType)
{
    switch (sampleType) {
    case 0: return 2;   // Int16
    case 1: return 4;   // Int24 laeuft bei TCI als 4-Byte-Wort
    case 2: return 4;   // Int32
    case 3: return 4;   // Float32
    default: return 0;
    }
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
    const QString extraCmds = (argc > 3) ? QString::fromLocal8Bit(argv[3]) : QString();

    out() << "TCI-Sonde — " << url << ", " << seconds << " Sekunden\n";
    out() << "------------------------------------------------------------\n";
    out().flush();

    QWebSocket sock;
    QStringList textLines;
    // Die Raten kommen aus dem Textkanal. Das Ratenfeld im Binaerkopf
    // folgt einer Umstellung NICHT: nach iq_samplerate:96000 steht dort
    // weiter 48000, waehrend doppelt so viele Rahmen kommen. Wer den
    // Kopf glaubt, bekommt die halbe Spanne und Ton in halber
    // Geschwindigkeit.
    quint32 iqRateFromText = 0;
    quint32 audioRateFromText = 0;
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
    // Laeuft der Empfaenger ueberhaupt? TCI meldet das als nackte
    // Zeile "start" oder "stop". Ohne laufenden Empfaenger gibt
    // es nichts zu senden, und die Frage nach den Stroemen ist
    // gar nicht gestellt — das hat mich am 2026-08-24 einen
    // zweiten Fehlbefund gekostet.
    bool trxRunning = false;
    bool sawRunState = false;

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
            if (t == QStringLiteral("start")) { trxRunning = true;  sawRunState = true; }
            if (t == QStringLiteral("stop"))  { trxRunning = false; sawRunState = true; }
            if (t.startsWith(QStringLiteral("audio_start"))) { ackAudioStart = true; }
            if (t.startsWith(QStringLiteral("iq_start")))    { ackIqStart = true; }
            if (t.startsWith(QStringLiteral("iq_samplerate:"))) {
                iqRateFromText = t.section(QLatin1Char(':'), 1).trimmed().toUInt();
            }
            if (t.startsWith(QStringLiteral("audio_samplerate:"))) {
                audioRateFromText = t.section(QLatin1Char(':'), 1).trimmed().toUInt();
            }
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
                // Optional per Aufrufparameter 3: zusaetzliche Befehle,
                // durch Semikolon getrennt. Damit kann man ausprobieren,
                // was das Geraet annimmt, ohne die Sonde neu zu bauen.
                QStringList cmds{ QStringLiteral("audio_samplerate:48000;") };
                if (!extraCmds.isEmpty()) {
                    for (const QString& e : extraCmds.split(QLatin1Char(';'),
                                                            Qt::SkipEmptyParts)) {
                        cmds << (e.trimmed() + QLatin1Char(';'));
                    }
                }
                cmds << QStringLiteral("audio_start:0;")
                     << QStringLiteral("iq_start:0;");
                for (const QString& cmd : cmds) {
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
        // Die ersten drei Rahmen je Strom im Rohzustand: Bytes und
        // Abtastzahl. Alles andere waere Deutung.
        if (frameCounts[h.streamType] <= 3) {
            out() << "  [roh] " << streamName(h.streamType)
                  << " Rahmen " << frameCounts[h.streamType]
                  << ": " << h.frameBytes << " Byte gesamt, Feld20="
                  << h.length << ", Feld28=" << h.channels << "\n";
            out().flush();
        }
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

            // ── Zuerst: laeuft der Empfaenger ueberhaupt? ────────────
            //
            // Diese Frage geht allen anderen vor. Steht der Empfaenger,
            // gibt es nichts zu senden, und aus dem Ausbleiben der
            // Rahmen laesst sich GAR NICHTS schliessen.
            if (sawRunState && !trxRunning) {
                out() << "  ACHTUNG: der Empfaenger steht (TCI meldet \"stop\").\n\n"
                      << "  Ohne laufenden Empfaenger gibt es kein I/Q und keinen\n"
                      << "  Ton — egal, was man anfordert. Die Frage nach den\n"
                      << "  Stroemen ist damit NICHT beantwortet.\n\n"
                      << "  In ExpertSDR den Start-Knopf druecken und diese\n"
                      << "  Sonde noch einmal laufen lassen.\n";
                out().flush();
                app.exit(2);
                return;
            }
            if (sawRunState && trxRunning) {
                out() << "  Empfaenger:      laeuft (TCI meldet \"start\")\n\n";
            }

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
                      << ", " << effectiveRate(it.key(), h.sampleRate,
                                               iqRateFromText, audioRateFromText)
                      << " Hz"
                      << (h.sampleRate != effectiveRate(it.key(), h.sampleRate,
                                                        iqRateFromText, audioRateFromText)
                              ? QStringLiteral(" (Kopf sagt %1 -- falsch)").arg(h.sampleRate)
                              : QString())
                      << ", " << sampleName(h.sampleType)
                      ;
                // Feld 28 (Kanaele) ist bei ExpertSDR2 NICHT gesetzt: im
                // IQ-Rahmen steht 0, im Tonrahmen das Bitmuster einer
                // Fliesskommazahl. Also nicht lesen, sondern messen --
                // aus dem Durchsatz ergibt sich die Kanalzahl zwingend.
                // Feld 20 zaehlt dabei ALLE Werte eines Rahmens, ueber
                // die Kanaele hinweg (nachgeprueft: Byte == Feld20 * 4).
                const int width = sampleWidthBytes(h.sampleType);
                const int payload = h.frameBytes - 64;
                const quint32 rate = effectiveRate(it.key(), h.sampleRate,
                                                   iqRateFromText, audioRateFromText);
                if (width > 0 && h.length > 0 && rate > 0
                    && seconds > 0 && payload == int(h.length) * width) {
                    const double valuesPerSec =
                        double(it.value()) * double(h.length) / double(seconds);
                    const double ch = valuesPerSec / double(rate);
                    out() << ", " << qRound(ch) << " Kanaele (gemessen "
                          << QString::number(ch, 'f', 2) << ")";
                } else {
                    out() << ", Kanalzahl nicht messbar";
                }
                out() << "\n";
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
