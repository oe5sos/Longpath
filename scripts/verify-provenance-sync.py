#!/usr/bin/env python3
"""Verify that provenance declarations stay in sync with source tree.

This script catches two classes of drift:
  1. Files in src/ or tests/ that contain "Ported from" or "From Thetis"
     markers in the first 120 lines but are NOT listed in
     docs/attribution/THETIS-PROVENANCE.md (new ports that missed
     PROVENANCE update).
  2. Rows in PROVENANCE.md that point to files no longer present on disk
     (stale rows after renames/deletes).

Exit 0 on clean, 1 on any discrepancy.
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
SRC_DIR = REPO / "src"
TESTS_DIR = REPO / "tests"
HEADER_WINDOW = 120

# Markers that indicate a file is derived from Thetis (not just any "Ported from")
# These mark actual Thetis ports that require PROVENANCE entries
DERIVATION_MARKERS = ["Ported from Thetis", "From Thetis"]

# Per-file escape hatch — mirrors scripts/check-new-ports.py. Files that
# genuinely cite Thetis without being ports (e.g. test fixtures asserting
# parity with a Thetis value, NereusSDR-original POD aggregators with a
# single default-value reference) can declare themselves exempt with
# `// no-port-check: <reason>` in the first 120 lines.
NO_PORT_CHECK_MARKER = "no-port-check:"


def find_ported_files():
    """Scan src/ and tests/ for files with derivation markers.

    Returns set of repo-relative paths (str) that contain a Thetis marker
    in the first 120 lines.
    """
    ported = set()

    # Collect all .cpp and .h files
    for src_dir in [SRC_DIR, TESTS_DIR]:
        if not src_dir.exists():
            continue
        for filepath in src_dir.rglob("*"):
            if filepath.suffix not in (".cpp", ".h"):
                continue
            if not filepath.is_file():
                continue

            # Read first 120 lines
            try:
                head = "\n".join(
                    filepath.read_text(errors="replace").splitlines()[:HEADER_WINDOW]
                )
            except Exception:
                continue

            # Honor the no-port-check: escape hatch for genuine
            # false-positives (mirrors check-new-ports.py behavior).
            if NO_PORT_CHECK_MARKER in head:
                continue

            # Check for derivation marker
            for marker in DERIVATION_MARKERS:
                if marker in head:
                    rel = str(filepath.relative_to(REPO))
                    ported.add(rel)
                    break

    return ported


def parse_provenance_paths():
    """Parse THETIS-PROVENANCE.md and extract all declared file paths.

    Returns tuple of (declared_paths, independent_paths, path_linenos):
      - declared_paths: set of paths from derivative tables (column 0)
      - independent_paths: set of paths from "Independently implemented"
        section (column 1, index 1)
      - path_linenos: dict[path -> lineno in PROVENANCE.md] for orphan
        reporting

    Files in independent_paths are expected to NOT have "Ported from"
    markers, so they should not be flagged as missing from ported_files.

    Row shape, not the section a row happens to sit under, decides how a
    row is parsed: a derivative-table row has the NereusSDR path in column
    0 (6 columns: file | Thetis source | lines | type | variant | notes);
    an "Independently implemented" row has it in column 1 (3 columns:
    resemblance | file | basis). Thetis-side paths always read
    "Project Files/Source/..." or similar, never "src/"/"tests/", so the
    two shapes never collide. Deciding per row — instead of flipping a
    `in_independent_section` flag on the section heading and never
    flipping it back — means a row keeps parsing correctly regardless of
    which section it lives under, and a stray or misplaced row no longer
    silently drags every later row into the wrong column (see the
    src/core/audio/{QsoRecorder,VoiceKeyer,WavFile,WavPlayer,WavRecorder,
    IqRecorder}.{h,cpp} + QsoRecorderApplet.{h,cpp} cluster, moved back
    into the derivative table 2026-09-08 after this exact failure mode).
    """
    declared = set()
    independent = set()
    path_linenos: dict[str, int] = {}
    withdrawn: set[str] = set()
    withdrawn_without_commit: list[tuple[str, int]] = []

    if not PROVENANCE.is_file():
        return declared, independent, path_linenos

    text = PROVENANCE.read_text()
    in_independent_section = False

    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()

        # Still tracked for the independent-row branch below (it disambiguates
        # a genuine 3-column independent row from a stray 2-column table
        # elsewhere in the doc) — but, unlike before, it is never the sole
        # signal for a declared row.
        if "Independently implemented" in line:
            in_independent_section = True
            continue

        # Skip non-data rows
        if not line.startswith("|") or line.startswith("|---"):
            continue

        # Parse cells
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells or len(cells) < 2:
            continue

        # Skip rows that are all dashes or separators
        all_cells_text = " ".join(cells)
        if "---" in all_cells_text or not cells[0] or cells[0] == "---":
            continue

        first_cell = cells[0]
        second_cell = cells[1]
        first_is_path = first_cell.startswith("src/") or first_cell.startswith("tests/")
        second_is_path = second_cell.startswith("src/") or second_cell.startswith("tests/")

        if first_is_path:
            # Derivative-table row: NereusSDR file in column 0 — regardless
            # of which section it is (mis)placed under.
            candidate = first_cell.replace("`", "").strip()
            if not candidate:
                continue

            # ── Zurueckgezogene Zeilen ───────────────────────────────
            #
            # Eine geloeschte Datei laesst sich nicht einfach aus der
            # Tabelle streichen: dann waere der Port nicht mehr
            # auffindbar. Anweisung des Betreibers, 2026-08-18: „Die
            # PROVENANCE-Zeile zurueckziehen, mit einer Notiz, unter
            # welchem Commit die Datei zuletzt stand — damit der Port
            # auffindbar bleibt."
            #
            # Eine Zeile, die ZURUECKGEZOGEN sagt, wird von der
            # Auf-der-Platte-Pruefung ausgenommen — aber NUR, wenn sie
            # auch einen Commit nennt. Eine Ruecknahme ohne Fundstelle
            # ist keine Ruecknahme, sondern ein Verlust mit Fussnote,
            # und faellt darum durch.
            row = " ".join(cells)
            if "ZURUECKGEZOGEN" in row:
                if not re.search(r"`[0-9a-f]{7,40}`", row):
                    withdrawn_without_commit.append((candidate, lineno))
                withdrawn.add(candidate)
                continue

            declared.add(candidate)
            path_linenos.setdefault(candidate, lineno)
        elif in_independent_section and second_is_path:
            # "Independently implemented" row: NereusSDR file in column 1.
            candidate = second_cell.replace("`", "").strip()
            if candidate:
                independent.add(candidate)
                path_linenos.setdefault(candidate, lineno)
        # else: header row or unrelated table — skip.

    return declared, independent, path_linenos, withdrawn, withdrawn_without_commit


def main():
    # Find all ported files
    ported_files = find_ported_files()

    # Find all declared files (both derivative and independent sections)
    (declared_files, independent_files, path_linenos,
     withdrawn, withdrawn_without_commit) = parse_provenance_paths()

    # Files that are expected to be in PROVENANCE (either derived or independent)
    all_expected = declared_files | independent_files

    failures = 0

    # Check 1: ported files not in PROVENANCE
    missing_from_provenance = ported_files - all_expected - withdrawn
    for path in sorted(missing_from_provenance):
        failures += 1
        print(f"FAIL {path} — has 'Ported from' marker but not in PROVENANCE.md")

    # Check 2: files listed in PROVENANCE that don't exist on disk
    #          (Note: we only check declared_files since independent files may not exist)
    missing_on_disk = {p for p in declared_files if not (REPO / p).is_file()}
    for path in sorted(missing_on_disk):
        failures += 1
        lineno = path_linenos.get(path, "?")
        print(f"FAIL THETIS-PROVENANCE.md:{lineno}  orphan row: {path!r} not on disk")

    # Check 3: eine Ruecknahme ohne Fundstelle ist keine Ruecknahme
    for path, lineno in withdrawn_without_commit:
        failures += 1
        print(f"FAIL THETIS-PROVENANCE.md:{lineno}  zurueckgezogene Zeile "
              f"{path!r} nennt keinen Commit — ohne `<sha>` ist der Port "
              f"nicht mehr auffindbar")

    # Check 4: eine zurueckgezogene Datei, die es doch noch gibt
    for path in sorted(withdrawn):
        if (REPO / path).is_file():
            failures += 1
            lineno = path_linenos.get(path, "?")
            print(f"FAIL THETIS-PROVENANCE.md:{lineno}  {path!r} ist als "
                  f"ZURUECKGEZOGEN gefuehrt, liegt aber auf der Platte")

    if withdrawn:
        print(f"  hinweis: {len(withdrawn)} zurueckgezogene Zeile(n), "
              f"je mit Commit-Fundstelle")

    # Summary
    total = len(ported_files)
    ok = total - len(missing_from_provenance)
    print(f"\n{ok}/{total} files pass sync check")

    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
