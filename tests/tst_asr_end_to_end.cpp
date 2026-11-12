// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vom Ton zum Text — der ganze Weg, mit einem gestellten Erkenner.
//
// 2026-08-23. Der Betreiber hat sich fuer den oertlichen Dienst
// entschieden ("1"), also Whisper auf seinem eigenen Rechner. Der Weg
// dorthin fuehrt durch vier Stationen:
//
//   Abgriff -> umtasten 48 kHz Stereo auf 16 kHz Mono
//           -> Zerleger (Sprechabschnitte finden)
//           -> Erkenner auf eigenem Faden
//           -> Text
//
// Jede einzeln zu pruefen genuegt nicht: die Uebergaenge sind die
// gefaehrlichen Stellen. Einer davon ist die Warteschlange zum
// Arbeiterfaden — Qt schickt einen Typ nur darueber, wenn er
// angemeldet ist, und schlaegt sonst STILL fehl. Kein
// Uebersetzungsfehler, keine Meldung, nur kein Text.
//
// Der Erkenner ist hier gestellt: er braucht keinen Dienst, keine
// Modelldatei und kein Netz. Geprueft wird der WEG, nicht Whisper.

#include <QtTest>
#include <QSignalSpy>

#include "asr/AsrService.h"
#include "asr/IAsrBackend.h"
#include "asr/RemoteAsrBackend.h"

#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cmath>
#include <memory>

using namespace Longpath;

namespace {

// Ein Erkenner, der zaehlt, was bei ihm ankommt, und eine feste
// Antwort gibt.
class FakeBackend : public IAsrBackend {
public:
    bool load(const QString&, QString*) override { m_loaded = true; return true; }
    bool isLoaded() const override { return m_loaded; }
    void unload() override { m_loaded = false; }

    AsrTranscript transcribe(const std::vector<float>& pcm, QString*) override
    {
        ++calls;
        lastSamples = int(pcm.size());
        return {QStringLiteral("CQ CQ DE OE5SOS"), 0.87f};
    }

    std::atomic<int> calls{0};
    std::atomic<int> lastSamples{0};
private:
    bool m_loaded = false;
};

// Ein Sprechabschnitt: Rauschen mit Sprechpegel, dann Stille.
// Verschraenktes Stereo bei 48 kHz, wie es aus der Mischung kommt.
QVector<float> speechThenSilence(int speechMs, int silenceMs, int rate = 48000)
{
    const int sFrames = rate * speechMs / 1000;
    const int qFrames = rate * silenceMs / 1000;
    QVector<float> out((sFrames + qFrames) * 2, 0.0f);
    for (int i = 0; i < sFrames; ++i) {
        // 0,2 Effektivwert liegt deutlich ueber der Vorgabeschwelle
        // von 0,010 — ein Sprechabschnitt, kein Rauschteppich.
        const float v = 0.28f * std::sin(float(i) * 0.05f);
        out[i * 2]     = v;
        out[i * 2 + 1] = v;
    }
    return out;
}

// ── Ein Whisper-Dienst, der sich Zeit laesst ────────────────────────
//
// whisper-server rechnet immer nur EINE Anfrage (ein Riegel um das
// Modell); weitere warten. Dieser Schein-Dienst tut dasselbe: er nimmt
// jede Anfrage an, beantwortet sie aber erst nacheinander, jede nach
// `rechenzeitMs`. Er zaehlt mit, wie viele zugleich offen waren und wie
// viele der Aufrufer aufgegeben hat, bevor er drankam — im echten
// Protokoll steht dann "client disconnected, aborted processing".
class LangsamerDienst : public QTcpServer {
public:
    explicit LangsamerDienst(int rechenzeitMs) : m_rechenzeitMs(rechenzeitMs)
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* sock = nextPendingConnection()) {
                annehmen(sock);
            }
        });
    }

    int offen = 0;               // ganz empfangen, noch nicht beantwortet
    int maxOffen = 0;
    int beantwortet = 0;
    int abgebrochen = 0;         // der Aufrufer war weg, bevor die Antwort kam
    qint64 werteBeantwortet = 0; // 16-kHz-Werte in beantworteten Anfragen
    qint64 werteAbgebrochen = 0;

private:
    struct Anfrage { QPointer<QTcpSocket> sock; qint64 werte; };

    void annehmen(QTcpSocket* sock)
    {
        auto buf = std::make_shared<QByteArray>();
        connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf] {
            buf->append(sock->readAll());
            const int kopfEnde = buf->indexOf("\r\n\r\n");
            if (kopfEnde < 0) { return; }
            int laenge = 0;
            for (const QByteArray& zeile : buf->left(kopfEnde).split('\n')) {
                const QByteArray z = zeile.trimmed().toLower();
                if (z.startsWith("content-length:")) { laenge = z.mid(15).trimmed().toInt(); }
            }
            if (buf->size() - (kopfEnde + 4) < laenge) { return; }
            // Die WAV-Datei im Formular: Datengroesse steht 40 Byte hinter "RIFF".
            qint64 werte = 0;
            const int riff = buf->indexOf("RIFF", kopfEnde);
            if (riff >= 0 && buf->size() >= riff + 44) {
                const auto* p = reinterpret_cast<const uchar*>(buf->constData() + riff + 40);
                werte = qint64(p[0] | (p[1] << 8) | (p[2] << 16) | (quint32(p[3]) << 24)) / 2;
            }
            buf->clear();
            ++offen;
            maxOffen = qMax(maxOffen, offen);
            m_warte.append({sock, werte});
            if (!m_rechnet) { naechste(); }
        });
    }

    void naechste()
    {
        if (m_warte.isEmpty()) { m_rechnet = false; return; }
        m_rechnet = true;
        const Anfrage a = m_warte.takeFirst();
        QTimer::singleShot(m_rechenzeitMs, this, [this, a] {
            --offen;
            if (!a.sock || a.sock->state() != QAbstractSocket::ConnectedState) {
                ++abgebrochen;
                werteAbgebrochen += a.werte;
            } else {
                ++beantwortet;
                werteBeantwortet += a.werte;
                const QByteArray json = "{\"text\":\"Abschnitt "
                                        + QByteArray::number(beantwortet) + "\"}";
                a.sock->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                              "Content-Length: " + QByteArray::number(json.size())
                              + "\r\nConnection: close\r\n\r\n" + json);
                a.sock->flush();
                a.sock->disconnectFromHost();
            }
            naechste();
        });
    }

    int m_rechenzeitMs;
    bool m_rechnet = false;
    QList<Anfrage> m_warte;
};

} // namespace

class TstAsrEndToEnd : public QObject
{
    Q_OBJECT

private slots:
    void tonWirdZuText()
    {
        AsrService svc;
        auto fake = std::make_unique<FakeBackend>();
        FakeBackend* raw = fake.get();
        svc.setBackend(std::move(fake));
        svc.start();
        QVERIFY(svc.isRunning());

        QSignalSpy got(&svc, &AsrService::transcript);

        const QVector<float> audio = speechThenSilence(800, 600);
        svc.feedForTest(audio.constData(), audio.size() / 2, 48000);

        // Der Erkenner sitzt auf einem eigenen Faden; das Ergebnis
        // kommt ueber die Warteschlange zurueck.
        QVERIFY2(got.wait(5000),
                 "Kein Text angekommen — der Weg ist irgendwo unterbrochen");
        QCOMPARE(got.count(), 1);
        QCOMPARE(got.at(0).at(0).toString(), QStringLiteral("CQ CQ DE OE5SOS"));
        qInfo().noquote() << "Text:" << got.at(0).at(0).toString()
                          << " Zuversicht:" << got.at(0).at(1).toFloat();

        // ── Die Umtastung ist wirklich passiert ─────────────────────
        //
        // 800 ms Sprache PLUS 300 ms Nachlauf, den der Zerleger
        // absichtlich mitnimmt (hangoverMs), sind 1100 ms. Bei 16 kHz
        // also 17 600 Werte. Kaeme der Ton ungetastet an, waeren es
        // dreimal so viele — und Whisper bekaeme 48 kHz vorgesetzt, wo
        // es 16 erwartet, und lieferte etwas, das plausibel klingt und
        // nicht stimmt.
        //
        // ── Was diese Zeile schon gefunden hat ──────────────────────
        //
        // Beim ersten Lauf standen hier 11 040 Werte, beim naechsten
        // 17 600 — dieselbe Eingabe, verschiedene Ergebnisse. Die
        // Ursache war ein echter Fehler: AsrService gab dem Wandler
        // bis zu 67 200 Rahmen auf einmal, obwohl er fuer 16 384
        // gebaut war. Es kam Ton heraus, nur die falsche Menge, und
        // ohne diese Zaehlung waere das nie aufgefallen.
        //
        // Die erste Fassung dieser Zeile erwartete 12 800 und war
        // damit selbst falsch — der Nachlauf war mir entgangen.
        const int n = raw->lastSamples.load();
        qInfo() << "Werte an den Erkenner:" << n << "(erwartet 17600)";
        QVERIFY2(n > 15000 && n < 20000,
                 qPrintable(QStringLiteral("%1 Werte — die Umtastung "
                                           "stimmt nicht").arg(n)));
        svc.stop();
    }

    void stilleErzeugtKeinenAufruf()
    {
        // Whisper auf Rauschen laufen zu lassen kostet Rechenzeit und
        // liefert erfundene Woerter. Der Zerleger muss das abfangen.
        AsrService svc;
        auto fake = std::make_unique<FakeBackend>();
        FakeBackend* raw = fake.get();
        svc.setBackend(std::move(fake));
        svc.start();

        QVector<float> quiet(48000 * 2, 0.0f);
        svc.feedForTest(quiet.constData(), 48000, 48000);
        QTest::qWait(300);
        qInfo() << "Aufrufe bei Stille:" << raw->calls.load();
        QCOMPARE(raw->calls.load(), 0);
        svc.stop();
    }

    void langsamerDienstVerliertNichts()
    {
        // ── Der Befund vom 2026-11-12 ───────────────────────────────
        //
        // Martin: "transcribe funktioniert schlecht". Im Protokoll des
        // Whisper-Dienstes standen 22 von 41 Abschnitten als
        // "client disconnected, aborted processing" — Longpath hatte
        // aufgegeben, bevor der Dienst drankam.
        //
        // Die Ursache sass im Arbeiter: der Erkenner wartet auf die
        // Antwort in einer eigenen Ereignisschleife, und die arbeitet
        // auch die WARTESCHLANGE des Fadens ab. Der naechste Abschnitt
        // startete also mitten im Warten auf den vorigen, und der
        // uebernaechste darin wieder. Der Dienst bekam alles zugleich,
        // rechnete es nacheinander, und was hinten stand, lief in die
        // Zeitgrenze.
        //
        // Vier Durchgaenge kurz hintereinander gegen einen Dienst, der
        // je 1,5 s rechnet, bei 4 s Zeitgrenze: nacheinander geht das
        // auf, alles zugleich nicht.
        LangsamerDienst dienst(1500);
        QVERIFY(dienst.listen(QHostAddress::LocalHost));

        RemoteAsrConfig cfg;
        cfg.url = QStringLiteral("http://127.0.0.1:%1/inference").arg(dienst.serverPort());
        cfg.timeoutMs = 4000;

        AsrService svc;
        svc.setBackend(std::make_unique<RemoteAsrBackend>(cfg));
        QSignalSpy got(&svc, &AsrService::transcript);
        QSignalSpy fail(&svc, &AsrService::failed);
        svc.start();
        QVERIFY(svc.isRunning());

        constexpr int kDurchgaenge = 4;
        QVector<float> audio;
        for (int i = 0; i < kDurchgaenge; ++i) { audio += speechThenSilence(800, 600); }
        svc.feedForTest(audio.constData(), audio.size() / 2, 48000);

        // Je Durchgang 17 600 Werte (siehe tonWirdZuText). Gewartet wird,
        // bis aller Ton beim Dienst angekommen ODER verworfen ist.
        constexpr qint64 kJeDurchgang = 17600;
        QTRY_VERIFY_WITH_TIMEOUT(
            dienst.werteBeantwortet + dienst.werteAbgebrochen
                >= kDurchgaenge * kJeDurchgang * 9 / 10
            && dienst.offen == 0,
            15000);

        qInfo() << "Anfragen zugleich hoechstens:" << dienst.maxOffen
                << " beantwortet:" << dienst.beantwortet
                << " abgebrochen:" << dienst.abgebrochen
                << " Werte beantwortet:" << dienst.werteBeantwortet
                << " verworfen:" << dienst.werteAbgebrochen
                << " Texte:" << got.count() << " Fehler:" << fail.count();

        QVERIFY2(dienst.maxOffen == 1,
                 qPrintable(QStringLiteral("%1 Anfragen gleichzeitig beim Dienst — "
                                           "der Arbeiter laeuft in sich selbst hinein")
                                .arg(dienst.maxOffen)));
        QVERIFY2(dienst.abgebrochen == 0,
                 qPrintable(QStringLiteral("%1 Abschnitte verworfen, weil sie zu lange "
                                           "warteten").arg(dienst.abgebrochen)));
        QCOMPARE(fail.count(), 0);
        // Aller Ton kam an — zusammengelegt oder einzeln.
        QVERIFY(dienst.werteBeantwortet >= kDurchgaenge * kJeDurchgang * 9 / 10);
        // Jede Antwort wird zu genau einem Text, in der Reihenfolge des Dienstes.
        QTRY_COMPARE_WITH_TIMEOUT(got.count(), dienst.beantwortet, 2000);
        for (int i = 0; i < got.count(); ++i) {
            QCOMPARE(got.at(i).at(0).toString(), QStringLiteral("Abschnitt %1").arg(i + 1));
        }
        svc.stop();
    }

    void anhaltenBeendetDenFaden()
    {
        // Der Arbeiter haelt einen nackten Zeiger auf den Erkenner.
        // Wird der freigegeben, waehrend der Faden noch laeuft, ist das
        // ein Absturz mit Ansage.
        AsrService svc;
        svc.setBackend(std::make_unique<FakeBackend>());
        svc.start();
        QVERIFY(svc.isRunning());
        svc.stop();
        QVERIFY(!svc.isRunning());
        // Und ein zweites Mal darf nichts tun.
        svc.stop();
        qInfo() << "Anhalten ist wiederholbar";
    }
};

QTEST_MAIN(TstAsrEndToEnd)
#include "tst_asr_end_to_end.moc"
