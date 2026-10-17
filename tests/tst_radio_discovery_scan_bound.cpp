// no-port-check: quiet-poll deadline safety net, added 2026-09-09 while
// investigating a CI-only (Linux, GitHub Actions) 120s GUI-test timeout in
// RadioDiscovery's NIC-walk (docs/architecture/2026-09-09-ci-discovery-hang-
// investigation.md). Longpath-original defensive addition, not itself a
// port: Thetis's clsRadioDiscovery.cs (its own quiet-poll while loop,
// `Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs:964-976`) has no
// deadline either and is likewise vulnerable to a sustained-readable
// condition holding its `quietPolls` counter from ever reaching the
// give-up threshold — a continuous flood is unbounded there too, so this
// test's premise (nothing before this closed that class of hang) holds for
// the upstream logic as well, not only the C++ port. This file only adds
// the missing backstop; it does not change quiet-counter semantics.
//
// What this guards: quietPollAttempt() (RadioDiscovery.cpp) used to have no
// wall-clock bound at all — its only exit was "quietBeforeStop consecutive
// not-readable polls", and ANY sustained "readable" condition (whatever
// causes it) resets that counter to 0 forever, hanging the loop
// indefinitely. Flooding a real loopback socket faster than pollMs
// reproduces "always readable" directly and deterministically, without
// needing the exact Linux-only trigger the CI log pointed at (see the
// investigation doc) — it proves the fix (a QDeadlineTimer safety net)
// actually bounds the loop instead of relying on quietPolls ever advancing.
#include <QtTest/QtTest>
#include <QUdpSocket>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QDeadlineTimer>

#include <atomic>
#include <chrono>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/RadioDiscovery.h"

using namespace Longpath;

namespace {

// Sends a 1-byte UDP datagram to 127.0.0.1:`port` roughly every 500us until
// `stop` flips true. Runs on its own std::thread with a plain POSIX socket:
// QUdpSocket::waitForReadyRead() does not pump this thread's own Qt event
// loop, so a QTimer-based flooder on the SAME thread as the code under test
// would never fire while that code is asleep inside waitForReadyRead().
void floodLoopbackPort(quint16 port, std::atomic<bool>* stop)
{
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const char payload = 0;
    while (!stop->load(std::memory_order_relaxed)) {
        ::sendto(fd, &payload, sizeof(payload), 0,
                 reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    ::close(fd);
}

// Sends exactly one 1-byte UDP datagram to 127.0.0.1:`port` after sleeping
// `delayMs`. Runs on its own std::thread for the same reason
// floodLoopbackPort() does — the code under test blocks the calling thread
// inside waitForReadyRead(), so a same-thread timer would never fire.
void sendSingleDatagramAfterDelay(quint16 port, int delayMs)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const char payload = 0;
    ::sendto(fd, &payload, sizeof(payload), 0,
             reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
}

} // namespace

class TstRadioDiscoveryScanBound : public QObject {
    Q_OBJECT
private slots:
    // The post-disconnect quiet deadline is process-wide; a leftover arm
    // from another test function must not affect these.
    void init() { RadioDiscovery::clearHoldOffForTest(); }

    // Core regression: a socket that is readable on every single poll (here,
    // via a real flood — a stray reply from an unrelated host on a shared CI
    // subnet, a spurious wakeup, or a duplicate NIC entry would all look the
    // same to this loop) must still terminate within the documented time
    // bound instead of hanging until an external killer (CTest's 120s
    // TIMEOUT) intervenes.
    void floodedSocketStillTerminatesViaDeadline() {
        RadioDiscovery disc;
        QUdpSocket victim;
        QVERIFY(victim.bind(QHostAddress::LocalHost, 0));

        std::atomic<bool> stopFlood{false};
        std::thread flooder(floodLoopbackPort, victim.localPort(), &stopFlood);

        constexpr int kQuietBeforeStop = 4;
        constexpr int kPollMs = 30;
        const QDeadlineTimer deadline(qint64(kQuietBeforeStop + 1) * kPollMs, Qt::CoarseTimer);

        RadioDiscovery::QuietPollMetrics metrics;
        QElapsedTimer clock;
        clock.start();
        const auto outcome = disc.quietPollAttemptForTest(
            victim, kQuietBeforeStop, kPollMs, deadline, &metrics);
        const qint64 elapsedMs = clock.elapsed();

        stopFlood.store(true);
        flooder.join();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::DeadlineExceeded));
        // Hier bleibt die ZEIT der Gegenstand — die Frist ist das
        // Sicherheitsnetz, das geprueft wird, und eine Zahl kann sie
        // nicht ersetzen. Die Zaehler sagen dafuer, dass die Flut
        // wirklich ankam und der Zaehler unter ihr nicht durchlief;
        // ohne sie koennte der Lauf auch an einem stillen Socket in die
        // Frist laufen und gruen aussehen.
        QVERIFY2(metrics.datagrams >= 1,
                 qPrintable(QStringLiteral(
                     "die Flut kam nicht an (%1 Datagramme) — dieser Lauf hat "
                     "die Frist dann an einem stillen Socket geprueft")
                     .arg(metrics.datagrams)));
        QVERIFY2(metrics.quietPolls < kQuietBeforeStop,
                 qPrintable(QStringLiteral(
                     "unter der Flut liefen %1 stille Fenster durch — dann war "
                     "es nicht die Frist, die die Schleife beendet hat")
                     .arg(metrics.quietPolls)));
        // Generous CI slack — the point is "bounded", not "exactly on time".
        // The pre-fix behaviour was unbounded (effectively forever, i.e. up
        // to CTest's 120s TIMEOUT), so even a loose multiple of the intended
        // bound is a meaningful assertion.
        QVERIFY2(elapsedMs < 5 * (kQuietBeforeStop + 1) * kPollMs,
                 qPrintable(QStringLiteral(
                     "flooded quiet-poll loop took %1 ms — the deadline "
                     "safety net did not bound it").arg(elapsedMs)));
    }

    // Regression for the 2026-09-09 quietPolls-reset fix: a single bursty
    // reply must leave the quiet counter where it was, not reset it to 0
    // (From Thetis clsRadioDiscovery.cs:964-976 [@852bf0e] — only the
    // not-readable branch touches quietPolls there). Sends exactly one
    // datagram after kBurstDelayMs, i.e. after roughly
    // kBurstDelayMs/kPollMs quiet polls have already elapsed. Fixed
    // behaviour is invariant to when in the loop that single reply
    // arrives: total time to Quiet stays close to kQuietBeforeStop *
    // kPollMs, same as if no reply had come at all. The pre-fix
    // reset-to-0 behaviour instead adds a full extra kQuietBeforeStop
    // polls on top of however many had already elapsed before the burst.
    //
    // ── Gezaehlt, nicht gemessen (2026-10-17) ───────────────────────
    //
    // Hier stand eine Zeitlatte: 180 ms fuer die feste Fassung, 270 ms
    // fuer den Rueckfall, Schwelle bei 8 * kPollMs = 240 ms dazwischen.
    // Am 2026-10-17 brauchte ein ausgelasteter macOS-Laeufer 257 ms fuer
    // die RICHTIGE Fassung (main-Lauf zu d7d57af7, „Build (macOS Apple
    // Silicon (2/2))"), der Lauf fiel also um, obwohl das Programm
    // stimmte; der Neulauf desselben Jobs war gruen. Zwischen „gut" und
    // „schlecht" lagen 90 ms, und die Schwelle liess 60 ms Luft fuer
    // Taktschwankungen — zu wenig.
    //
    // Geprueft wird jetzt die Groesse, auf die es ankommt: die ZAHL der
    // stillen Fenster. Sie ist bei der festen Fassung genau
    // quietBeforeStop, egal wann die Antwort kommt und egal wie schnell
    // der Rechner ist; der Rueckfall braucht kBurstDelayMs/kPollMs
    // mehr. Dieselbe Wette ist dieser Pruefstand an vier anderen
    // Stellen gerade losgeworden (#250, #246, #248, #252) und bei
    // tst_ft8_senden noch einmal (#260).
    void aBurstyReplyDoesNotResetTheQuietCounter() {
        RadioDiscovery disc;
        QUdpSocket victim;
        QVERIFY(victim.bind(QHostAddress::LocalHost, 0));

        constexpr int kQuietBeforeStop = 6;
        constexpr int kPollMs = 30;
        constexpr int kBurstDelayMs = 3 * kPollMs;
        const QDeadlineTimer deadline(qint64(kQuietBeforeStop + 4) * kPollMs, Qt::CoarseTimer);

        std::thread burst(sendSingleDatagramAfterDelay, victim.localPort(), kBurstDelayMs);

        RadioDiscovery::QuietPollMetrics metrics;
        QElapsedTimer clock;
        clock.start();
        const auto outcome = disc.quietPollAttemptForTest(
            victim, kQuietBeforeStop, kPollMs, deadline, &metrics);
        const qint64 elapsedMs = clock.elapsed();

        burst.join();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::Quiet));

        // Erst: die Antwort ist wirklich angekommen. Ohne diese Zeile
        // wuerde ein verlorenes Datagramm den Lauf gruen machen, ohne
        // dass die Sperrklinke je geprueft worden waere.
        QVERIFY2(metrics.datagrams >= 1,
                 qPrintable(QStringLiteral(
                     "die eine Antwort kam nicht an (%1 Datagramme) — dieser "
                     "Lauf hat den Stille-Zaehler dann gar nicht geprueft")
                     .arg(metrics.datagrams)));

        // Und dann die Sperrklinke selbst, gezaehlt statt gemessen: die
        // feste Fassung braucht GENAU quietBeforeStop stille Fenster,
        // egal wann die Antwort kommt und egal wie schnell der Rechner
        // ist. Der Rueckfall (Zaehler bei einer Antwort auf 0) braucht
        // die vor der Antwort verbrauchten Fenster noch einmal, hier
        // also kBurstDelayMs/kPollMs mehr.
        QCOMPARE(metrics.quietPolls, kQuietBeforeStop);

        // Die Zeit bleibt als Schranke stehen, nicht als Messlatte: sie
        // faengt nur noch den Haenger, nicht mehr den Rueckfall. Die
        // Grenze liegt darum weit weg (dieselbe Grosszuegigkeit wie in
        // floodedSocketStillTerminatesViaDeadline) — am 2026-10-17
        // brauchte ein ausgelasteter macOS-Laeufer 257 ms fuer die
        // RICHTIGE Fassung, wo die alte Latte bei 240 ms lag.
        QVERIFY2(elapsedMs < 5 * (kQuietBeforeStop + 4) * kPollMs,
                 qPrintable(QStringLiteral(
                     "bursty-reply quiet-poll loop took %1 ms — die Schleife "
                     "war nicht mehr begrenzt (%2 stille Fenster)")
                     .arg(elapsedMs).arg(metrics.quietPolls)));
    }

    // No-regression check: with nothing flooding it, the loop must still
    // terminate the ordinary way (Quiet, via the counter reaching
    // quietBeforeStop) — the deadline must not cut the normal no-reply case
    // short.
    void quietSocketReturnsQuietWithinBudget() {
        RadioDiscovery disc;
        QUdpSocket victim;
        QVERIFY(victim.bind(QHostAddress::LocalHost, 0));

        constexpr int kQuietBeforeStop = 4;
        constexpr int kPollMs = 30;
        const QDeadlineTimer deadline(qint64(kQuietBeforeStop + 1) * kPollMs, Qt::CoarseTimer);

        RadioDiscovery::QuietPollMetrics metrics;
        QElapsedTimer clock;
        clock.start();
        const auto outcome = disc.quietPollAttemptForTest(
            victim, kQuietBeforeStop, kPollMs, deadline, &metrics);
        const qint64 elapsedMs = clock.elapsed();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::Quiet));
        // Der gewoehnliche Weg, gezaehlt: genau quietBeforeStop stille
        // Fenster, kein Datagramm. Das ist zugleich die Grundlinie, gegen
        // die der Fall mit der einen Antwort oben zu lesen ist.
        QCOMPARE(metrics.quietPolls, kQuietBeforeStop);
        QCOMPARE(metrics.datagrams, 0);
        QVERIFY2(elapsedMs < 5 * kQuietBeforeStop * kPollMs,
                 qPrintable(QStringLiteral(
                     "quiet loop with no traffic took %1 ms").arg(elapsedMs)));
    }

    // The cooperative-cancel path scanAllNics() has always had (stopDiscovery()
    // flips m_stopRequested) must keep working unchanged through this
    // refactor, even mid-flood.
    void stopDiscoveryCancelsEvenDuringFlood() {
        RadioDiscovery disc;
        QUdpSocket victim;
        QVERIFY(victim.bind(QHostAddress::LocalHost, 0));

        std::atomic<bool> stopFlood{false};
        std::thread flooder(floodLoopbackPort, victim.localPort(), &stopFlood);

        disc.stopDiscovery();

        constexpr int kQuietBeforeStop = 4;
        constexpr int kPollMs = 30;
        const QDeadlineTimer deadline(qint64(kQuietBeforeStop + 1) * kPollMs, Qt::CoarseTimer);

        RadioDiscovery::QuietPollMetrics metrics;
        QElapsedTimer clock;
        clock.start();
        const auto outcome = disc.quietPollAttemptForTest(
            victim, kQuietBeforeStop, kPollMs, deadline, &metrics);
        const qint64 elapsedMs = clock.elapsed();

        stopFlood.store(true);
        flooder.join();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::Cancelled));
        // „Steigt beim allerersten Poll aus" — und das ist eine Zahl,
        // keine Zeitspanne: die Abbruchpruefung steht VOR dem ersten
        // Fenster, es darf also kein stilles Fenster verbraucht worden
        // sein. Vorher stand hier `elapsedMs < kPollMs * 5` (150 ms),
        // was dasselbe sagen sollte, aber von der Auslastung des
        // Laeufers abhing.
        QCOMPARE(metrics.quietPolls, 0);
        QVERIFY2(elapsedMs < kPollMs * 20,
                 qPrintable(QStringLiteral(
                     "cancel took %1 ms — der Abbruch war nicht begrenzt")
                     .arg(elapsedMs)));
    }
};

QTEST_MAIN(TstRadioDiscoveryScanBound)
#include "tst_radio_discovery_scan_bound.moc"
