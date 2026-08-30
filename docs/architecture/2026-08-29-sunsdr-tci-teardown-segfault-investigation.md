# `tst_sunsdr_spectrum_wiring` teardown SIGSEGV — investigation notes

> **Status:** Investigated, root cause NOT confirmed. Exact crash site
> identified from a real, symbolicated macOS crash report — but the
> *reason* the crash happens is still open. No fix has been attempted;
> per `CLAUDE.md`'s AI-agent boundary ("Bugs with clear root cause"),
> this does not yet qualify, and this teardown path already has a long
> history of crash fixes (see "Related prior incidents" below) — a
> guess here risks masking the real bug or introducing a new one.

## What happened

`tst_sunsdr_spectrum_wiring` crashed with SIGSEGV during a full,
10-way-parallel `ctest` run (2026-08-29, ~06:16 CEST), inside
`ddsUndIqRahmenSetzenDieWahreMittenfrequenzAmPanadapter()`, right after
`mw->close()`. It has **not** reproduced since, across roughly 20
attempts under several conditions (2026-08-29/30): 3 standalone runs,
a targeted 30-test parallel subset alongside other GPU/spectrum-heavy
tests (`-j 10`, 100% pass), two full 815-test suite runs (once serial,
once matching the original's `-j 10` parallelism — both 100% pass), a
full 294-test `gui`-labelled parallel run (100% pass, run concurrently
with the lldb loop below), and 14 further standalone runs under `lldb`
(`-o run -o bt all -o quit` in a loop; the one non-clean iteration was
`lldb`'s own attach mechanism failing to pause the process, not the
target crashing — a tooling hiccup, not a repro). The one concrete
artifact is still the real macOS crash report macOS itself saved:

```
~/Library/Logs/DiagnosticReports/tst_sunsdr_spectrum_wiring-2026-08-29-061622.ips
```

That file has full symbol names (not just addresses) for every thread,
which is what made this investigation possible without a live debugger
session.

## Exact crash site

Crashing thread (`com.apple.main-thread`), top of stack:

```
QMapData<std::__1::map<int, QList<QString>, ...>>::copyIfNotEquivalentTo(...)
QMap<int, QList<QString>>::remove(int const&)
Longpath::FFTRouter::removeReceiver(int)
QtPrivate::QCallableObject<Longpath::MainWindow::wireSunSdr()::$_3,
    QtPrivate::List<Longpath::TciClient::State, QString const&>, void>::impl(...)
void doActivate<false>(QObject*, int, void**)
```

`doActivate<false>` means this was a **direct**, same-thread call — not
a queued cross-thread signal. So `TciClient::stateChanged` fired
synchronously on the main thread, and its handler in
`MainWindow::wireSunSdr()` (`src/gui/MainWindow_SunSdr.cpp:614-621`)
ran:

```cpp
connect(m_sunSdrClient, &TciClient::stateChanged, this,
        [this](TciClient::State state, const QString& detail) {
    ...
    if (state == TciClient::State::Error
        || state == TciClient::State::Disconnected) {
        if (m_radioModel && m_sunSdrTargetSliceId >= 0) {
            ...
            if (auto* router = m_radioModel->fftRouter()) {
                router->removeReceiver(kSunSdrPseudoStreamIndex);  // <-- crashes inside this call
            }
        }
    }
});
```

The fault: `esr` = "(Data Abort) byte read Translation fault", faulting
address in the `0xc10001xx` range — a garbage pointer read, consistent
with either heap corruption or a stale/freed structure, encountered
deep inside Qt's own copy-on-write detach for the `QMap<int,
QList<QString>>` that backs `FFTRouter::m_receiverToPans`
(`src/core/FFTRouter.h:61`, `src/core/FFTRouter.cpp:27-29`).

## What's been ruled out

- **`FFTRouter`/`RadioModel` destroyed before the callback ran:** no.
  `m_radioModel` is constructed once in `MainWindow`'s ctor
  (`m_radioModel(new RadioModel(this))`, `MainWindow.cpp:518`) and
  `m_fftRouter` once in `RadioModel`'s ctor (`new FFTRouter(this)`,
  `RadioModel.cpp:860`) — both parented, never reassigned, never
  explicitly deleted. The test never calls `delete mw` (every test in
  this file leaks its `MainWindow`, deliberately or not — see "Loose
  end" below), so neither object should have been destroyed at any
  point during the test's execution.
- **A live cross-thread race at the moment of the crash:** no. The
  crash report has all 27 threads' stacks. None of the other 26 is
  executing anything at the crash instant — every one is parked on a
  semaphore/`poll`/`__psynch_cvwait`. This doesn't rule out corruption
  written earlier by another thread and only *read* here, but it does
  rule out a simultaneous read+write.
- **A second, unsynchronized `removeReceiver()` call site racing this
  one:** structurally there are two call sites for the same
  `kSunSdrPseudoStreamIndex`
  (`MainWindow_SunSdr.cpp:333` in `releaseSunSdrSlice()`, triggered by
  `RadioModel::sliceRemoved`, and `:620` here) — both are main-thread,
  so they can't be concurrent with each other, and `QMap::remove()` on
  an absent key is a defined, safe no-op either way.
- **All other `FFTRouter` mutators are also main-thread-only** — grep
  confirms every call site (`MainWindow.cpp:3167,3204,3229,4384`,
  `MainWindow_SunSdr.cpp:333,620`) is in GUI code, none in
  `FFTEngine`/`m_fftThread` code.

## What's still open (this is the actual gap)

The QMap COW-detach failure signature is the classic symptom of either
(a) heap corruption written by something unrelated earlier in the
run, now surfacing on next touch, or (b) `this` (the `FFTRouter*`)
being a valid-looking but wrong pointer — and neither has been pinned
down. Two directions worth trying with real tooling this investigation
didn't have access to:

1. ~~**Reproduce under a debugger, not by re-running.**~~ **Tried,
   2026-08-30 — did not reproduce.** 14 standalone runs under
   `lldb -o run -o bt all -o quit` in a loop, plus a full 294-test
   `gui`-labelled parallel run alongside them, plus two full 815-test
   suite runs (one serial, one at the original's own `-j 10`
   parallelism) — all clean. This crash genuinely does not reproduce
   on demand, even under load conditions that should match the
   original. Ruled out as "just re-run it under a debugger and wait" —
   whatever triggers this needs either far more attempts than were
   tried here, or a fundamentally different detection method (below).
2. **AddressSanitizer — set up 2026-08-30, not yet run against this
   crash.** `CMakeLists.txt`'s existing `if(CMAKE_BUILD_TYPE STREQUAL
   "Debug")` block already compiles `LongpathObjs` with
   `-fsanitize=address`, but test executables never got the matching
   *link* option (`tests/CMakeLists.txt`), so a Debug test build would
   have failed to link at all — the sanitizer support existed but was
   never actually usable for tests. Fixed (mirrors the same Debug-only
   gate on `nereus_add_test()`'s executables). A fresh `build-asan/`
   configured `-DCMAKE_BUILD_TYPE=Debug`; building
   `tst_sunsdr_spectrum_wiring` there and running it — even just once,
   even if it doesn't crash outright — has a real chance of catching
   heap corruption or a use-after-free at the exact moment it happens,
   not just when something later touches the corrupted memory (which
   is what makes this bug so hard to catch by re-running: the write
   that corrupts things and the read that crashes on them may be far
   apart in time, exactly the case ASAN exists for). Whoever picks
   this up next: check `build-asan/` for a compiled ASAN build before
   reconfiguring from scratch.
3. **Check for a leak/multiplication effect across the file's five
   test functions.** As noted below, none of them delete their
   `MainWindow`. If that's intentional (QTest tears the process down
   after the last test regardless), fine — but if `wireSunSdr()`'s
   connections or `FFTRouter` state from an *earlier* test function in
   this same file are somehow still live and interacting with a *later*
   test's fresh `MainWindow`/`FFTRouter` (e.g. via a static/global,
   or via `QApplication::topLevelWidgets()` iterating stale windows),
   that would explain corruption that only shows up under load stress
   thinning out timing margins.

## Loose end noticed in passing (not investigated further, not this crash's confirmed cause)

Every test function in this file does `auto* mw = new MainWindow();` and
`mw->close();`, but never `delete mw` and never sets
`Qt::WA_DeleteOnClose`. Whether this is deliberate (relying on process
exit at `cleanupTestCase()`/end of `main()` to reclaim everything) or an
oversight is not established here — it wasn't touched because doing so
would change five tests' behavior on a guess, which is exactly the kind
of change this investigation is trying to avoid making without proof.

## Related prior incidents (same neighborhood, already fixed — for context, not because they're the same bug)

- `MainWindow.cpp:2020-2038` — dangling `FFTEngine*` in
  `m_fftEngines` after the engine's real destruction outran the map
  cleanup (`Longpath-2026-08-24-093116.ips`). Fixed via a
  `QObject::destroyed` cleanup connection.
- `MainWindow.cpp:13827-13850` (`closeEvent()`) — floating-window
  `QOpenGLContext`/`QThreadStorageData` SIGSEGV during Cocoa's
  terminate cascade (2026-08-21, twice). Fixed by tearing down floating
  windows explicitly before Qt's own thread-data teardown runs.
- `docs/architecture/...` / `longpath-session-2026-08-27-layout-crash-hunt`
  (session memory, not a file in this repo) — a reentrant
  close-triggered crash plus five related profile/window bugs, all
  found and fixed the same night.

None of these are confirmed to be the same root cause as this one —
listed only so a future debugging session doesn't have to rediscover
that this exact neighborhood (`MainWindow::closeEvent`, `FFTRouter`,
window/thread teardown ordering) has a real track record of exactly
this failure mode, which should raise the prior for "teardown ordering
bug" over "one-off cosmic ray."

## Debug Session Prompt

Paste the following into a fresh Claude Code session (ideally one with
`lldb` access) to pick this up:

```
Read docs/architecture/2026-08-29-sunsdr-tci-teardown-segfault-investigation.md
for full context. tst_sunsdr_spectrum_wiring crashed once with SIGSEGV
inside FFTRouter::removeReceiver() (called from MainWindow::wireSunSdr()'s
TciClient::stateChanged handler), deep in Qt's QMap copy-on-write detach.
It has not reproduced in ~4 attempts since, including one matching the
original's parallel-load conditions. Try to reproduce it under lldb
(loop the standalone binary, or attach to it during a real full-suite
-j10 ctest run) and get a live backtrace + inspect the FFTRouter/RadioModel
object state at the crash. Do not land a speculative fix without a
confirmed root cause -- this teardown path has a real history of crashes
that turned out to need the actual mechanism understood, not guessed at.
```
