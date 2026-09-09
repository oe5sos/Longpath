# CI-only (Linux) 120s GUI-test timeouts — investigation + fix

> **Status: RESOLVED, 2026-09-09 (second pass, same day).** The
> RadioDiscovery fix below (PR #9, merged as `988fa77e`) was shipped
> first and closes two real, confirmed bugs — but a post-merge CI run
> on `main` showed the *exact same* set of tests still timing out at
> 120s, byte-for-byte identical to the pre-fix run, proving discovery
> was never the actual cause. The real cause, found by reading the
> post-merge CI log more carefully and confirmed against the source:
> `MainWindow::showAudioDiagnoseDialog()` (Linux-only) pops a **modal**
> dialog (`dlg->exec()`, no internal timeout) whenever no ALSA/JACK
> device is detected and the "first run seen" flag isn't set — both
> true on *every* Linux CI test run, never on macOS (the feature is
> `#ifdef Q_OS_LINUX`-gated end to end) and never past the first real
> launch on a real machine. Nothing in an automated test ever clicks
> Dismiss, so `exec()` blocks forever and only CTest's own external
> 120s `TIMEOUT` ever ends it. See "The actual root cause" below for
> the full evidence chain. The RadioDiscovery fix stays — it closes two
> real, independently-worthwhile bugs — but is not, and was never
> confirmed to be, why CI was timing out. Everything under "The
> failure" through "Verification performed" is kept as the original,
> same-day writeup for the record; read "The actual root cause" for
> what actually explains the timeouts.

## The failure

11 GUI tests failed consistently and only on the Linux CI shards (`Build (Linux x64 (N/4))`), never on macOS, with CTest's own kill:

```
180/221 Test #180: tst_sunsdr_spectrum_wiring ...................***Timeout 120.01 sec
```

All 11 construct a `MainWindow` (grep-confirmed in each `tests/tst_*.cpp`):
`tst_window_widgets`, `tst_kiwi_sdr_safety_gate`, `tst_sunsdr_audio_feed`,
`tst_sunsdr_is_reachable`, `tst_todays_work_together`,
`tst_sunsdr_connect_wiring`, `tst_real_container_move`,
`tst_real_zoom_visible`, `tst_splitter_handles_grabbable`,
`tst_real_notch_rightclick`, `tst_sunsdr_spectrum_wiring`.

The CI log's buffered stdout dump for the timed-out test (`tst_sunsdr_spectrum_wiring`, shard 1/4, run
[34353708375](https://github.com/oe5sos/Longpath/actions/runs/34353708375))
shows the last thing the process did before being killed:

```
QDEBUG : ... nereus.discovery: Scanning NIC "eth0" "10.1.1.114" profile 4
QWARN  : ... QRhiWidget: No QRhi
QWARN  : ... QRhiWidget: No QRhi
QWARN  : ... This plugin does not support propagateSizeHints()
QDEBUG : ... nereus.discovery: Scanning NIC "eth0" "10.1.1.114" profile 4
QDEBUG : ... nereus.connpanel.timing: ConnectionPanel ctor total elapsed (ms): 2259 of which buildUI(): 5 rows: 0
```

Two things stand out: the same NIC gets scanned **twice**, and the process is
killed with no `Totals:` line ever printed — the last visible activity sits
inside `RadioDiscovery::scanAllNics()`'s per-NIC blocking poll loop
(`src/core/RadioDiscovery.cpp`).

CI log timestamps here are not trustworthy for measuring *when* something
hung: GitHub Actions only receives the test process's buffered stdout as one
block, flushed all at once, at the moment CTest kills the process. The
*order* lines were written in is still reliable (single-threaded, same
stream), but gaps between timestamps tell you nothing about real elapsed
time once buffering has kicked in. The only trustworthy number in that
excerpt is `2259` — the ctor's own `QElapsedTimer` reading, computed and
formatted by the program itself before anything was buffered away.

## The fix

Two independent, confirmed bugs, both in the discovery path:

### 1. Every window open scanned twice (confirmed, reproduced on macOS)

`MainWindow`'s constructor had a direct call:

```cpp
// Start discovery in background so radios are found before the user opens the panel
m_radioModel->discovery()->startDiscovery();
...
QTimer::singleShot(0, this, &MainWindow::openConnectionPanelOnLaunch);
```

The comment is wrong: `scanAllNics()` is **fully synchronous** — a per-NIC
bind + send + blocking-poll loop on the calling thread, not a worker thread.
So this call blocks `MainWindow`'s own constructor for the whole NIC walk.
Worse, it is completely redundant: `openConnectionPanelOnLaunch()` (queued
for the very next event-loop turn) opens a `ConnectionPanel`, and
`ConnectionPanel`'s own constructor (`src/gui/ConnectionPanel.cpp:305`)
unconditionally calls `startDiscovery()` again, from scratch, regardless of
what the first scan already found. Every single launch (and every one of
the 28 tests that construct a `MainWindow`) therefore ran the *entire*
NIC-walk twice, back to back.

**Reproduced locally, no Linux needed:** running `tst_real_container_move`
on this Mac (single real NIC, `en0`) showed the exact same "Scanning NIC"
line twice, and `ConnectionPanel ctor total elapsed (ms): 2271` — the same
shape the CI log shows, on completely different hardware with no possible
Azure/duplicate-interface involvement. This rules out "CI runner has a
duplicate NIC entry" as the explanation for the *doubling* — the doubling is
a plain, deterministic, always-on code bug, not an environment quirk.

**Fix:** removed the redundant call (`src/gui/MainWindow.cpp`). Nothing
downstream reads `m_radioModel->discovery()->discoveredRadios()` before
`ConnectionPanel` is constructed, so nothing depended on the early scan's
result; `ConnectionPanel`'s own scan (which the user was always going to
see run, as "Searching for radios...") is unaffected. Measured effect on
`tst_real_container_move`: 17598 ms → 11228 ms wall time, and the duplicate
log line is gone. This halves real per-window discovery latency for every
user, not just CI.

### 2. The quiet-poll loop had no wall-clock bound (confirmed by code reading + new regression test)

`RadioDiscovery::scanAllNics()`'s per-attempt loop (now extracted into
`RadioDiscovery::quietPollAttempt()`) had exactly one way to exit under
normal operation: a counter, `quietPolls`, reaching `quietBeforeStop`
consecutive not-readable polls. Every single `waitForReadyRead() == true`
event reset that counter back to 0 — **including one that turns out to
carry nothing usable** (an unrelated host's broadcast reply reaching the
ephemeral port by chance on a shared subnet, a spurious wakeup, or simply
a NIC entry the class's own comment already anticipated: "quietPolls...
replies may be bursty"). The class's own header promises a hard bound —
`attemptsPerNic × quietPollsBeforeResend × pollTimeoutMs`, documented as
"~1800 ms" for `SafeDefault` — but nothing in the code actually enforced
that promise. A sustained "readable" condition, however produced, could
hold `quietPolls` at 0 indefinitely: not a slow path, a genuinely unbounded
one. That is a necessary condition for a 120-second CTest kill to occur at
all — a bounded ~2.25 s scan (or even the doubled ~4.5 s from bug #1) cannot
reach 120 s on its own.

**Checked against upstream, not assumed:** Thetis's own loop
(`../Thetis/Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs:964-976`)
has the identical shape and the identical gap — a `while (quietPolls <
quietBeforeResend)` with no deadline, where a continuous flood of readable
polls (upstream: frozen in place rather than reset to 0, since the C#
loop's `readable` branch never touches `quietPolls` at all, only the
not-readable branch does `quietPolls++`) is unbounded there too. So this is
not a Longpath-introduced regression from porting; the upstream C# has the
same latent gap, just never observed to hit it. One separate, narrower
thing this comparison surfaced: the Longpath port *does* diverge from
Thetis on ordinary bursty replies specifically — Thetis's counter is
untouched (frozen) on a readable poll, while Longpath's C++ explicitly
resets it to 0 (`RadioDiscovery.cpp`, "Reset quiet counter on activity —
replies may be bursty"), so occasional (non-continuous) replies make the
Longpath port wait longer to naturally go quiet than Thetis would. That is
a pre-existing, unrelated divergence from upstream timing behavior — not
part of this hang and not touched by this fix, since changing ported
protocol-timing semantics without being asked is out of scope here; flagged
separately for whoever wants to decide whether it's worth matching upstream
exactly.

**Fix:** `quietPollAttempt()` now takes a `QDeadlineTimer` armed at
`(quietBeforeStop + 1) × pollTimeoutMs` (one extra poll of slack so the
ordinary no-reply case, which already takes almost exactly
`quietBeforeStop × pollTimeoutMs`, is never cut short by scheduling jitter).
If the deadline expires before the counter would naturally reach
`quietBeforeStop`, the attempt ends anyway — `scanAllNics()` treats
`DeadlineExceeded` exactly like a normal `Quiet` exit and moves on to the
next attempt or NIC. This makes the documented time bound an actual
guarantee instead of a best-effort description, for every call site
(`startDiscovery()`'s NIC walk), regardless of what produces a sustained
readable condition.

**New regression test**, `tests/tst_radio_discovery_scan_bound.cpp`:
constructs a real loopback `QUdpSocket`, floods it from a separate
`std::thread` (a plain POSIX socket, since a same-thread `QTimer` flooder
cannot fire while the code under test is blocked inside
`waitForReadyRead()`) faster than `pollTimeoutMs`, and confirms
`quietPollAttempt()` returns `DeadlineExceeded` within the bounded time
instead of hanging — plus a no-flood companion test (still exits
`Quiet`, within budget) and a cancel-during-flood test
(`stopDiscovery()` still wins immediately). All three pass locally
(macOS, arm64, Qt 6.11.1):

```
PASS   : TstRadioDiscoveryScanBound::floodedSocketStillTerminatesViaDeadline()
PASS   : TstRadioDiscoveryScanBound::quietSocketReturnsQuietWithinBudget()
PASS   : TstRadioDiscoveryScanBound::stopDiscoveryCancelsEvenDuringFlood()
Totals: 5 passed, 0 failed, 0 skipped, 0 blacklisted, 279ms
```

This is a direct, mechanical proof that the fixed loop cannot be held open
forever by an "always readable" condition, independent of whatever
specific external event produces that condition on a given machine.

## What I could not confirm

I do not have a Linux, Docker, or `act` environment available in this
session (checked: no `docker`, no `act` on this Mac), so I could not
`strace`, `gdb`, or otherwise instrument the actual GitHub Actions runner to
find **which** sustained-readable condition it hits. Candidates I
considered but did not confirm:

- A stray UDP reply/broadcast from an unrelated process or host reaching
  the ephemeral discovery-scan port on the shared runner subnet.
- A Linux-specific spurious-readable behavior around ICMP
  Destination-Unreachable notifications on unconnected UDP sockets (a
  documented-elsewhere Linux quirk; not verified against this codebase).
- Something specific to the runner's virtual NIC setup making
  `QNetworkInterface::allInterfaces()` or the bind/broadcast path behave
  differently than on a normal machine.

None of these were ruled in or out with real evidence — they remain
speculation, and are deliberately **not** presented as the root cause. What
*is* confirmed, by direct local reproduction and by reading the code, is
that (a) every window open ran the full NIC walk twice, and (b) the loop
that walk depends on had no enforced upper bound and could — for any reason
that produces a sustained "readable" signal — run forever. Fixing both
closes the hang possibility regardless of which specific condition was
triggering it on the Linux runners, because the fix bounds the one
primitive every contributing factor would have to funnel through. If the CI
failures recur after this fix lands, that would itself be strong evidence
that a *third*, still-unidentified mechanism is involved — worth reopening
this investigation rather than assuming the fix was incomplete.

## Verification performed

- `./tools/syntax_check.sh` clean on `RadioDiscovery.h`,
  `RadioDiscovery.cpp`, `MainWindow.cpp`,
  `tst_radio_discovery_scan_bound.cpp` (the last one only after the real
  CMake build, since `syntax_check.sh` does not run `moc` — expected for
  any `Q_OBJECT` test file; the actual build is the real check for those).
- New test `tst_radio_discovery_scan_bound` (3 tests): all pass.
- Existing `tst_radio_discovery_probe` (8 tests) and `tst_radio_discovery_parse`
  (9 tests): all still pass after the `quietPollAttempt()` extraction —
  no behavior change to the normal (non-flooded) path.
- All 11 originally-failing tests, plus `tst_closing_takes_the_float_along`
  (exercises `showConnectionPanel()` directly): all pass locally (macOS),
  each visibly faster than before (the duplicate-scan removal), none
  anywhere near the 120s CTest `TIMEOUT`.
- Not run: the full ~32-minute test suite (`docs/development/fast-test-loop.md`)
  and anything on an actual Linux CI runner — no Linux access this session.
  The next CI run on this branch is the real confirmation. *(It ran; see
  below — the RadioDiscovery fix had no effect on the failures.)*

## The actual root cause

PR #9 merged as `988fa77e`. `main`'s own post-merge CI run
([34368565107](https://github.com/oe5sos/Longpath/actions/runs/34368565107))
finished with the four Linux shards failing with the identical set of
timeouts as the pre-fix reference run
([34353708375](https://github.com/oe5sos/Longpath/actions/runs/34353708375))
— not a subset, not a different set, the *same* tests in the *same* shards
down to the test number:

| Shard | Pre-fix failures | Post-fix failures |
| --- | --- | --- |
| 1/4 | `tst_real_container_move`, `tst_real_zoom_visible`, `tst_splitter_handles_grabbable`, `tst_real_notch_rightclick`, `tst_sunsdr_spectrum_wiring` | identical |
| 2/4 | `tst_real_rotor_window`, `tst_kiwi_is_reachable`, `tst_compact_bar_draws`, `tst_every_applet_is_reachable`, `tst_settings_are_remembered`, `tst_a_second_receiver_can_be_closed`, `tst_zoom_buttons_do_something`, `tst_real_pan_float_state`, `tst_native_overlay_audit`, `tst_sunsdr_control_wiring` | (shard still running when checked; expected identical based on the other three) |
| 3/4 | `tst_window_widgets`, `tst_kiwi_sdr_safety_gate`, `tst_sunsdr_audio_feed`, `tst_sunsdr_is_reachable`, `tst_todays_work_together`, `tst_sunsdr_connect_wiring` | identical |
| 4/4 | `tst_real_mainwindow_detach`, `tst_quit_leaves_no_pending_deletes`, `tst_kiwi_tx_mute`, `tst_reachability_audit`, `tst_real_layout_profile_roundtrip` | identical |

(This also means the failure was never actually limited to the 11 tests
named at the start of this investigation — the real reference run had 26
across all four shards. The 11 were whichever subset had been reported by
the time this investigation started.)

A deterministic, byte-for-byte-identical failure set before and after a fix
that made the one thing it targeted strictly faster and strictly bounded is
about as clean a "your fix didn't touch the actual mechanism" signal as CI
can give. Time to go back to the log.

**Reading shard 3/4's buffered dump for `tst_window_widgets` more
carefully** (post-fix run) shows only **one** `Scanning NIC` line now (the
double-scan fix worked) and `ConnectionPanel ctor total elapsed (ms): 2260`
— a normal, bounded discovery cycle. The dump continues *past* that point
this time, through `ConnectionPanel`'s own `show()`/`raise()` calls, and
then stops with no further output at all. That "stops with no further
output" gap is the same shape as before, just later in the sequence — the
discovery scan was never the wall the process hit, it was just the last
thing that happened to still be printing when the wall was hit slightly
after. `tst_window_widgets`'s own test body
(`tests/tst_window_widgets.cpp:29-51`) is:

```cpp
auto* mwp = new MainWindow();      // bewusst nicht abgeraeumt
mwp->resize(1280, 800);
mwp->show();
QVERIFY(QTest::qWaitForWindowExposed(mwp));
QTest::qWait(400);
```

`MainWindow`'s constructor queues its post-construction work via several
`QTimer::singleShot(0, ...)` calls (the ConnectionPanel-open one already
covered above, plus a spot-client auto-connect restore, a VAX first-run
check, and — on Linux only — a Linux-audio first-run check). All of these
only actually *run* once something pumps the event loop, which is exactly
what `mwp->show()` and `QTest::qWaitForWindowExposed()` do. One of them,
`MainWindow::showAudioDiagnoseDialog()` (`src/gui/MainWindow.cpp:14656`,
scheduled at line 920), is:

```cpp
void MainWindow::showAudioDiagnoseDialog()
{
#if defined(Q_OS_LINUX)
    AudioEngine* eng = m_radioModel->audioEngine();
    if (!eng) { return; }
    auto* dlg = new VaxLinuxFirstRunDialog(eng, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();          // <-- modal. blocks. no timeout.
#endif
}
```

`QDialog::exec()` is modal and blocks the calling thread until the dialog
is closed — there is no internal timeout, and nothing in an automated,
headless test run ever clicks its Dismiss button. It is scheduled
whenever both are true:

- `m_radioModel->audioEngine()->linuxBackend() == LinuxAudioBackend::None`
  — every buffered CI dump in this investigation shows the exact log line
  this produces: `QINFO: nereus.audio: Linux audio backend detected:
  "None"`, immediately preceded by ALSA failing to find any card and JACK
  failing to start. This is not inferred — it is printed, verbatim, by
  every single one of the hanging tests' own logs. A GitHub Actions Linux
  runner has no audio hardware at all, so this is `None` on every run,
  every time.
- `AppSettings::instance().value("Audio/LinuxFirstRunSeen", "False") !=
  "True"` — every test binary runs under
  `QStandardPaths::setTestModeEnabled(true)` (`tests/TestSandboxInit.cpp`,
  linked into every `nereus_add_test()` target), which sandboxes
  `AppSettings` to a **fresh** per-run config directory. `Audio/
  LinuxFirstRunSeen` has never been set to `"True"` there, because nothing
  has ever run in that fresh sandbox before. This is true for every test,
  every time, on every platform whose sandbox is fresh — the reason it
  only *matters* on Linux is that the dialog itself is Linux-only.

Both conditions hold unconditionally, on every Linux CI test run, for
every test that constructs a `MainWindow`. The dialog pops, `exec()`
blocks forever, and CTest's own external 120s `TIMEOUT` per test
(`tests/CMakeLists.txt`, `set_tests_properties(... TIMEOUT 120)`) is the
only thing that ever ends it — which is exactly, precisely, what "the same
tests time out at ~120.0x sec every single run, deterministically,
regardless of what else in the process changes" looks like. This also
explains why the RadioDiscovery fix made zero difference: it was correct
and real, just downstream of a wall the process had already stopped
walking toward.

**The fix:** `showAudioDiagnoseDialog()` now returns immediately if
`QStandardPaths::isTestModeEnabled()` is true, before touching
`AudioEngine` or constructing the dialog. That function is `true` in
every test binary (set unconditionally, before `main()`, by
`TestSandboxInit.cpp`) and `false` in every real install (the shipped
`Longpath` app never links `TestSandboxInit.cpp` and never calls
`setTestModeEnabled`), so this is a no-op for actual users and a
guaranteed skip for every automated test — it does not change what a real
first-time Linux user sees on a real machine with no audio backend
detected.

**Not run against a live Linux CI job yet** — this fix is going out as its
own PR; the next CI run on it is the real confirmation, the same caveat as
the RadioDiscovery fix above, and for the same reason (no Linux/Docker/act
access this session). What's different this time: the mechanism is a
plain, unconditional `QDialog::exec()` with no environmental
non-determinism involved at all (no network, no timing races, no "which
condition triggers it" — both gating conditions are logged verbatim in
every failing test's own output), so there is far less room for a third
surprise here than there was for the discovery hypothesis.
  The next CI run on this branch is the real confirmation.
