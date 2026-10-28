// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Eine Sonde bleibt offline (2026-10-21).
//
// Gemessen an einem Livelauf mit gesetztem LONGPATH_CONFIG_DIR -- also
// voll isoliert, was die Dateien angeht. Im Protokoll der Sonde stand:
//
//   DxClusterClient: connecting to "dxc.nc7j.com":7300 callsign= "OE5SOS"
//   DxClusterClient: connecting to "telnet.reversebeacon.net":7000 callsign= "OE5SOS"
//   Rotor: Rotator connected -- rotctld ...
//
// Das ist der Vorfall vom 2026-09-17 ein zweites Mal: eine Sonde meldete
// sich mit dem Rufzeichen des Betreibers an und warf damit SEINE Sitzung
// aus dem Cluster. Isoliert waren die Dateien; das Netz war es nie.
//
// Dieser Lauf faehrt BEIDE Richtungen in einem Durchgang: mit der
// Umgebungsvariablen muessen die Dienste still bleiben, ohne sie muessen
// sie starten wie bisher. Die zweite Haelfte ist die Gegenprobe, und sie
// bleibt dauerhaft stehen -- eine Wache, die alles abschaltet, waere
// genauso falsch wie keine.
//
// Geprueft wird an den beiden Diensten, die sich OHNE Netzverkehr
// beobachten lassen: WSJT-X und SpotCollector binden je einen lokalen
// UDP-Port, und `isListening()` sagt es. Die anderen sechs (DX-Cluster,
// Reverse Beacon, POTA, SOTA, FreeDV Reporter, PSK Reporter) haengen an
// DEMSELBEN frueheren Ausgang in restoreSpotClientAutoStartState() --
// darum ist dieser eine Ausgang dort auch die ganze Wache und nicht acht
// einzelne Abfragen. Sie hier mitzustarten hiesse, in einem Prueflauf
// wirklich ins Netz zu gehen.

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QUdpSocket>

#include "core/AppSettings.h"
#include "core/SpotCollectorClient.h"
#include "core/StartWatch.h"
#include "core/WsjtxClient.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

/// Einen Port, den gerade niemand hat. Feste Nummern im Prueflauf
/// kollidieren irgendwann mit einem echten WSJT-X auf der Maschine --
/// und dann faellt der Lauf aus einem Grund um, der nichts mit der
/// Wache zu tun hat.
quint16 freierPort()
{
    QUdpSocket s;
    if (!s.bind(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 p = s.localPort();
    s.close();
    return p;
}

void stelleSchluesselEin(quint16 wsjtxPort, quint16 sammlerPort)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("WsjtxAutoStart"), QStringLiteral("True"));
    s.setValue(QStringLiteral("WsjtxAddress"), QStringLiteral("127.0.0.1"));
    s.setValue(QStringLiteral("WsjtxPort"), QString::number(wsjtxPort));
    s.setValue(QStringLiteral("SpotCollectorAutoStart"), QStringLiteral("True"));
    s.setValue(QStringLiteral("SpotCollectorPort"), QString::number(sammlerPort));
    // Und ausdruecklich NICHT die sechs, die wirklich ins Netz gehen.
    for (const QString& k : {QStringLiteral("DxClusterAutoConnect"),
                             QStringLiteral("RbnAutoConnect"),
                             QStringLiteral("PotaAutoStart"),
                             QStringLiteral("SotaAutoStart"),
                             QStringLiteral("FreeDvAutoStart"),
                             QStringLiteral("PskReporterAutoStart")}) {
        s.setValue(k, QStringLiteral("False"));
    }
}

} // namespace

class TstSondeBleibtOffline : public QObject { Q_OBJECT
private slots:

    // ── Der Pruefbetrieb selbst ist jetzt isoliert (2026-10-28) ──────
    //
    // Bis heute setzte NIEMAND LONGPATH_CONFIG_DIR im Pruefbetrieb:
    // nicht TestSandboxInit, nicht tools/run_tests.sh. In jedem
    // gewoehnlichen ctest-Lauf war darfVonSelbstVerbinden() also wahr
    // -- also genau der Zustand, gegen den dieser Pruefstand
    // geschrieben wurde, nur eine Stufe hoeher. Die Wache war da, der
    // Pruefbetrieb stand aber auf der falschen Seite davon.
    //
    // Seit TestSandboxInit die Variable setzt, laeuft jedes
    // Pruefprogramm isoliert. DAS ist die Zusicherung, und sie wird
    // hier festgenagelt -- ohne dass dieser Fall die Variable selbst
    // setzt, denn dann prueft er nur sich.
    //
    // In initTestCase(), weil cleanup() unten den Wert zurueckstellt
    // und ein spaeterer Fall ihn also schon beruehrt haben koennte.
    void initTestCase()
    {
        m_beimStart = qEnvironmentVariable("LONGPATH_CONFIG_DIR");
        QVERIFY2(!m_beimStart.trimmed().isEmpty(),
                 "Der Pruefbetrieb setzt LONGPATH_CONFIG_DIR nicht -- dann "
                 "darf jedes Pruefprogramm von selbst ins Netz. Siehe "
                 "tests/TestSandboxInit.cpp.");
        QVERIFY(AppSettings::isSandbox());
        QVERIFY(!AppSettings::darfVonSelbstVerbinden());
    }

    // Zurueck auf den Startwert, NICHT loeschen. Geloescht liefe jeder
    // Fall nach dem ersten un-isoliert -- und das ist das, was gerade
    // behoben wurde.
    void cleanup()
    {
        if (m_beimStart.isEmpty()) {
            qunsetenv("LONGPATH_CONFIG_DIR");
        } else {
            qputenv("LONGPATH_CONFIG_DIR", m_beimStart.toUtf8());
        }
    }

    // Der Schalter selbst: er haengt an der Umgebung, nicht an einer
    // gespeicherten Einstellung. Eine Sonde soll nichts einstellen
    // muessen, um still zu sein.
    void derSchalterFolgtDerUmgebung()
    {
        qunsetenv("LONGPATH_CONFIG_DIR");
        QVERIFY(!AppSettings::isSandbox());
        QVERIFY(AppSettings::darfVonSelbstVerbinden());

        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("LONGPATH_CONFIG_DIR", ordner.path().toUtf8());
        QVERIFY(AppSettings::isSandbox());
        QVERIFY(!AppSettings::darfVonSelbstVerbinden());
    }

    // Mit der Variablen: die Dienste bleiben still.
    void inDerSandboxBleibenDieDiensteStill()
    {
        const quint16 wsjtxPort  = freierPort();
        const quint16 sammlerPort = freierPort();
        QVERIFY(wsjtxPort != 0 && sammlerPort != 0 && wsjtxPort != sammlerPort);
        stelleSchluesselEin(wsjtxPort, sammlerPort);

        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("LONGPATH_CONFIG_DIR", ordner.path().toUtf8());

        RadioModel modell;
        modell.restoreSpotClientAutoStartState();

        QVERIFY(modell.wsjtx() != nullptr);
        QVERIFY(modell.spotCollector() != nullptr);
        QVERIFY2(!modell.wsjtx()->isListening(),
                 "die Sonde hat den WSJT-X-Port gebunden");
        QVERIFY2(!modell.spotCollector()->isListening(),
                 "die Sonde hat den SpotCollector-Port gebunden");

        // Und die Ports sind wirklich frei -- nicht nur die Flagge sagt
        // es. Ein Dienst, der bindet und seine Flagge nicht setzt,
        // kaeme hier durch.
        QUdpSocket probe;
        QVERIFY2(probe.bind(QHostAddress::LocalHost, wsjtxPort),
                 "der WSJT-X-Port ist belegt, obwohl niemand lauschen soll");
        probe.close();
    }

    // ── Die Gegenprobe, dauerhaft ────────────────────────────────────
    //
    // Ohne die Variable starten die Dienste wie bisher. Ohne diesen
    // Fall waere eine Wache, die IMMER zuschlaegt, genauso gruen -- und
    // dann waere der Betreiber beim naechsten Start ohne seine Dienste
    // da, ohne dass es jemand merkt.
    void ohneSandboxStartenSieWieBisher()
    {
        const quint16 wsjtxPort  = freierPort();
        const quint16 sammlerPort = freierPort();
        QVERIFY(wsjtxPort != 0 && sammlerPort != 0 && wsjtxPort != sammlerPort);
        stelleSchluesselEin(wsjtxPort, sammlerPort);

        qunsetenv("LONGPATH_CONFIG_DIR");
        QVERIFY(AppSettings::darfVonSelbstVerbinden());

        RadioModel modell;
        modell.restoreSpotClientAutoStartState();

        QVERIFY2(modell.wsjtx()->isListening(),
                 "WSJT-X lauscht nicht, obwohl der Autostart an ist");
        QVERIFY2(modell.spotCollector()->isListening(),
                 "SpotCollector lauscht nicht, obwohl der Autostart an ist");
    }

    // Der Startwaechter: eine Sonde zaehlt nicht mit. Gemessen am
    // 2026-10-21 -- zwei kurze Sonden hintereinander, und der dritte
    // Lauf fuhr OHNE die gespeicherte Anordnung hoch. Genau die
    // Anordnung war das, was gemessen werden sollte.
    void derStartwaechterZaehltDieSondeNichtMit()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("LONGPATH_CONFIG_DIR", ordner.path().toUtf8());

        // Drei Starts in Folge, keiner "kommt durch" (markRunning()
        // bleibt aus) -- in einem echten Lauf waere der dritte der
        // sichere Start.
        QCOMPARE(StartWatch::beginStart(), 0);
        QCOMPARE(StartWatch::beginStart(), 0);
        QCOMPARE(StartWatch::beginStart(), 0);
        QVERIFY2(!QFile::exists(StartWatch::markerPath()),
                 "die Sonde hat eine Marke hinterlassen");
        QVERIFY2(!StartWatch::shouldStartSafely(),
                 "die Sonde faehrt in den sicheren Start");

        // Auch eine Marke, die ein Lauf OHNE Sandbox im selben Ordner
        // liegengelassen hat, schickt die Sonde nicht dorthin.
        QFile marke(StartWatch::markerPath());
        QVERIFY(marke.open(QIODevice::WriteOnly | QIODevice::Text));
        marke.write("9\n");
        marke.close();
        QVERIFY2(!StartWatch::shouldStartSafely(),
                 "eine fremde Marke schickt die Sonde in den sicheren Start");

        // Gegenprobe: ohne die Variable zaehlt er wie bisher.
        qunsetenv("LONGPATH_CONFIG_DIR");
        StartWatch::reset();
        QCOMPARE(StartWatch::beginStart(), 0);
        QVERIFY2(QFile::exists(StartWatch::markerPath()),
                 "ohne Sandbox muss die Marke geschrieben werden");
        QCOMPARE(StartWatch::beginStart(), 1);
        QCOMPARE(StartWatch::beginStart(), 2);
        QVERIFY2(StartWatch::shouldStartSafely(),
                 "ohne Sandbox muss der sichere Start greifen");
        StartWatch::reset();
    }

private:
    /// Der Wert, mit dem der Pruefbetrieb gestartet ist.
    QString m_beimStart;
};

QTEST_MAIN(TstSondeBleibtOffline)
#include "tst_sonde_bleibt_offline.moc"
