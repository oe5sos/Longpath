#!/usr/bin/env python3
"""Fail if a spectrum slice control is wired to activeSpectrumWidget().

Bench 2026-07-28: two slices on one pan, and click-to-tune retuned flag A
however many times the operator selected flag B.

activeSpectrumWidget() resolves to pan-0 at startup and connect() binds the
OBJECT, not the expression, so a connect made through it is pan-0 forever.
Worse, the lambdas on the other end captured `slice` by value, which is
whatever RadioModel::activeSlice() happened to be when the wiring ran, i.e.
Slice A. Click-to-tune, the filter-edge drag, the pan drag and the CTUN
toggle therefore acted on Slice A for the life of the session, and never
consulted the pan's own active slice at all.

The per-pan replacements resolve their target through
MainWindow::sliceForPan(panId) on every signal, so they follow the flag the
operator selected. This check exists because the two shapes look almost
identical in review, and the codebase has been bitten by exactly this
"two paths, one incomplete" split before (the flag-path unification that
created createSliceFlag, and the WDSP channel-0 hardcodes that
verify-no-gui-dsp-access.py now guards).

Only the controls that TARGET A SLICE are listed. Display-only wiring
(dbmRangeChangeRequested and friends) has no slice to get wrong and is
deliberately not covered.

Zweite Regel, 2026-10-22: dieselbe Falle in der ANDEREN Richtung.
Display-only wiring hat keine Scheibe, aber es hat einen PAN -- und der
war falsch. Sieben Regler der Knopfleiste (Wasserfall-Verstaerkung,
Schwarzpunkt, Farbtafel, 2D/3D, Frequenz am Zeiger, Fuellfarbe,
Deckkraft) waren einmalig an activeSpectrumWidget() als EMPFAENGER
gebunden, also an Pan 0. Jedes der sieben Signale kam im ganzen Programm
genau einmal vor; im Zweipan-Bild bewegte der Regler im Flyout des
zweiten Pans nichts. Gemessen am 2026-10-22 in
tests/tst_anzeigeregler_auf_jedem_pan.cpp: alle sieben Werte blieben auf
der Vorgabe stehen. Behoben in
MainWindow::wireDisplayControlsForPan(panel, sw).

Diese Regel prueft deshalb den EMPFAENGER: ein Setter aus der Liste
unten darf nicht ueber activeSpectrumWidget() verdrahtet werden.

Noch NICHT abgedeckt, weil es dort nicht um einen Mangel, sondern um
eine Gestaltungsfrage geht -- welcher Pan soll es zeigen:
  * setTxFilterRange    (das TX-Filterband; MainWindow.cpp ~5941)
  * onNoiseFloorChanged (das rauschflurgefuehrte Raster; ~5970)
Wer das entscheidet, nimmt sie hier mit auf.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUI = ROOT / "src" / "gui"

# Signals whose handler has to pick a slice to act on.
SLICE_CONTROL_SIGNALS = (
    "frequencyClicked",
    "filterEdgeDragged",
    "centerChanged",
    "ctunEnabledChanged",
)

# The sender expression is captured rather than matched against
# activeSpectrumWidget() literally. The CTUN toggle was wired through a local
# alias -- `SpectrumWidget* const ctunPan = activeSpectrumWidget();` -- so a
# check that only knew the literal spelling passed it, which is the whole
# failure mode being guarded. The rule is therefore positive: the sender must
# be the per-pan `sw` parameter and nothing else.
PATTERN = re.compile(
    r"connect\s*\(\s*([A-Za-z_][A-Za-z0-9_]*(?:\s*\(\s*\))?)\s*,\s*"
    r"&SpectrumWidget::(" + "|".join(SLICE_CONTROL_SIGNALS) + r")\b"
)
PER_PAN_SENDER = "sw"

# Anzeige-Setter, die je Pan verdrahtet werden muessen. Hier ist
# SpectrumWidget der EMPFAENGER, also steht das Ziel beim connect fest --
# und darf deshalb nicht activeSpectrumWidget() sein.
PER_PAN_DISPLAY_SLOTS = (
    "setWfColorGain",
    "setWfBlackLevel",
    "setWfColorScheme",
    "setSpectrumRenderMode",
    "setCursorFreqVisible",
    "setFillColor",
    "setFillAlpha",
)

# Als Empfaenger: `activeSpectrumWidget()` direkt vor einem dieser Setter.
RECEIVER_PATTERN = re.compile(
    r"activeSpectrumWidget\s*\(\s*\)\s*,\s*&SpectrumWidget::("
    + "|".join(PER_PAN_DISPLAY_SLOTS) + r")\b"
)

# Und der Lambda-Umweg, der die beiden Enum-Setter frueher nahm:
# `connect(panel, &…::colorSchemeChanged, activeSpectrumWidget(), [this]…)`
# mit dem Setter IM Rumpf. Eine Zeile, die activeSpectrumWidget() als
# Empfaenger nennt UND irgendwo einen dieser Setter aufruft, reicht.
LAMBDA_PATTERN = re.compile(
    r"activeSpectrumWidget\s*\(\s*\)\s*->\s*("
    + "|".join(PER_PAN_DISPLAY_SLOTS) + r")\s*\("
)

def main() -> int:
    failures = []
    receiver_failures = []
    for path in sorted(GUI.rglob("*.cpp")):
        rel = path.relative_to(ROOT).as_posix()
        for num, line in enumerate(path.read_text().splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            m = PATTERN.search(line)
            if m and m.group(1) != PER_PAN_SENDER:
                failures.append(f"{rel}:{num}: {stripped}")
            r = RECEIVER_PATTERN.search(line) or LAMBDA_PATTERN.search(line)
            if r:
                receiver_failures.append(
                    f"{rel}:{num}: {r.group(1)} -- {stripped}")
    if receiver_failures:
        print("[spectrum-wiring] Anzeige-Setter muessen je Pan verdrahtet "
              "werden, nicht ueber activeSpectrumWidget().")
        print("Wire them in MainWindow::wireDisplayControlsForPan(panel, sw), "
              "die ensureOverlayPanels() fuer JEDEN Pan aufruft -- sonst "
              "bewegt der Regler im Flyout jedes zweiten Pans nichts "
              "(gemessen 2026-10-22).")
        for f in receiver_failures:
            print(f"  {f}")
    if failures:
        print("[spectrum-wiring] Slice controls must be wired per pan, not "
              "through activeSpectrumWidget().")
        print("Wire them in MainWindow::wireSpectrumSliceControls(sw, panId), "
              "which resolves the target through sliceForPan(panId) on every "
              "signal instead of capturing one slice at connect time.")
        for f in failures:
            print(f"  {f}")
    if failures or receiver_failures:
        return 1
    print("[spectrum-wiring] OK: no slice control and no per-pan display "
          "setter wired to activeSpectrumWidget()")
    return 0

if __name__ == "__main__":
    sys.exit(main())
