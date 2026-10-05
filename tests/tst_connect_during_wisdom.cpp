// no-port-check: Longpath-original. Weder Thetis noch deskhpsdr kennen
// einen asynchronen Wisdom-Lauf mit nebenher laufender Bedienung — Thetis
// baut die FFTW-Plaene in radio.cs:140-158 [v2.10.3.13] synchron vor dem
// ersten Connect. Es gibt hier also nichts zu portieren.
// =================================================================
// tests/tst_connect_during_wisdom.cpp  (Longpath)
// =================================================================
//
// Befund 2026-10-05, frisches LONGPATH_CONFIG_DIR (Profil ohne
// wdspWisdom00), also jede Erstinstallation:
//
//   INF: Initializing WDSP wisdom on background thread ... needsGeneration= true
//   INF: Connecting with sampleRate= 48000 inSize= 64 activeRxCount= 1
//   INF: Sub-Epic I: streams= 2 channels= 2
//   WRN: WdspEngine wisdom generation already in progress; ignoring re-entrant initialize()
//
// Danach bleibt die Oberflaeche auf "Disconnected" stehen, ohne Meldung.
// Die Erzeugung lief auf einem MacBook Air rund 15 Minuten (FFT-Groessen
// bis 262144, ~60 % CPU) — ein Fenster, in dem JEDER Verbindungswunsch
// hineinfaellt.
//
// Mechanik der alten Fassung: connectToRadio() rief mittendrin
// WdspEngine::initialize() und wartete danach in einem GESCHACHTELTEN
// QEventLoop, bis initializedChanged(true) kam. Eine geschachtelte
// Ereignisschleife pumpt alles weiter — Zeitgeber, Discovery-Antworten,
// den Menueeintrag "Verbinden". Der zweite Wunsch lief deshalb nicht ins
// Leere, sondern lief die ganze Vorbereitung ein ZWEITES Mal durch und
// legte sich in eine ZWEITE geschachtelte Schleife. Beim Abwickeln nach
// den 15 Minuten lief dann der Rumpf zweimal: zwei RadioConnection-
// Objekte, zwei Sockets, das erste verwaist und nie abgebaut — und das
// Geraet sieht zwei metis-start.
//
// Der Prueffall faehrt genau diese Reihenfolge: Wunsch A, waehrend der
// Erzeugung Wunsch B, danach Ende der Erzeugung. Er prueft zwei Dinge,
// die in der alten Fassung beide fehlschlagen:
//
//   1. connectToRadio() blockiert nicht. Alt: die Funktion kehrt erst
//      nach dem Ende der Erzeugung zurueck (in echt: nach 15 Minuten).
//   2. Die Verbindungsfolge laeuft danach GENAU EINMAL, fuer den
//      zuletzt geaeusserten Wunsch. Alt: zweimal.
//
// Warum ein unbekanntes Protokoll: RadioConnection::create() liefert
// dafuer nullptr, und connectToRadio() steigt an dieser Stelle aus —
// nach der gesamten Vorbereitung, aber bevor ein Socket aufgeht. Damit
// ist der Rumpfdurchlauf zaehlbar (infoChanged), ohne dass der Prueffall
// Netz braucht.
//
// Warum eine Naht statt echter Wisdom: der echte Lauf kostet ~15 Minuten,
// die Suite deckelt jeden Test bei 120 s. beginWisdomForTests() /
// completeWisdomForTests() setzen genau den Zustand, den der Worker
// erzeugt, ohne FFTW anzufassen.
// =================================================================

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTimer>

#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Unbekanntes Protokoll -> RadioConnection::create() gibt nullptr, der
// Rumpf steigt vor dem ersten Socket aus. Alles davor laeuft echt.
constexpr auto kNoSuchProtocol = static_cast<ProtocolVersion>(99);

RadioInfo makeInfo(const QString& name, const QString& mac)
{
    RadioInfo info;
    info.name = name;
    info.macAddress = mac;
    info.address = QHostAddress(QStringLiteral("127.0.0.1"));
    info.port = 1024;
    info.protocol = kNoSuchProtocol;
    return info;
}

} // namespace

class TstConnectDuringWisdom : public QObject {
    Q_OBJECT

private slots:
    // Der Wunsch kommt WAEHREND der Erzeugung — nicht davor, nicht danach.
    void connectRequestDuringWisdomIsCarriedOutOnce()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);

        const RadioInfo radioA = makeInfo(QStringLiteral("Radio A"),
                                          QStringLiteral("AA:AA:AA:AA:AA:AA"));
        const RadioInfo radioB = makeInfo(QStringLiteral("Radio B"),
                                          QStringLiteral("BB:BB:BB:BB:BB:BB"));

        // infoChanged faellt oben im Rumpf von connectToRadio(), lange vor
        // RadioConnection::create() — ein Zaehler fuer Rumpfdurchlaeufe.
        QSignalSpy bodyRuns(&model, &RadioModel::infoChanged);

        // Frisches Profil: die Erzeugung laeuft.
        engine->beginWisdomForTests();

        // Der zweite Wunsch trifft mitten in der Erzeugung ein — in echt
        // die Discovery-Antwort des zuletzt verbundenen Geraets oder ein
        // zweiter Klick.
        QTimer::singleShot(100, &model, [&model, radioB]() {
            model.connectToRadio(radioB);
        });
        // Ende der Erzeugung.
        QTimer::singleShot(400, engine, [engine]() {
            engine->completeWisdomForTests();
        });

        QElapsedTimer t;
        t.start();
        model.connectToRadio(radioA);
        const qint64 blockedMs  = t.elapsed();
        const int runsAtReturn  = bodyRuns.count();

        QTRY_VERIFY_WITH_TIMEOUT(bodyRuns.count() > 0, 5000);
        QTest::qWait(300);   // einem zweiten Durchlauf Zeit geben, sich zu zeigen
        const int runsTotal     = bodyRuns.count();

        // Alle drei Messwerte zusammen ins Protokoll, damit ein roter Lauf
        // die ganze Mechanik zeigt und nicht nur die erste Zusicherung.
        qInfo("Messung: connectToRadio() blockierte %lld ms, "
              "Rumpfdurchlaeufe bei Rueckkehr %d, insgesamt %d, "
              "verbundener Name \"%s\"",
              blockedMs, runsAtReturn, runsTotal,
              qPrintable(model.name()));

        // ── 1. Der Aufruf darf nicht blockieren ────────────────────────
        QVERIFY2(blockedMs < 150,
                 qPrintable(QStringLiteral(
                     "connectToRadio() hielt den Hauptfaden %1 ms fest; in "
                     "echt waere das die volle Wisdom-Laufzeit (~15 min).")
                     .arg(blockedMs)));

        // Waehrend der Erzeugung darf noch nichts gelaufen sein.
        QCOMPARE(runsAtReturn, 0);

        // ── 2. Nach dem Ende der Erzeugung: genau ein Durchlauf ────────
        QCOMPARE(runsTotal, 1);
        // Der zuletzt geaeusserte Wunsch gewinnt.
        QCOMPARE(model.name(), radioB.displayName());
    }

    // Wer waehrend der Erzeugung auf Trennen drueckt, meint es ernst.
    // Ein geparkter Wunsch darf nicht Minuten spaeter doch noch kommen.
    void explicitDisconnectDropsTheParkedRequest()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);

        const RadioInfo radio = makeInfo(QStringLiteral("Radio D"),
                                         QStringLiteral("DD:DD:DD:DD:DD:DD"));
        QSignalSpy bodyRuns(&model, &RadioModel::infoChanged);

        engine->beginWisdomForTests();
        model.connectToRadio(radio);
        QVERIFY(model.isWaitingForWdspWisdom());

        model.disconnectFromRadio();
        QVERIFY(!model.isWaitingForWdspWisdom());
        QCOMPARE(model.connectionState(), ConnectionState::Disconnected);

        engine->completeWisdomForTests();
        QTest::qWait(300);
        QCOMPARE(bodyRuns.count(), 0);
    }

    // Gegenprobe: ohne laufende Erzeugung bleibt der Weg unveraendert —
    // der Wunsch wird sofort ausgefuehrt, nicht geparkt.
    void connectRequestAfterWisdomRunsImmediately()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);

        engine->beginWisdomForTests();
        engine->completeWisdomForTests();

        const RadioInfo radio = makeInfo(QStringLiteral("Radio C"),
                                         QStringLiteral("CC:CC:CC:CC:CC:CC"));
        QSignalSpy bodyRuns(&model, &RadioModel::infoChanged);
        model.connectToRadio(radio);

        QCOMPARE(bodyRuns.count(), 1);
        QCOMPARE(model.name(), radio.displayName());
    }
};

QTEST_MAIN(TstConnectDuringWisdom)
#include "tst_connect_during_wisdom.moc"
