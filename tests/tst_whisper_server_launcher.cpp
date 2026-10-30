// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Whisper-Dienst aus Longpath heraus (2026-09-17): Argumente, die
// Vorgaben, die Fehlerfaelle mit Grund, Start und Stopp eines echten
// Prozesses — mit einem Shell-Skript als Stellvertreter, das laeuft,
// bis man es beendet. Kein Netz, kein Modell.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QTcpServer>
#include <QTemporaryDir>
#include "asr/WhisperServerLauncher.h"
#include "PortProbe.h"

using namespace Longpath;
using S = WhisperServerLauncher::State;

// Der eigene Port kommt jetzt aus tests/PortProbe.h — binden statt
// verbinden. Warum, steht dort: am 2026-10-30 fielen drei Faelle dieses
// Prueflaufs um, weil ein TCP-Connect auf dem CI-Laeufer Erfolg meldete,
// wo niemand lauschte (Lauf 41020438997). Dieselbe Luege traf
// tst_asr_against_live_server und tst_tci_client_against_live_device.
namespace {
quint16 freePort() { return LongpathTest::freeLocalPort(); }
} // namespace

class TestWhisperServerLauncher : public QObject
{
    Q_OBJECT
private slots:
    void argumentsCarryModelPortAndLanguage()
    {
        WhisperServerLauncher::Config c;
        c.binary = QStringLiteral("/x/whisper-server");
        c.model = QStringLiteral("/m/ggml-small.bin");
        c.endpoint = QStringLiteral("http://127.0.0.1:9091/inference");
        c.language = QStringLiteral("de");
        const QStringList a = WhisperServerLauncher::argumentsFor(c);
        QCOMPARE(a, (QStringList{"-m", "/m/ggml-small.bin", "--host", "127.0.0.1",
                                 "--port", "9091", "-l", "de"}));
        c.endpoint = QStringLiteral("http://localhost/inference");   // kein Port
        c.language = QStringLiteral("auto");
        const QStringList b = WhisperServerLauncher::argumentsFor(c);
        QVERIFY(b.contains(QStringLiteral("8080")));
        QVERIFY2(!b.contains(QStringLiteral("-l")), "auto heisst: keine Sprache vorgeben");
    }

    void onlyLocalEndpointsAreStarted()
    {
        QVERIFY(WhisperServerLauncher::endpointIsLocal(QStringLiteral("http://127.0.0.1:8080/inference")));
        QVERIFY(WhisperServerLauncher::endpointIsLocal(QStringLiteral("http://localhost:8080/inference")));
        QVERIFY(!WhisperServerLauncher::endpointIsLocal(QStringLiteral("https://api.example.com/v1/audio")));
    }

    void missingBinaryOrModelFailsWithAReason()
    {
        WhisperServerLauncher l;
        QSignalSpy spy(&l, &WhisperServerLauncher::stateChanged);
        WhisperServerLauncher::Config c;
        c.binary = QStringLiteral("/nirgends/whisper-server");
        c.model = QStringLiteral("/nirgends/ggml.bin");
        c.endpoint = QStringLiteral("http://127.0.0.1:%1/inference").arg(freePort());
        l.start(c);
        QCOMPARE(l.state(), S::Failed);
        QVERIFY(l.reason().contains(QStringLiteral("nicht ausfuehrbar")));
        c.binary = QStringLiteral("/bin/sh");
        l.start(c);
        QCOMPARE(l.state(), S::Failed);
        QVERIFY(l.reason().contains(QStringLiteral("Modell nicht gefunden")));
        QVERIFY(spy.count() >= 2);
    }

    void aRealProcessRunsAndStops()
    {
        QTemporaryDir tmp;
        // Ein Stellvertreter: nimmt die Argumente entgegen, meldet sich
        // und wartet, bis er beendet wird.
        const QString script = tmp.filePath(QStringLiteral("fake-whisper-server"));
        {
            QFile f(script);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("#!/bin/sh\necho \"fake $@\"\nwhile :; do sleep 1; done\n");
            f.close();
            QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        }
        const QString model = tmp.filePath(QStringLiteral("ggml-small.bin"));
        { QFile m(model); QVERIFY(m.open(QIODevice::WriteOnly)); m.write("x"); }

        WhisperServerLauncher l;
        QSignalSpy spy(&l, &WhisperServerLauncher::stateChanged);
        WhisperServerLauncher::Config c;
        c.binary = script; c.model = model;
        const quint16 port = freePort();
        QVERIFY(port != 0);
        c.endpoint = QStringLiteral("http://127.0.0.1:%1/inference").arg(port);
        c.language = QStringLiteral("de");
        l.start(c);
        // ── Grosszuegig warten (2026-10-16) ─────────────────────
        //
        // Hier standen 5000 ms. Unter Last reicht das nicht: im
        // `ctest -j6`-Durchgang vom 2026-10-15 fiel die zweite Zeile
        // um — in fuenf Sekunden war von einem ECHTEN Prozess noch
        // keine Ausgabe da. Einzeln ist das nicht nachzustellen (zwoelf
        // Laeufe parallel, alle gruen), die Frist ist aber der einzige
        // Kandidat, und eine grosszuegige kostet nur im Fehlerfall
        // Zeit.
        constexpr int kFrist = 20000;
        QVERIFY(QTest::qWaitFor([&] { return l.state() == S::Running; }, kFrist));
        QVERIFY(QTest::qWaitFor([&] { return !l.recentOutput().isEmpty(); }, kFrist));
        QVERIFY(l.recentOutput().first().startsWith(QStringLiteral("fake -m")));

        l.start(c);   // ein zweites start() waehrend es laeuft tut nichts
        QCOMPARE(l.state(), S::Running);

        l.stop();
        QCOMPARE(l.state(), S::Stopped);
        QVERIFY(l.reason().isEmpty());
    }

    void anExistingListenerIsUsedNotDoubled()
    {
        // Der Fall vom 2026-09-17: ein Login-Dienst lauscht schon auf
        // dem Port. whisper-server bindet mit SO_REUSEPORT, ein zweiter
        // Start wuerde also nicht scheitern, sondern still ein weiteres
        // Modell laden. Der Starter muss "Extern" melden und nichts tun.
        QTcpServer other;
        QVERIFY(other.listen(QHostAddress::LocalHost, 0));
        const QString endpoint = QStringLiteral("http://127.0.0.1:%1/inference")
                                     .arg(other.serverPort());
        QVERIFY(WhisperServerLauncher::portInUse(endpoint));

        WhisperServerLauncher l;
        WhisperServerLauncher::Config c;
        c.binary = QStringLiteral("/nirgends/whisper-server");   // darf gar nicht erst geprueft werden
        c.model = QStringLiteral("/nirgends/ggml.bin");
        c.endpoint = endpoint;
        l.start(c);
        QCOMPARE(l.state(), S::External);
        QVERIFY(l.reason().contains(QString::number(other.serverPort())));

        l.stop();   // nichts zu beenden, nur der Zustand faellt zurueck
        QCOMPARE(l.state(), S::Stopped);

        other.close();
        QVERIFY(!WhisperServerLauncher::portInUse(endpoint));
    }
};

QTEST_MAIN(TestWhisperServerLauncher)
#include "tst_whisper_server_launcher.moc"
