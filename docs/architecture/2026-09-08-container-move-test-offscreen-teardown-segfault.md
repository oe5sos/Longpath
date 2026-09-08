# `tst_real_container_move` teardown SIGSEGV — investigation + fix

> **Status: RESOLVED 2026-09-08.** Confirmed by AddressSanitizer plus a
> cocoa/offscreen A/B comparison to be a genuine bug inside Qt's own
> `offscreen` platform plugin (`QOffscreenBackingStore::clearHash()`),
> not a Longpath use-after-free. Fixed by skipping the one affected
> test function under the `offscreen` platform only — the same guard
> shape already used in `MacFloatingWindowBehavior.mm`. The test still
> runs, and would still catch a real regression, under any real window
> system.

## The fix

**Root cause:** `QOffscreenBackingStore::clearHash()` — a function
inside Qt's prebuilt `libqoffscreen.dylib`, no Longpath code anywhere
in the call stack — dereferences a null pointer while walking its own
static `m_backingStoreForWinIdHash` (a process-wide `QHash` mapping
`WId → QOffscreenBackingStore*`, maintained purely by the offscreen
platform plugin itself). This runs during ordinary top-level-widget
teardown, once `aFloatedContainerIsNotFullScreen()` has created and
left alive three independent top-level windows (`MainWindow`, the
floated `FloatingContainer`, and the detached `AppletFloatingWindow`)
— exactly what the test is deliberately checking (see the file's own
"bewusst nicht abgeraeumt" comments). Longpath's own object graph is
not corrupted: ASAN found no heap-use-after-free, no double-free, no
buffer overflow, and the same widget-tree dance runs clean under a
real window system (see "cocoa vs. offscreen" below).

**The fix** (`tests/tst_real_container_move.cpp`): `QSKIP` at the top
of `aFloatedContainerIsNotFullScreen()`, gated on
`QGuiApplication::platformName() == "offscreen"`, before any widget is
constructed — so the crash-triggering combination of top-level windows
never comes into existence under the platform where Qt's own cleanup
code cannot tear it down. Nothing about this touches
`MacFloatingWindowBehavior.mm` (already fixed and verified separately,
16/20 of the original macOS CI crashes) or any other Longpath source.
The other two test functions in the file are unaffected — they build
at most two top-level windows, a combination that has never crashed in
this investigation, on either platform.

**Why not fix it in application code instead:** there is no
Longpath-side bug to fix. Every angle explored below (ASAN, a minimal
Qt-only repro, disassembly of the actual crash instruction) points the
same direction: Qt's own compiled code, unconditionally, regardless of
what Longpath does. Trying to dodge it by changing how/when the
widgets get destroyed was already tried once (see "What was ruled out"
below) and made no difference — the crash follows the *existence* of
this widget combination, not the destruction order.

**Verification:**
- Before the fix: offscreen platform crashed **5/5** runs (the
  original investigation's reproduction, plus 4 more in this session),
  always the identical `AddressSanitizer:DEADLYSIGNAL` at address
  `0x20` in `QOffscreenBackingStore::clearHash()`.
- Same ASAN binary, same test, **real `cocoa` platform** (just running
  the binary without `QT_QPA_PLATFORM=offscreen`, on this Mac's actual
  display): **4/4 clean** runs, `Totals: 3 passed`, exit 0, zero ASAN
  reports of any kind.
- After the fix: `QT_QPA_PLATFORM=offscreen ./build/tests/tst_real_container_move`
  ran clean 5/5 (the affected test now reports `SKIP`, the other two
  still `PASS`); the normal (non-ASAN) `build/` build compiles clean
  and `./tools/syntax_check.sh` passes on the changed file.

## ASAN evidence

```
AddressSanitizer:DEADLYSIGNAL
==73741==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000020
==73741==The signal is caused by a READ memory access.
==73741==Hint: address points to the zero page.
    #0 QOffscreenBackingStore::clearHash()+0x150      (libqoffscreen.dylib)
    #1 QOffscreenBackingStore::~QOffscreenBackingStore()+0x20 (libqoffscreen.dylib)
    #2 QOffscreenBackingStore::~QOffscreenBackingStore()+0x8  (libqoffscreen.dylib)
    #3 QBackingStore::~QBackingStore()+0x24           (QtGui)
    #4 QWidgetWindow::~QWidgetWindow()+0x78           (QtWidgets)
    #5 QWidgetWindow::~QWidgetWindow()+0x8            (QtWidgets)
    #6 QWidget::destroy(bool, bool)+0x2ec             (QtWidgets)
    #7 QApplication::~QApplication()+0x1c0            (QtWidgets)
    #8 main()                                          (tst_real_container_move)
```

Note what's *not* here: no Longpath frame anywhere, and no
"previously freed by" / "previously allocated by" stanza — the two
things ASAN always prints for a genuine heap-use-after-free (compare
the `tst_sunsdr_spectrum_wiring` investigation from 2026-08-29, which
had exactly that shape). `DEADLYSIGNAL` means ASAN's own
allocator-poisoning machinery has nothing to say about this address —
it caught a raw SIGSEGV via the process signal handler, the same as it
would for any null-pointer dereference in uninstrumented code.

## Disassembly: this is Qt's own code, not corrupted data

`lldb` at the crash:

```
frame #0: QOffscreenBackingStore::clearHash() + 336
    0x...3f4 <+328>: cbz    x8, 0x...3f4          ; (one of two empty-span checks)
    0x...3f4 <+328>: mov    x2, #0x0
    0x...3f8 <+332>: mov    x1, #0x0
->  0x...3fc <+336>: ldr    x9, [x1, #0x20]       ; <-- crash: x1 was just set to 0
```

Register `x20` at the crash holds the address of the symbol
`QOffscreenBackingStore::m_backingStoreForWinIdHash` — confirming
which static structure is being walked. The crashing instruction is
not reading a stray/dangling pointer that Longpath wrote — it is
reading a register that Qt's own compiled function set to the literal
constant `0` two instructions earlier, on a code path reached after an
internal "is this span/bucket empty" check. This is consistent with an
edge case in Qt 6.11.1's inlined `QHash` span-walking logic when the
static hash empties out via a particular sequence of insertions and
removals — not with heap corruption from application code.

## cocoa vs. offscreen — the deciding comparison

Both runs use the identical ASAN-instrumented binary
(`build-asan/tests/tst_real_container_move`,
`-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=address`),
identical test, identical machine — the only variable is
`QT_QPA_PLATFORM`:

| Platform | Runs | Result |
| --- | --- | --- |
| `offscreen` (what CI uses on both Linux and macOS, `ci.yml:684/687`) | 5/5 | `AddressSanitizer:DEADLYSIGNAL` at the same address every time |
| `cocoa` (this Mac's real display — what every operator actually runs) | 4/4 | `Totals: 3 passed`, exit 0, no ASAN findings |

This is the concrete version of the operator's own standing principle
(task item 6): a behavior that reproduces 100% under the headless test
backend and 0% under a real window system, with the entire crash stack
sitting inside Qt's own compiled library, is a platform-plugin defect
to document — not a Longpath bug to chase further, and not something
to paper over with unrelated application-code changes.

## What was ruled out along the way

- **A UAF/double-free during the nested float→detach reparent dance**
  (`ContainerManager::setMeterFloating` / `FloatingContainer::takeOwner`
  / `MainWindow::detachApplet`, the code this test actually exercises):
  no. ASAN instruments exactly this code (`LongpathObjs` compiles with
  `-fsanitize=address` in `CMAKE_BUILD_TYPE=Debug`, see
  `CMakeLists.txt:1724-1726`) and found nothing — no heap error of any
  kind, on either platform.
- **Destruction *order/timing* mattering** — a prior pass (before ASAN
  was wired up for tests) added a `cleanup()` slot that explicitly
  `delete`s the `MainWindow` before the test function returns, instead
  of leaving teardown to `~QApplication()`. This produced the
  *identical* crash at the identical address, then was reverted as
  unhelpful complexity. In hindsight this is exactly what you'd expect
  once the real cause was known: `delete m_mwp` tears down the same
  `QWidgetWindow → QBackingStore → QOffscreenBackingStore` chain
  through the same `clearHash()` call, just triggered a few
  microseconds earlier in the same process. There is no ordering that
  avoids it once these three top-level windows have existed.
- **A minimal, Longpath-free repro** (a standalone `QApplication` +
  four bare `QWidget`s, never deleted): does not crash. A closer
  repro — a `MainWindow`-shaped top-level widget, a `Qt::Tool`-flagged
  "container" widget constructed unparented then `setParent()`'d onto
  it (mirroring `FloatingContainer`'s exact reparent-after-construction
  pattern), a child widget moved into it, and a second `Qt::Tool`
  sibling receiving a widget pulled out of the first (mirroring
  `AppletFloatingWindow`) — also does not crash standalone. The real
  app's much larger churn of top-level `QOffscreenBackingStore`
  registrations before this point (`ConnectionPanel`, repeated
  `MeterWidget` teardown/recreation via
  `ContainerManager::extractMeterItems` / `installFreshMeter` around
  every float/dock operation, etc.) is evidently needed to put the
  static hash into the specific bucket/span layout that triggers the
  bug — consistent with an internal hash-table edge case, not with a
  small, easily-isolated Longpath defect.
- **An already-known/filed Qt bug matching this exact signature**: not
  found (checked the Qt bug tracker and forums). The general *class*
  of bug — a static/global container's own destroy-time cleanup
  breaking during `QApplication` teardown — is a recurring, documented
  Qt failure mode (e.g. `QTBUG-7746`, a global `QHash<..., QPixmap>`
  crashing on exit), which is at least precedent that this shape of
  bug does happen in Qt and is not implausible here.

## Scope note

This investigation covers only `tst_real_container_move` (specifically
`aFloatedContainerIsNotFullScreen()`). `MacFloatingWindowBehavior.mm`'s
`enableFullScreenAuxiliaryBehavior()` guard (same day, earlier in this
investigation) fixed 16 of the original 20 macOS CI test crashes,
unrelated to this one. Three more of the original crash set remain
unexamined; whether they share this same offscreen-platform root cause
or are distinct is not yet known.
