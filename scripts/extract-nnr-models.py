#!/usr/bin/env python3
"""Extract the raw WDSPNN tensor blob embedded as a C byte array literal
in an nnr_model_N.c file (WDSP 2.10, Warren Pratt NR0V) into a standalone
.bin file loadable via nnet.c's nnio_open() / SetNNRModelPathSlot().
"""
import re
import sys

def extract(src_path, dst_path, expected_size):
    with open(src_path, "r") as f:
        text = f.read()

    m = re.search(r"_data\[\]\s*=\s*\{(.*?)\};", text, re.DOTALL)
    if not m:
        raise SystemExit(f"could not find array literal in {src_path}")

    body = m.group(1)
    tokens = re.findall(r"0x([0-9a-fA-F]{2})", body)
    data = bytes(int(t, 16) for t in tokens)

    if len(data) != expected_size:
        raise SystemExit(
            f"size mismatch: extracted {len(data)} bytes, expected {expected_size}"
        )

    if data[:8] != b"WDSPNN\x00\x00":
        raise SystemExit(f"bad magic: {data[:8]!r}")

    with open(dst_path, "wb") as f:
        f.write(data)

    print(f"{src_path} -> {dst_path}: {len(data)} bytes, magic OK")


if __name__ == "__main__":
    extract(sys.argv[1], sys.argv[2], int(sys.argv[3]))
