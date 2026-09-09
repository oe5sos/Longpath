// no-port-check: quiet-poll deadline safety net, added 2026-09-09 while
// investigating a CI-only (Linux, GitHub Actions) 120s GUI-test timeout in
// RadioDiscovery's NIC-walk (docs/architecture/2026-09-09-ci-discovery-hang-
// investigation.md). NereusSDR-original defensive addition, not itself a
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

        QElapsedTimer clock;
        clock.start();
        const auto outcome =
            disc.quietPollAttemptForTest(victim, kQuietBeforeStop, kPollMs, deadline);
        const qint64 elapsedMs = clock.elapsed();

        stopFlood.store(true);
        flooder.join();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::DeadlineExceeded));
        // Generous CI slack — the point is "bounded", not "exactly on time".
        // The pre-fix behaviour was unbounded (effectively forever, i.e. up
        // to CTest's 120s TIMEOUT), so even a loose multiple of the intended
        // bound is a meaningful assertion.
        QVERIFY2(elapsedMs < 5 * (kQuietBeforeStop + 1) * kPollMs,
                 qPrintable(QStringLiteral(
                     "flooded quiet-poll loop took %1 ms — the deadline "
                     "safety net did not bound it").arg(elapsedMs)));
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

        QElapsedTimer clock;
        clock.start();
        const auto outcome =
            disc.quietPollAttemptForTest(victim, kQuietBeforeStop, kPollMs, deadline);
        const qint64 elapsedMs = clock.elapsed();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::Quiet));
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

        QElapsedTimer clock;
        clock.start();
        const auto outcome =
            disc.quietPollAttemptForTest(victim, kQuietBeforeStop, kPollMs, deadline);
        const qint64 elapsedMs = clock.elapsed();

        stopFlood.store(true);
        flooder.join();

        QCOMPARE(int(outcome), int(RadioDiscovery::QuietPollOutcome::Cancelled));
        QVERIFY2(elapsedMs < kPollMs * 5,
                 qPrintable(QStringLiteral(
                     "cancel took %1 ms — should exit on the very first poll")
                     .arg(elapsedMs)));
    }
};

QTEST_MAIN(TstRadioDiscoveryScanBound)
#include "tst_radio_discovery_scan_bound.moc"
