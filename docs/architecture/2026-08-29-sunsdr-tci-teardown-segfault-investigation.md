# `tst_sunsdr_spectrum_wiring` teardown SIGSEGV — investigation + fix

> **Status: RESOLVED 2026-08-30.** Root cause confirmed by
> AddressSanitizer (a genuine heap-use-after-free, not a guess) and
> fixed with a one-line guard matching an already-established pattern
> in the same class. Verified: the ASAN build caught the bug
> deterministically on its very first run before the fix, and passed
> clean on 3/3 runs after. The original (non-ASAN) build's relevant
> tests all still pass. See "The fix" below for the confirmed
> mechanism; everything under "Investigation" is kept for the record —
> most of its open questions turned out to be moot once ASAN gave a
> real answer instead of more re-running.

## The fix

**Root cause:** `MainWindow::wireSunSdr()`'s `TciClient::stateChanged`
handler (`src/gui/MainWindow_SunSdr.cpp`) touches `m_radioModel`
without checking whether `MainWindow` itself is mid-teardown. During
`~MainWindow()` (reached via `QObjectPrivate::deleteChildren()`, in
turn reached from `closeEvent()`'s own
`QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete)`
call), Qt destroys a parent's children in **insertion order**, which
is unspecified with respect to *this* interaction: `m_radioModel` gets
destroyed before `m_sunSdrClient` (the `TciClient`). But
`TciClient::~TciClient()` calls `disconnectFromEndpoint()` →
`setState()`, which **synchronously emits `stateChanged`** — still
inside the same destruction cascade, on the same thread, same call
stack. The handler's `if (m_radioModel && ...)` guard does not catch
this: `m_radioModel` is a raw pointer that still holds the old
address — it was never nulled, only the object it pointed to was
freed. Calling `m_radioModel->audioEngine()` (the first thing the
handler does) reads a freed `RadioModel` — confirmed byte-for-byte by
AddressSanitizer (see "ASAN evidence" below).

**The fix** (`src/gui/MainWindow_SunSdr.cpp`, one line): the handler
now checks `m_shuttingDown` first and returns immediately if set —
the exact same guard `closeEvent()` already uses for an analogous
problem elsewhere in this same class (the auto-open-ConnectionPanel
slot, `MainWindow.cpp:13540`). `m_shuttingDown` is set as the literal
first line of `closeEvent()` (`MainWindow.cpp:13750`), so it is
already `true` by the time this destruction cascade runs.

**Verification:**
- Before the fix: the ASAN-instrumented build (`build-asan/`, see
  "AddressSanitizer setup" below) caught the heap-use-after-free on
  its **first** run, deterministically — not a rare timing window
  under ASAN's instrumentation.
- After the fix: 3/3 clean ASAN runs.
- The normal (non-ASAN, `RelWithDebInfo`) `build/` also rebuilt clean
  (`./tools/syntax_check.sh` + `cmake --build`) and every directly
  relevant test passes: `tst_sunsdr_spectrum_wiring`,
  `tst_sunsdr_connect_wiring`, `tst_sunsdr_control_wiring`,
  `tst_sunsdr_audio_feed`, `tst_sunsdr_is_reachable`,
  `tst_quit_leaves_no_pending_deletes`,
  `tst_a_second_receiver_can_be_closed` — 7/7 green.

## ASAN evidence

```
==11613==ERROR: AddressSanitizer: heap-use-after-free on address 0x623000147530
READ of size 8 at 0x623000147530 thread T0
    #0 Longpath::RadioModel::audioEngine()
    #1 Longpath::MainWindow::wireSunSdr()::$_3::operator()(TciClient::State, QString const&) const
    #2-#6 Qt signal-dispatch machinery (doActivate, QCallableObject, ...)
    #7 Longpath::TciClient::stateChanged(TciClient::State, QString const&)
    #8 Longpath::TciClient::setState(TciClient::State, QString const&)
    #9 Longpath::TciClient::disconnectFromEndpoint()
    #10-#12 Longpath::TciClient::~TciClient()  (three frames: the delegating dtor chain)
    #13 QObjectPrivate::deleteChildren()
    #14 QWidget::~QWidget()
    #15-#17 Longpath::MainWindow::~MainWindow()
    #18 QObject::event(QEvent*)   <- handling a DeferredDelete
    ...
    #25 Longpath::MainWindow::closeEvent(QCloseEvent*)
    ...
    #40 TstSunSdrSpectrumWiring::ddsUndIqRahmenSetzenDieWahreMittenfrequenzAmPanadapter()

freed by thread T0 here:
    #0 operator delete
    #1 Longpath::RadioModel::~RadioModel()
    #2 QObjectPrivate::deleteChildren()      <- SAME cascade, same call
    #3 QWidget::~QWidget()                      to closeEvent()'s sendPostedEvents
    #4-#6 Longpath::MainWindow::~MainWindow()
    #7 QObject::event(QEvent*)
    ...
    #13 Longpath::MainWindow::closeEvent(QCloseEvent*)
    ...
    #28 TstSunSdrSpectrumWiring::ddsUndIqRahmenSetzenDieWahreMittenfrequenzAmPanadapter()

previously allocated by thread T0 here:
    #0 operator new
    #1 Longpath::MainWindow::MainWindow(QWidget*)   <- m_radioModel(new RadioModel(this))
```

Both the "freed by" and the "read" (use-after-free) stacks bottom out
in the **same** test function's own `mw->close()` call — this is not
a cross-test interaction, and not a race with another thread (ASAN
would have reported a data race differently, and the earlier `.ips`
crash report already showed all other threads idle). It is a genuine
same-thread reentrancy bug: `MainWindow::closeEvent()` triggers its
own object's full teardown (via the global `sendPostedEvents` flush)
*while still running*, and a child object's destructor fires a signal
whose handler assumes teardown hasn't started yet.

## AddressSanitizer setup (now working for tests, wasn't before)

`CMakeLists.txt`'s `if(CMAKE_BUILD_TYPE STREQUAL "Debug")` block
already compiled `LongpathObjs` with `-fsanitize=address` (a `PUBLIC`
compile option, inherited by every test executable that links against
it), but only added the matching **link** option to the `Longpath` app
target — not to test executables (`tests/CMakeLists.txt`). A
Debug-configured test build would therefore fail to link at all
(undefined `__asan_*` symbols): the sanitizer support existed on paper
but was never actually usable for tests. Fixed in
`tests/CMakeLists.txt`'s `nereus_add_test()` function, mirroring the
same Debug-only gate. To reuse this for a future investigation:

```
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DNEREUS_BUILD_TESTS=ON
cmake --build build-asan --target <test_name> -j <n>
ASAN_OPTIONS=detect_leaks=0:halt_on_error=0 ./build-asan/tests/<test_name>
```

(`detect_leaks=0` because this codebase's tests deliberately leak
`MainWindow` instances — see "Loose end" below; leave `halt_on_error`
at its default `1` to stop at the first real error unless, like here,
you want to see the test finish anyway.)

---

## Investigation (kept for the record)

The original hunt for this crash, before ASAN gave a direct answer.
Some of the theories below are now moot; kept because the negative
results (what was ruled out, and how) are still useful precedent for
the next time a crash in this neighborhood needs chasing.

### What happened

`tst_sunsdr_spectrum_wiring` crashed with SIGSEGV during a full,
10-way-parallel `ctest` run (2026-08-29, ~06:16 CEST), inside
`ddsUndIqRahmenSetzenDieWahreMittenfrequenzAmPanadapter()`, right after
`mw->close()`. It did **not** reproduce again by re-running, across
roughly 20 attempts under several conditions (2026-08-29/30): 3
standalone runs, a targeted 30-test parallel subset alongside other
GPU/spectrum-heavy tests (`-j 10`, 100% pass), two full 815-test suite
runs (once serial, once matching the original's `-j 10` parallelism —
both 100% pass), a full 294-test `gui`-labelled parallel run (100%
pass), and 14 further standalone runs under `lldb`
(`-o run -o bt all -o quit` in a loop; the one non-clean iteration was
`lldb`'s own attach mechanism failing to pause the process, not the
target crashing). **This is itself a useful data point in hindsight:**
a genuine use-after-free that ASAN catches on its first instrumented
run can still be nearly unreproducible on an uninstrumented build,
because it depends on the freed memory not having been overwritten
yet when the dangling read happens — pure luck of the allocator, not a
rare code path. "Doesn't reproduce after N re-runs" was never good
evidence this wasn't a real bug.

The one concrete non-ASAN artifact was a real macOS crash report macOS
itself saved (`~/Library/Logs/DiagnosticReports/tst_sunsdr_spectrum_wiring-2026-08-29-061622.ips`),
with full symbol names for every thread — genuinely useful before ASAN
entered the picture, and worth remembering as a source for any future
from-a-single-crash investigation in this project.

### What was ruled out along the way

- **A live cross-thread race at the moment of the crash:** no. The
  `.ips` report's 27 threads were all idle except the crashing one.
- **Two `removeReceiver()` call sites racing each other**
  (`MainWindow_SunSdr.cpp:333` and `:620`, both main-thread): not a
  race (same thread, sequential), and a redundant `remove()` on an
  absent key is a defined no-op regardless.
- **`FFTRouter`/`RadioModel` destroyed before the callback ran, in
  general:** this early guess undersold the real mechanism — they
  *are* destroyed before the callback runs, but only because the
  whole `MainWindow` is reentering its own teardown from inside
  `closeEvent()`, not because of some earlier, unrelated destruction.

### Loose end noticed in passing (not this crash's cause, still true)

Every test function in `tst_sunsdr_spectrum_wiring.cpp` does
`auto* mw = new MainWindow();` and `mw->close();`, but never
`delete mw` and never sets `Qt::WA_DeleteOnClose`. This is why
`ASAN_OPTIONS=detect_leaks=0` is needed above — otherwise ASAN's leak
checker (a separate feature from the use-after-free detector that
found this bug) reports every leaked `MainWindow` as a leak at exit.
Not touched here; changing it would affect five tests' behavior on a
guess unrelated to this investigation's actual finding.

### Related prior incidents (same neighborhood — for context)

- `MainWindow.cpp:2020-2038` — dangling `FFTEngine*` in
  `m_fftEngines` after the engine's real destruction outran the map
  cleanup (`Longpath-2026-08-24-093116.ips`). Fixed via a
  `QObject::destroyed` cleanup connection.
- `MainWindow.cpp:13827-13850` (`closeEvent()`) — floating-window
  `QOpenGLContext`/`QThreadStorageData` SIGSEGV during Cocoa's
  terminate cascade (2026-08-21, twice). Fixed by tearing down floating
  windows explicitly before Qt's own thread-data teardown runs.
- `longpath-session-2026-08-27-layout-crash-hunt` (session memory, not
  a file in this repo) — a reentrant close-triggered crash plus five
  related profile/window bugs, all found and fixed the same night.

This crash is a fourth instance of the same family: `closeEvent()`
re-entering its own object's teardown mid-flight, and something
downstream assuming teardown hasn't started. Worth keeping in mind
that `m_shuttingDown` is the established guard for this family, but is
opt-in per call site — anything new added to `wireSunSdr()` (or
similar per-feature wiring functions) that reacts to a child object's
destruction-time signals should check it too, not just this one
handler.
