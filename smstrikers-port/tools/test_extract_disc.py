#!/usr/bin/env python3
"""Malformed disc images against tools/extract-disc.py: the FST decides where every byte goes,
so each case is a disc built in a temporary directory with one field wrong, and the check is that
the script refuses it and nothing lands where it should not.

    python3 tools/test_extract_disc.py
"""

import os
import pathlib
import struct
import subprocess
import sys
import tempfile

TOOLS = pathlib.Path(__file__).resolve().parent
SCRIPT = TOOLS / "extract-disc.py"
PAYLOAD = b"strikers"


def be32(v):
    return struct.pack(">I", v)


def fst_bytes(entries):
    """entries: (is_dir, name, a, b), a/b being offset/size for a file and parent/end for a
    directory. The root comes first, as on a disc."""
    names = b""
    table = b""
    offsets = []
    for _is_dir, name, _a, _b in entries:
        offsets.append(len(names))
        names += name.encode("ascii") + b"\0"
    for (is_dir, _name, a, b), o in zip(entries, offsets):
        table += bytes([1 if is_dir else 0]) + o.to_bytes(3, "big") + be32(a) + be32(b)
    return table + names


def write_disc(path, entries, fst_patch=None):
    """A minimal GCM: header, empty apploader, a 0x100-byte DOL of nothing, the FST, then PAYLOAD."""
    head = bytearray(0x2440)
    head[:6] = b"G4QE01"
    head[0x1C:0x20] = be32(0xC2339F3D)
    head[0x20:0x24] = b"test"
    dol_off = 0x2460
    fst_off = dol_off + 0x100
    # The payload sits after the FST, at fst_off + len(fst), which every case computes the same way.
    fst = fst_bytes(entries)
    if fst_patch:
        fst = fst_patch(fst)
    head[0x420:0x424] = be32(dol_off)
    head[0x424:0x428] = be32(fst_off)
    head[0x428:0x42C] = be32(len(fst))
    path.write_bytes(bytes(head) + bytes(0x20) + bytes(0x100) + fst + PAYLOAD)
    return fst_off + len(fst)


def payload_offset(entries):
    return 0x2460 + 0x100 + len(fst_bytes(entries))


def run(iso, out):
    return subprocess.run([sys.executable, str(SCRIPT), str(iso), str(out)],
                          capture_output=True, text=True)


def main():
    failures = 0
    checked = 0

    def check(ok, what, detail=""):
        nonlocal failures, checked
        checked += 1
        if not ok:
            failures += 1
            print(f"    FAIL {what}" + (f"\n         {detail}" if detail else ""))

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)

        root = [(True, "", 0, 4)]
        entries = root + [(False, "ok.bin", 0, len(PAYLOAD)),
                          (True, "d", 0, 4), (False, "in.bin", 0, len(PAYLOAD))]
        off = payload_offset(entries)
        entries = [(d, n, off if (not d and a == 0) else a, b) for d, n, a, b in entries]
        iso = tmp / "good.iso"
        write_disc(iso, entries)
        out = tmp / "good" / "G4QE01"
        r = run(iso, out)
        check(r.returncode == 0, "a well-formed disc extracts", r.stderr.strip())
        check((out / "files" / "ok.bin").read_bytes() == PAYLOAD if r.returncode == 0 else False,
              "a file's bytes come out as they went in")
        check((out / "files" / "d" / "in.bin").exists() if r.returncode == 0 else False,
              "a directory's child lands inside it")
        check((out / "sys" / "boot.bin").exists() if r.returncode == 0 else False,
              "sys/boot.bin is written, which is what the port reads the region from")

        for bad in ("../escape.txt", "a/b.bin", "..", "."):
            entries = root[:1] + [(False, bad, 0, len(PAYLOAD))]
            entries[0] = (True, "", 0, len(entries))
            off = payload_offset(entries)
            entries[1] = (False, bad, off, len(PAYLOAD))
            iso = tmp / "bad.iso"
            write_disc(iso, entries)
            out = tmp / "bad" / "G4QE01"
            r = run(iso, out)
            check(r.returncode != 0, f"an entry named {bad!r} is refused", r.stderr.strip())
            check("not a plain file name" in r.stderr, f"...and the message says why for {bad!r}",
                  r.stderr.strip())
            # `../escape.txt` from files/ lands beside files/, one level up.
            check(not (out / "escape.txt").exists() and not (tmp / "bad" / "escape.txt").exists(),
                  f"...and nothing was written outside files/ for {bad!r}")

        entries = [(True, "", 0, 2), (True, "d", 0, 99)]
        iso = tmp / "dirend.iso"
        write_disc(iso, entries)
        r = run(iso, tmp / "dirend" / "G4QE01")
        check(r.returncode != 0 and "past its parent" in r.stderr,
              "a directory ending past the table is refused", r.stderr.strip())

        entries = [(True, "", 0, 5), (True, "a", 0, 3), (True, "b", 1, 5),
                   (False, "x.bin", 0, len(PAYLOAD)), (False, "y.bin", 0, len(PAYLOAD))]
        off = payload_offset(entries)
        entries = [(d, n, off if (not d and a == 0) else a, b) for d, n, a, b in entries]
        iso = tmp / "nest.iso"
        write_disc(iso, entries)
        r = run(iso, tmp / "nest" / "G4QE01")
        check(r.returncode != 0 and "past its parent" in r.stderr,
              "a directory ending past its parent is refused", r.stderr.strip())

        entries = [(True, "", 0, 2), (False, "x", 0, 1)]

        def break_name(fst):
            return fst[:12] + bytes([0]) + (0xFFFFFF).to_bytes(3, "big") + fst[16:]

        iso = tmp / "nameoff.iso"
        write_disc(iso, entries, fst_patch=break_name)
        r = run(iso, tmp / "nameoff" / "G4QE01")
        check(r.returncode != 0 and "outside the table" in r.stderr,
              "a name outside the string table is refused", r.stderr.strip())

        entries = [(True, "", 0, 3), (False, "ok.bin", 0, len(PAYLOAD)), (False, "../late.txt", 0, 1)]
        off = payload_offset(entries)
        entries[1] = (False, "ok.bin", off, len(PAYLOAD))
        entries[2] = (False, "../late.txt", off, 1)
        iso = tmp / "late.iso"
        write_disc(iso, entries)
        out = tmp / "late" / "G4QE01"
        r = run(iso, out)
        check(r.returncode != 0, "a bad entry after a good one is refused", r.stderr.strip())
        check(not (out / "files").exists() and not (out / "sys").exists(),
              "...and nothing was written before the refusal")

        entries = [(True, "", 0, 2), (False, "big.bin", 0, 1 << 20)]
        off = payload_offset(entries)
        entries[1] = (False, "big.bin", off, 1 << 20)
        iso = tmp / "span.iso"
        write_disc(iso, entries)
        r = run(iso, tmp / "span" / "G4QE01")
        check(r.returncode != 0 and "runs past the end" in r.stderr,
              "a file past the end of the image is refused", r.stderr.strip())

        try:
            victim = tmp / "victim.txt"
            victim.write_bytes(b"do not touch")
            out = tmp / "sysln" / "G4QE01"
            (out / "sys").mkdir(parents=True)
            os.symlink(victim, out / "sys" / "boot.bin")
        except OSError:
            print("    (symlinks unavailable here; the sys symlink case was not run)")
        else:
            entries = [(True, "", 0, 2), (False, "ok.bin", 0, len(PAYLOAD))]
            off = payload_offset(entries)
            entries[1] = (False, "ok.bin", off, len(PAYLOAD))
            iso = tmp / "sysln.iso"
            write_disc(iso, entries)
            r = run(iso, out)
            check(r.returncode != 0 and "symlink" in r.stderr,
                  "a symlink at sys/boot.bin is refused", r.stderr.strip())
            check(victim.read_bytes() == b"do not touch",
                  "...and the file it pointed at is untouched")

        try:
            elsewhere = tmp / "elsewhere"
            elsewhere.mkdir()
            out = tmp / "link" / "G4QE01"
            out.mkdir(parents=True)
            os.symlink(elsewhere, out / "files")
        except OSError:
            print("    (symlinks unavailable here; the symlink case was not run)")
        else:
            entries = [(True, "", 0, 2), (False, "ok.bin", 0, len(PAYLOAD))]
            off = payload_offset(entries)
            entries[1] = (False, "ok.bin", off, len(PAYLOAD))
            iso = tmp / "link.iso"
            write_disc(iso, entries)
            r = run(iso, out)
            check(r.returncode != 0 and "symlink" in r.stderr,
                  "a symlink at files/ is refused rather than written through", r.stderr.strip())
            check(not (elsewhere / "ok.bin").exists(),
                  "...and nothing reached the symlink's target")

    if failures:
        print(f"\n{failures} of {checked} cases failed")
        return 1
    print(f"ok: {checked} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
