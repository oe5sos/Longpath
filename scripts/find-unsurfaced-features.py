#!/usr/bin/env python3
# =================================================================
# scripts/find-unsurfaced-features.py  (NereusSDR)
# =================================================================
#
# Sucht Merkmale, die gebaut sind und an keiner Bedienflaeche haengen.
#
# WARUM ES DIESES SKRIPT GIBT. An zwei Tagen (2026-08-18 und -19) sind
# drei solche Faelle aufgefallen — die Modusgruppen, der Bandplan und
# Apollo/PennyLane. Alle drei habe ich VON HAND gefunden, beim Suchen
# nach etwas anderem. Das ist Glueck, kein Verfahren.
#
# WAS ES NICHT IST: ein Tor. Es meldet, es sperrt nicht. Die Regel
# „jedes Merkmal braucht eine Flaeche" hat berechtigte Ausnahmen —
# Codecs, Audio-Quellen, Transport-Arbeiter gehoeren nie in die
# Oberflaeche. Ein Tor, das staendig zu Unrecht rot wird, schaltet
# jemand ab, und dann schuetzt es gar nichts mehr.
#
# Aufruf:
#     python3 scripts/find-unsurfaced-features.py            # Bericht
#     python3 scripts/find-unsurfaced-features.py --setters  # zusaetzlich
#                                                            # nie gerufene Setter
#
# =================================================================
# Modification history (NereusSDR):
#   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
#                 KI-gestuetzt ueber Anthropic Claude (Cowork).
# =================================================================

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Verzeichnisse, in denen Merkmale mit Bedienflaeche leben.
FEATURE_DIRS = ["src/core", "src/models"]
UI_DIRS = ["src/gui"]

# Klassen, die BEWUSST keine Bedienflaeche haben. Jede Zeile mit Grund —
# eine Ausnahmeliste ohne Begruendung waechst, bis sie alles enthaelt.
EXEMPT = {
    # Protokoll-Codecs: uebersetzen Bytes, kennen keine Oberflaeche.
    "IP1Codec", "IP2Codec", "P1CodecStandard", "P1CodecHl2",
    "P1CodecRedPitaya", "P1CodecAnvelinaPro3", "P2CodecHermes",
    "P2CodecHermesII", "P2CodecOrionMkII", "P2CodecSaturn",
    # Audio-Innereien: Quellen, Senken, Ringe, Mischer.
    "IAudioBus", "AudioRingSpsc", "MasterMixer", "TxMicSource",
    "PcMicSource", "RadioMicSource", "VaxTxMicSource", "NullMicSource",
    "MicReorderBuffer", "QAudioSinkAdapter", "PipeWireStream",
    "PipeWireThreadLoop",
    # MMIO-Transporte: haengen an ihrer eigenen Einstellseite ueber die
    # Fabrik, nicht ueber den Klassennamen.
    "ITransportWorker", "UdpEndpointWorker", "SerialEndpointWorker",
    "TcpClientEndpointWorker", "TcpListenerEndpointWorker",
    # TCI-Innereien: Rahmen, Warteschlangen, Entpreller.
    "TciBinaryFrame", "TciSendQueue", "TciVfoCoalescer",
    # DSP-Filter hinter den NR-Knoepfen der VFO-Flagge.
    "DeepFilterFilter", "MacNRFilter",
    # RADE-Innereien.
    "RadeText", "RadeTxHpf80", "RadeTx48to16",
    # Rechenwege ohne eigene Anzeige.
    "PsFeedbackChannel", "WidebandFrameAccumulator", "CouplerZero",
    "FlexRadioDiscoveryBroadcaster", "FreeDVRadeReporterBridge",
    "RxDspWorker", "MeterModel", "ClientPhaseRotator",
}


def run(cmd):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True).stdout


def classes_in(dirs):
    found = []
    for d in dirs:
        for dirpath, _, files in os.walk(os.path.join(ROOT, d)):
            for f in files:
                if not f.endswith(".h"):
                    continue
                path = os.path.join(dirpath, f)
                rel = os.path.relpath(path, ROOT)
                text = open(path, encoding="utf-8", errors="ignore").read()
                for m in re.finditer(r"^class\s+(\w+)\s*(?::|\{)", text, re.M):
                    found.append((m.group(1), rel))
    return found


def unsurfaced():
    hits = []
    for cls, path in classes_in(FEATURE_DIRS):
        if cls in EXEMPT:
            continue
        used = run(["grep", "-rl", cls] + UI_DIRS).strip()
        if used:
            continue
        # Auch Tests zaehlen als Beleg, dass jemand das Merkmal benutzt —
        # aber nicht als Bedienflaeche. Sie werden getrennt gemeldet.
        tested = bool(run(["grep", "-rl", cls, "tests"]).strip())
        hits.append((cls, path, tested))
    return hits


def uncalled_setters():
    hits = []
    for cls, path in classes_in(FEATURE_DIRS):
        if cls in EXEMPT:
            continue
        text = open(os.path.join(ROOT, path), encoding="utf-8",
                    errors="ignore").read()
        for m in re.finditer(r"\bvoid\s+(set[A-Z]\w*)\s*\(", text):
            name = m.group(1)
            callers = run(["grep", "-rl", name + "(", "src"]).strip().splitlines()
            callers = [c for c in callers if not c.startswith(path.rsplit(".", 1)[0])]
            if not callers:
                hits.append((cls, name, path))
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--setters", action="store_true",
                    help="zusaetzlich nie gerufene oeffentliche Setter melden")
    args = ap.parse_args()

    print("Merkmale ohne Bedienflaeche")
    print("=" * 60)
    rows = unsurfaced()
    if not rows:
        print("  keine — jede Klasse ausserhalb der Ausnahmeliste kommt in "
              "src/gui vor")
    for cls, path, tested in rows:
        note = "hat Tests" if tested else "OHNE Tests"
        print(f"  {cls:32s} {path:46s} {note}")

    if args.setters:
        print()
        print("Oeffentliche Setter, die niemand ruft")
        print("=" * 60)
        srows = uncalled_setters()
        if not srows:
            print("  keine")
        for cls, name, path in srows:
            print(f"  {cls}::{name:34s} {path}")

    print()
    print(f"{len(rows)} Merkmal(e) ohne Flaeche. Das ist ein BERICHT, kein "
          "Urteil: pruefen, dann entweder anschliessen, ausbauen oder in "
          "die Ausnahmeliste mit Begruendung.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
