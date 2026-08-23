// SPDX-License-Identifier: GPL-3.0-or-later
//
// UNSER Backend gegen einen WIRKLICH laufenden Whisper-Server.
//
// 2026-08-23. Der Betreiber hat den oertlichen Weg gewaehlt und
// whisper-server gestartet. Alle bisherigen ASR-Pruefungen benutzen
// einen gestellten Erkenner — sie zeigen, dass der WEG stimmt, aber
// nicht, dass unsere Anfrage zu einem echten Server passt.
//
// Diese hier schliesst die Luecke: sie schickt eine echte
// HTTP-Anfrage an 127.0.0.1:8080 und prueft, was zurueckkommt.
//
// ── Warum sie sich selbst ueberspringt ──────────────────────────────
//
// Auf einem Rechner ohne laufenden Server waere ein Fehlschlag hier
// eine Falschmeldung — er saegte "unser Code ist kaputt", wo nur der
// Dienst fehlt. Ein Gate, das aus fremden Gruenden rot wird, verliert
// seinen Wert. Also: kein Server, kein Urteil.

#include <QtTest>
#include <QTcpSocket>

#include "asr/RemoteAsrBackend.h"

#include <cmath>

using namespace Longpath;

namespace {

bool serverIsUp(quint16 port = 8080)
{
    QTcpSocket sock;
    sock.connectToHost(QStringLiteral("127.0.0.1"), port);
    return sock.waitForConnected(300);
}

// Eine Sekunde Ton bei 16 kHz. Kein Sprachsignal — es geht um Format
// und Antwortweg, nicht um den Inhalt der Erkennung.
std::vector<float> oneSecond()
{
    std::vector<float> v(16000);
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = 0.25f * std::sin(float(i) * 0.05f);
    }
    return v;
}

} // namespace

class TstAsrAgainstLiveServer : public QObject
{
    Q_OBJECT

private slots:
    void unsereAnfrageKommtDurch()
    {
        if (!serverIsUp()) {
            QSKIP("Kein Whisper-Server auf 127.0.0.1:8080 — nichts zu pruefen.");
        }

        RemoteAsrConfig cfg;
        cfg.url = QStringLiteral("http://127.0.0.1:8080/inference");
        cfg.language = QStringLiteral("de");
        cfg.timeoutMs = 60000;

        RemoteAsrBackend backend(cfg);
        QString error;
        QVERIFY2(backend.load(QString(), &error), qPrintable(error));

        const AsrTranscript t = backend.transcribe(oneSecond(), &error);

        // Das Wichtigste zuerst: KEIN Fehler. Ein Formatfehler auf
        // unserer Seite kaeme genau hier heraus — falscher Feldname,
        // falscher Inhaltstyp, falscher Pfad.
        QVERIFY2(error.isEmpty(), qPrintable(error));

        qInfo().noquote() << "Antwort:" << (t.text.isEmpty()
                                                ? QStringLiteral("(leer)")
                                                : t.text)
                          << "· Zuversicht:" << t.confidence;

        // Die Zuversicht kommt aus avg_logprob der Segmente. Steht sie
        // auf 0, hat unser Auslesen die Antwortstruktur nicht
        // getroffen — auch dann, wenn Text ankam.
        QVERIFY2(t.confidence > 0.0f && t.confidence <= 1.0f,
                 qPrintable(QStringLiteral("Zuversicht %1 — das Auslesen "
                                           "der Antwort trifft nicht")
                                .arg(t.confidence)));
    }

    void einFalscherPfadMeldetSichAlsFehler()
    {
        if (!serverIsUp()) { QSKIP("Kein Server."); }

        // Gegenprobe: schlaegt es fehl, wenn es fehlschlagen MUSS?
        // Ohne die waere nicht belegt, dass die Pruefung oben etwas
        // aussagt.
        RemoteAsrConfig cfg;
        cfg.url = QStringLiteral("http://127.0.0.1:8080/gibtesnicht");
        cfg.timeoutMs = 10000;
        RemoteAsrBackend backend(cfg);
        QString error;
        QVERIFY(backend.load(QString(), &error));
        backend.transcribe(oneSecond(), &error);
        qInfo().noquote() << "falscher Pfad meldet:" << error;
        QVERIFY2(!error.isEmpty(),
                 "Ein falscher Pfad wird stillschweigend hingenommen");
    }
};

QTEST_MAIN(TstAsrAgainstLiveServer)
#include "tst_asr_against_live_server.moc"
