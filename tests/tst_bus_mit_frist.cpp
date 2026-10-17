// Prüfstand: ein Audiogerät, das nicht aufmacht, darf das Programm nicht
// mitnehmen.
//
// Am 2026-10-03 fror der Verbindungsaufbau ein — nicht der Ton, das GANZE
// Programm samt „Abbrechen". `AudioEngine::start()` öffnet sieben Geräte
// nacheinander, jedes ohne Zeitlimit, und das alles in der verschachtelten
// Ereignisschleife von `RadioModel::connectToRadio`. Ein einziges hängendes
// `open()` genügt.
//
// Geprüft wird darum genau die Zusage, auf der die Behebung steht: nach der
// Frist geht es weiter, und niemand bleibt auf einem halb gebauten Gerät
// sitzen.

#include <QtTest>

#include <atomic>
#include <chrono>
#include <thread>

#include "core/audio/BusMitFrist.h"

using namespace Longpath;

namespace {

/// Zählt mit, wie viele dieser Busse noch leben — damit sich belegen lässt,
/// dass ein aufgegebener Bus wirklich aufgeräumt wird und nicht leckt.
std::atomic<int> g_lebende{0};

/// Ein Bus, dessen `open()` so lange steht, wie man ihm sagt.
class LahmerBus : public IAudioBus {
public:
    explicit LahmerBus(int oeffnenMs, bool erfolg = true)
        : m_oeffnenMs(oeffnenMs), m_erfolg(erfolg) { ++g_lebende; }
    ~LahmerBus() override { --g_lebende; }

    bool open(const AudioFormat&) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_oeffnenMs));
        m_offen = m_erfolg;
        return m_erfolg;
    }
    void close() override { m_offen = false; }
    bool isOpen() const override { return m_offen; }
    qint64 push(const char*, qint64 bytes) override { return bytes; }
    qint64 pull(char*, qint64) override { return 0; }
    float rxLevel() const override { return 0.0F; }
    float txLevel() const override { return 0.0F; }
    QString backendName() const override { return QStringLiteral("LahmerBus"); }
    AudioFormat negotiatedFormat() const override { return {}; }
    QString errorString() const override { return QStringLiteral("so gewollt"); }

private:
    int m_oeffnenMs;
    bool m_erfolg;
    bool m_offen = false;
};

}  // namespace

class TestBusMitFrist : public QObject {
    Q_OBJECT

private slots:

    void init() { QCOMPARE(g_lebende.load(), 0); }

    /// Der Normalfall: das Gerät macht schnell auf, der Aufrufer bekommt es.
    void schnellesGeraetKommtDurch()
    {
        auto bus = Audio::oeffneMitFrist(std::make_unique<LahmerBus>(0), {},
                                         QStringLiteral("schnell"), 2000);
        QVERIFY(bus);
        QVERIFY(bus->isOpen());
        bus.reset();
        QCOMPARE(g_lebende.load(), 0);
    }

    /// Das eigentliche Stück: ein Gerät, das NICHT aufmacht, hält den
    /// Aufrufer nur bis zur Frist auf — und nicht für immer.
    void haengendesGeraetGibtNachDerFristAuf()
    {
        QElapsedTimer uhr;
        uhr.start();
        auto bus = Audio::oeffneMitFrist(std::make_unique<LahmerBus>(4000), {},
                                         QStringLiteral("haengt"), 300);
        const qint64 gebraucht = uhr.elapsed();

        QVERIFY2(!bus, "Ein Bus, der die Frist riss, darf nicht herauskommen");
        // Grosszuegig nach oben: ein belasteter Laeufer ist langsam. Der
        // Punkt ist, dass NICHT die vollen 4000 ms gewartet wurden.
        QVERIFY2(gebraucht < 2500,
                 qPrintable(QStringLiteral("Es wurde %1 ms gewartet statt der "
                                           "Frist von 300 ms")
                                .arg(gebraucht)));
        QVERIFY2(gebraucht >= 250,
                 "Es wurde gar nicht erst gewartet — dann prueft das nichts");

        // Und der aufgegebene Bus muss weggeraeumt werden, sobald sein
        // open() zurueckkommt. Ohne diese Zusage wuerde jeder Zeitablauf
        // ein Geraet offen zuruecklassen.
        QTRY_VERIFY_WITH_TIMEOUT(g_lebende.load() == 0, 8000);
    }

    /// Sauberes Scheitern ist kein Zeitlimit: der Aufrufer bekommt nullptr,
    /// und der Bus ist sofort weg — nicht erst nach einer Frist.
    void sauberesScheiternBrauchtKeineFrist()
    {
        QElapsedTimer uhr;
        uhr.start();
        auto bus = Audio::oeffneMitFrist(
            std::make_unique<LahmerBus>(0, /*erfolg=*/false), {},
            QStringLiteral("scheitert"), 3000);
        QVERIFY(!bus);
        QVERIFY2(uhr.elapsed() < 1000,
                 "Ein sauber gescheitertes open() darf nicht auf die Frist "
                 "warten");
        QTRY_VERIFY_WITH_TIMEOUT(g_lebende.load() == 0, 3000);
    }

    /// Solange ein aufgegebener Versuch noch hängt, wird kein zweiter
    /// gestartet — und sobald er durch ist, geht es wieder.
    ///
    /// Das ist die Sicherung, die mir beim Nachlesen des eigenen Codes
    /// aufgefallen ist: `Pa_OpenStream` darf nicht von zwei Fäden gleichzeitig
    /// laufen. Ohne sie wäre genau das die Folge der Frist — der erste Versuch
    /// hängt noch, der Hauptfaden geht weiter zum nächsten Gerät und öffnet
    /// parallel. Die Frist hätte den Hänger gegen etwas Schlimmeres
    /// eingetauscht.
    void waehrendEinerHaengtWirdKeinZweiterGestartet()
    {
        // Erster: hängt 1,5 s, Frist 200 ms -> wird aufgegeben.
        auto a = Audio::oeffneMitFrist(std::make_unique<LahmerBus>(1500), {},
                                       QStringLiteral("erster"), 200);
        QVERIFY(!a);

        // Zweiter: würde in 0 ms aufmachen — darf aber gar nicht erst
        // starten, solange der erste hängt. Und zwar SOFORT abgelehnt, nicht
        // erst nach einer weiteren Frist.
        QElapsedTimer uhr;
        uhr.start();
        auto b = Audio::oeffneMitFrist(std::make_unique<LahmerBus>(0), {},
                                       QStringLiteral("zweiter"), 3000);
        const qint64 gebraucht = uhr.elapsed();
        QVERIFY2(!b, "Ein zweiter Versuch lief, waehrend der erste noch haengt");
        QVERIFY2(gebraucht < 150,
                 qPrintable(QStringLiteral("Die Ablehnung dauerte %1 ms — sie "
                                           "soll sofort kommen")
                                .arg(gebraucht)));

        // Sobald der erste durch ist, muss es wieder gehen. Ohne diese
        // Haelfte waere eine Sperre denkbar, die nie wieder aufgeht.
        QTRY_VERIFY_WITH_TIMEOUT(g_lebende.load() == 0, 8000);
        auto c = Audio::oeffneMitFrist(std::make_unique<LahmerBus>(0), {},
                                       QStringLiteral("danach"), 2000);
        QVERIFY2(c, "Nach dem Ende des haengenden Versuchs blieb die Sperre zu");
        c.reset();
    }

    /// Kein Bus hinein, kein Absturz heraus.
    void ohneBusKeinAbsturz()
    {
        QVERIFY(!Audio::oeffneMitFrist(nullptr, {}, QStringLiteral("nichts")));
    }
};

QTEST_MAIN(TestBusMitFrist)
#include "tst_bus_mit_frist.moc"
