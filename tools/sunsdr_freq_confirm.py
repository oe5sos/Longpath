#!/usr/bin/env python3
"""sunsdr_freq_confirm.py  (NereusSDR/Longpath)

Confirms (or refutes) the candidate SunSDR2 QRP frequency-payload
encoding found 2026-08-27 — see
docs/architecture/2026-08-24-sunsdr-native-driver-design.md, "candidate
frequency-encoding formula found — source-grounded, band-plausible, but
NOT bench-confirmed".

Candidate formula (from ArtemisSDR's real sunsdr_send_freq_pkt(),
sunsdr.c:2259-2277 [@f8b01d25c5]): the frequency-set control frame
(opcode 0x08) carries `freqHz * 10` as an 8-byte little-endian integer
at payload offset 0 (packet byte 18, right after the 18-byte control
header).

Why this needs a real packet capture rather than a plain socket
listener: ExpertSDR2 talks to the radio over ordinary unicast UDP: a
third program on the same Mac cannot simply "overhear" that traffic by
binding a socket to the same port — UDP delivery is per-destination,
not promiscuous. This script uses tcpdump instead (the same technique
that produced every SunSDR capture referenced in the design doc), which
DOES see it, because it captures at the network-interface level.

Usage:
    1. Have ExpertSDR2 running and connected to the SunSDR2 QRP as
       normal — this script only WATCHES, it sends nothing.
    2. Run:  sudo python3 tools/sunsdr_freq_confirm.py [seconds]
       (default: 15 seconds if not given)
    3. While it's running, tune the VFO in ExpertSDR2 to ONE exact,
       deliberately-chosen frequency (write down exactly what you set
       it to — e.g. 14074000 for 14.074.000 Hz).
    4. The script prints every opcode-0x08 frame it sees, decoded via
       the candidate formula, live, as it happens.
    5. Compare the printed value against the frequency you actually
       set. If they match exactly, the formula is confirmed and
       setReceiverFrequency() can be wired for real. If it's off by a
       consistent ratio, the scale factor differs from DX/PRO's 10 for
       this radio specifically. If it's unrelated noise, the hypothesis
       is wrong.

Needs sudo because tcpdump needs raw-socket access to capture traffic
this process is not itself a party to. Nothing is sent to the radio —
this is a pure, read-only observer.
"""

import subprocess
import struct
import sys
import time

CONTROL_PORT = 50001
FREQ_SCALE = 10


def find_default_interface():
    """Always 'any' -- macOS's -i any captures every interface at once
    (PKTAP-wrapped, but that only adds bytes ahead of the real payload;
    the marker-search parsing below finds the SunSDR magic regardless
    of how much extra header precedes it, so this doesn't need special
    handling). Found necessary live, 2026-08-27: an earlier version of
    this script guessed a single interface via the default route
    (typically en0), and captured ZERO packets -- the QRP traffic was
    on a different interface entirely. This project's own earlier
    bench captures always used `-i any` for exactly this reason (see
    design doc's original capture commands); guessing a specific
    interface was an unnecessary complication this rediscovered the
    hard way."""
    return "any"


def decode_freq_payload(payload8: bytes):
    """payload8: exactly 8 bytes, little-endian freqHz*FREQ_SCALE."""
    scaled = int.from_bytes(payload8, byteorder="little", signed=False)
    return scaled / FREQ_SCALE


def parse_udp_payload(raw: bytes):
    """Very small, dependency-free UDP/IP payload extractor for
    tcpdump's raw -w output read back via -r ... -x-style hex is fragile
    across platforms, so this script instead runs tcpdump with -l -U -x
    and parses ITS printed hex dump directly — no pcap library, no
    scapy, works with the same tcpdump every earlier SunSDR capture in
    this project already used."""
    return raw


def main():
    seconds = 15
    if len(sys.argv) > 1:
        try:
            seconds = int(sys.argv[1])
        except ValueError:
            print(f"Ignoring non-numeric seconds argument {sys.argv[1]!r}, using default 15")

    iface = find_default_interface()
    print("SunSDR frequency-formula confirmation — read-only, sends nothing")
    print("-" * 70)
    print(f"Listening on interface '{iface}', UDP port {CONTROL_PORT}, for {seconds}s.")
    print("Tune the VFO in ExpertSDR2 NOW to one exact, known frequency.")
    print("(Nothing is sent by this script — it only watches.)")
    print()

    # -l: line-buffered stdout so we see frames as they arrive, not only
    # at the end. -x: hex dump of packet contents (not -X, to keep the
    # output simpler to parse — pure hex, no ASCII side column).
    # -n: don't resolve hostnames (faster, avoids DNS side effects).
    cmd = [
        "tcpdump", "-i", iface, "-l", "-x", "-n",
        f"udp port {CONTROL_PORT}",
    ]

    try:
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, bufsize=1,
        )
    except FileNotFoundError:
        print("FEHLER: tcpdump nicht gefunden. Auf macOS gehört es zum System "
              "(kein separates Paket noetig).")
        sys.exit(1)

    deadline = time.monotonic() + seconds
    current_packet_hex = []
    frames_seen = 0
    freq_frames_seen = 0

    def flush_packet():
        nonlocal frames_seen, freq_frames_seen
        if not current_packet_hex:
            return
        frames_seen += 1
        all_bytes = bytes.fromhex("".join(current_packet_hex))
        # Find the SunSDR magic (0x03 0xFF, opcode 0x08) anywhere in the
        # captured frame -- tcpdump's hex dump includes Ethernet/IP/UDP
        # headers ahead of the actual UDP payload, and their exact
        # length varies (loopback vs. real NIC, IPv4 vs. IPv6, VLAN
        # tags), so scanning for the known 3-byte magic+opcode marker is
        # far more robust than assuming one fixed header length.
        marker = bytes([0x03, 0xFF, 0x08])
        idx = all_bytes.find(marker)
        if idx == -1:
            return
        # Payload starts at header byte 18. The marker is 3 bytes
        # (header bytes [0:3]), so payload_start = idx + 18, not +16 --
        # an off-by-2 here was a real bug, found live 2026-08-27: it
        # read 2 bytes too early, landing partly inside the header's
        # own already-documented non-zero tail (bytes 14-17) instead of
        # the true payload, producing a nonsense multi-hundred-GHz
        # "frequency". Confirmed against the one known-good reference
        # frame in this project's own code
        # (SunSdrRadioConnection::replayedFrequencyFrameForTest(),
        # design doc "the exact frame in code"): marker at index 0,
        # true payload `6ce0780800000000` at index 18, not 16.
        payload_start = idx + 18
        payload = all_bytes[payload_start:payload_start + 8]
        if len(payload) != 8:
            return
        freq_frames_seen += 1
        freq_hz = decode_freq_payload(payload)
        print(f"  [0x08 frame #{freq_frames_seen}] payload={payload.hex()}  "
              f"=> candidate freq = {freq_hz:,.1f} Hz = {freq_hz/1e6:.6f} MHz")

    try:
        while time.monotonic() < deadline:
            line = proc.stdout.readline()
            if not line:
                break
            line = line.rstrip("\n")
            if line.startswith("\t0x") or (line and line[0:1].isspace() and "0x" in line[:12]):
                # tcpdump -x hex-dump continuation line, e.g.
                # "\t0x0000:  4500 003a ..."
                hexpart = line.split(":", 1)[-1] if ":" in line else line
                current_packet_hex.append(hexpart.replace(" ", ""))
            else:
                # A new packet's summary line -- flush the previous one.
                flush_packet()
                current_packet_hex = []
        flush_packet()
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()

    print()
    print("-" * 70)
    print(f"Done. {frames_seen} packet(s) captured, {freq_frames_seen} "
          f"opcode-0x08 (frequency) frame(s) decoded above.")
    if freq_frames_seen == 0:
        print("No frequency frames seen -- try tuning the VFO again while "
              "this script is running, or increase the seconds argument.")


if __name__ == "__main__":
    main()
