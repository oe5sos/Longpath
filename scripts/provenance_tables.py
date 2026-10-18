#!/usr/bin/env python3
"""Shared reader for the attribution tables under `docs/attribution/`.

Not a check of its own -- a library for the two gates that both have to
agree on *which files are registered where*:

  * `scripts/check-new-ports.py` -- the merge gate. Uses the first
    column of every provenance table as an allowlist ("is this file
    registered anywhere at all?").
  * `scripts/compliance-inventory.py` -- the inventory. Needs the
    narrower question answered ("does this file carry a real derivation
    claim, so the bucket's header markers are required?").

They used to answer those questions with two unrelated extractors, and
on 2026-10-09 that cost us: `AETHERSDR-PORTS.md` was read by the merge
gate but not by the inventory, so 97 AetherSDR ports -- the whole
channel strip, every KiwiSDR file, the ASR backend, the SPE amplifier
port -- were counted as `nereussdr-original` and their header markers
were never checked. The first column is parsed in ONE place now.
"""
from __future__ import annotations

import re
from pathlib import Path

# A first cell may name several files at once. Two shorthands are in use
# across the tables, both handled by `_resolve_first_cell`:
#   `src/core/Foo.{h,cpp}`          -- brace pair
#   `src/core/Foo.h`, `.cpp`        -- extension-only follow-on token
_BRACE_PAIR = re.compile(r"(src/.+)\.\{h,cpp\}$")
_REGISTERED_PREFIX = re.compile(r"(src|tests|third_party)/")

# Second-column cells that do NOT name an upstream source. The tables use
# these for rows that are registered for bookkeeping but are explicitly
# Longpath-original ("Kein Port" / "(none -- ...)"); see
# `aethersdr_port_paths`.
_NO_SOURCE_PREFIXES = ("—", "–", "-", "(none")


def _split_row(line: str) -> list[str] | None:
    """Return a markdown table row's cells, or None if `line` isn't one."""
    line = line.strip()
    if not line.startswith("|") or line.startswith("|---"):
        return None
    cells = [c.strip() for c in line.strip("|").split("|")]
    return cells or None


def _resolve_first_cell(first_cell: str) -> list[str]:
    """Expand one first-column cell into repo-relative file paths.

    2026-09-08 bug fix: AETHERSDR-PORTS.md and FREEDV-GUI-PROVENANCE.md
    both use a second first-cell shorthand for a header/source pair --
    two separate backtick-quoted tokens, comma-separated, where the
    second token is extension-only and shares the first token's
    basename (e.g. ``| `src/core/strip/ClientGate.h`, `.cpp` | ...``).
    The old code only stripped the OUTERMOST backtick characters of the
    whole cell, which left the inner backticks and comma embedded in a
    single bogus "path" that could never match a real src file -- every
    one of the 23+ rows already using this established format was
    silently unregistered as a result. Extract every backtick-quoted
    token in the cell instead and resolve extension-only tokens against
    the nearest preceding `src/...` token.
    """
    if not first_cell or first_cell.lower() in ("nereussdr file", "file"):
        return []

    tokens = re.findall(r"`([^`]+)`", first_cell)
    if not tokens:
        tokens = [first_cell.strip("`").strip()]

    paths: list[str] = []
    base = None
    for tok in tokens:
        tok = tok.strip()
        if not tok:
            continue
        m = _BRACE_PAIR.match(tok)
        if m:
            paths.append(f"{m.group(1)}.h")
            paths.append(f"{m.group(1)}.cpp")
            base = m.group(1)
            continue
        if tok.startswith("."):
            # Extension-only shorthand referring to the previous token's
            # basename (e.g. the "`.cpp`" in the example above). Silently
            # ignored if there was no preceding src/ token to anchor it to.
            if base:
                paths.append(f"{base}{tok}")
            continue
        # 2026-09-21: THETIS-PROVENANCE.md registers tests
        # (`tests/tst_*.cpp`, unquoted first cell) and WDSP-PROVENANCE.md
        # the vendored tree under `third_party/wdsp/src/`; both were
        # dropped here by the `src/` prefix test, so every registered
        # test and WDSP file was flagged the moment a PR touched it
        # (PR #42, 53 files, all of them already in a table).
        if _REGISTERED_PREFIX.match(tok):
            paths.append(tok)
            base = re.sub(r"\.[^./]+$", "", tok)
    return paths


def parse_provenance_paths(*doc_paths: Path) -> set[str]:
    """Return union of *first-column* file paths listed in provenance tables.

    All the docs use the same markdown-table convention: the first cell
    of each data row is the registered Longpath file path. Counterpart /
    source / prose cells sometimes contain `src/...` strings too (e.g.
    reconciliation cites AetherSDR upstream paths that happen to share a
    filename with a Longpath file), so we MUST NOT pull paths from
    anywhere else in the row -- doing so allowlists files that aren't
    actually registered and creates a false-negative loophole for future
    ports.
    """
    paths: set[str] = set()
    for doc in doc_paths:
        if not doc.is_file():
            continue
        for line in doc.read_text(encoding="utf-8").splitlines():
            cells = _split_row(line)
            if not cells:
                continue
            paths.update(_resolve_first_cell(cells[0]))
    return paths


def aethersdr_port_paths(md_path: Path) -> set[str]:
    """Rows of AETHERSDR-PORTS.md that carry a real derivation claim.

    Narrower than `parse_provenance_paths` on the same file, and for the
    same reason the reconciliation doc is read Bucket-A-only: a row whose
    *second* column names no upstream source is registered for
    bookkeeping, not because anything was ported. Four rows say so in as
    many words -- `tests/tst_spe_verbindung.cpp` and
    `src/gui/setup/SpePage.{h,cpp}` ("Kein Port: AetherSDR hat fuer
    SpeConnection keinen Pruefstand"), `StripGraphics.h` and
    `EqPalette.h` ("(none -- ...)"). Counting those as
    `aethersdr-port` would demand an AetherSDR attribution header on
    Longpath-original files, i.e. a false claim, which is worse than the
    miscount this function exists to fix.
    """
    if not md_path.is_file():
        return set()
    paths: set[str] = set()
    for line in md_path.read_text(encoding="utf-8").splitlines():
        cells = _split_row(line)
        if not cells or len(cells) < 2:
            continue
        source = cells[1].strip().strip("`").strip()
        if not source or source.lower().startswith(_NO_SOURCE_PREFIXES):
            continue
        paths.update(_resolve_first_cell(cells[0]))
    return paths
