#!/usr/bin/env python3
"""Extract a GameCube disc image into the layout the port expects.

    tools/extract-disc.py "Mario Smash Football.iso" data/G4QP01

writes <out>/sys/{boot.bin,bi2.bin,apploader.img,main.dol,fst.bin} and
<out>/files/... exactly as dtk's `disc extract` would, so the port's
STRIKERS_DATA can point at <out>/files and DVDGetCurrentDiskID can read
<out>/sys/boot.bin. The output directory is named for the game id read from
the image, and refuses to unpack one region's disc over another's directory.

Plain GCM images only, no CISO, no GCZ, no NKit. Every field in the header
and the FST is big-endian, as everything on the disc is.
"""
import os
import struct
import sys


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def refuse_link(iso, path):
    if os.path.islink(path):
        sys.exit(f"{iso}: refusing to write through a symlink at {path}")


def open_new(iso, path):
    """A file to write, never through a symlink: O_NOFOLLOW where the host has it, a check where
    it does not."""
    refuse_link(iso, path)
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
    flags |= getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0)
    return os.fdopen(os.open(path, flags, 0o644), "wb")


def make_dir(iso, path):
    refuse_link(iso, path)
    os.makedirs(path, exist_ok=True)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    iso, out = sys.argv[1], sys.argv[2]
    image_size = os.path.getsize(iso)
    with open(iso, "rb") as f:
        head = f.read(0x2440)
        if len(head) != 0x2440 or be32(head, 0x1C) != 0xC2339F3D:
            sys.exit(f"{iso}: not a GameCube disc image (no magic at 0x1C)")
        game_id = head[:6].decode("ascii")
        want = os.path.basename(os.path.normpath(out))
        if want != game_id:
            sys.exit(f"{iso} is {game_id}; refusing to unpack it into {out}")
        # Outside the f-string: a backslash inside one is a syntax error before Python 3.12.
        title = head[0x20:0x60].rstrip(b"\0").decode("ascii", "replace")
        print(f"{game_id}: {title}")

        dol_off, fst_off, fst_size = be32(head, 0x420), be32(head, 0x424), be32(head, 0x428)

        # Everything is read and checked before anything is written, so a bad
        # image leaves nothing behind rather than half a tree.
        def span(what, off, size):
            if off + size > image_size:
                sys.exit(f"{iso}: {what} runs past the end of the image "
                         f"({off:#x} + {size:#x} > {image_size:#x})")

        # Apploader: 0x20-byte header carrying the size and the trailer size.
        f.seek(0x2440)
        ah = f.read(0x20)
        asize, atrail = be32(ah, 0x14), be32(ah, 0x18)
        span("the apploader", 0x2440, 0x20 + asize + atrail)

        # DOL: its size is the furthest any of its 18 sections reaches.
        span("the DOL header", dol_off, 0x100)
        f.seek(dol_off)
        dh = f.read(0x100)
        dol_size = 0x100
        for i in range(18):
            off, size = be32(dh, i * 4), be32(dh, 0x90 + i * 4)
            if size:
                dol_size = max(dol_size, off + size)
        span("the DOL", dol_off, dol_size)

        span("the file table", fst_off, fst_size)
        f.seek(fst_off)
        fst = f.read(fst_size)

        nent = be32(fst, 8) if len(fst) >= 12 else 0
        if nent < 1 or nent * 12 > len(fst):
            sys.exit(f"{iso}: FST claims {nent} entries in {len(fst)} bytes")
        strtab = nent * 12

        def name(i):
            o = be32(fst, i * 12) & 0xFFFFFF
            start = strtab + o
            e = fst.find(b"\0", start) if start < len(fst) else -1
            if e < 0:
                sys.exit(f"{iso}: FST entry {i} names a string outside the table")
            n = fst[start:e].decode("shift_jis", "replace")
            # One path component and nothing else: the FST decides where the bytes go, and a
            # name of "../x" is a write outside <out>.
            if n in ("", ".", "..") or "/" in n or "\\" in n:
                sys.exit(f"{iso}: FST entry {i} is not a plain file name: {n!r}")
            return n

        # Walk the tree: a directory entry's "size" is the index one past its last child.
        plan = []
        stack = [(1, nent, os.path.join(out, "files"))]
        i = 1
        while i < nent:
            while stack and i >= stack[-1][1]:
                stack.pop()
            parent = stack[-1][2]
            flags = fst[i * 12]
            if flags & 1:
                last = be32(fst, i * 12 + 8)
                # Against the enclosing directory's end, not only the table's: a child claiming
                # entries past its parent would take the parent's later files with it.
                if last <= i or last > stack[-1][1]:
                    sys.exit(f"{iso}: FST directory entry {i} ends at {last}, "
                             f"past its parent's end at {stack[-1][1]}")
                d = os.path.join(parent, name(i))
                plan.append(("dir", d, 0, 0))
                stack.append((i, last, d))
                i += 1
                continue
            off, size = be32(fst, i * 12 + 4), be32(fst, i * 12 + 8)
            span(f"FST entry {i}", off, size)
            plan.append(("file", os.path.join(parent, name(i)), off, size))
            i += 1

        sysdir = os.path.join(out, "sys")
        make_dir(iso, sysdir)
        with open_new(iso, os.path.join(sysdir, "boot.bin")) as o:
            o.write(head[:0x440])
        with open_new(iso, os.path.join(sysdir, "bi2.bin")) as o:
            o.write(head[0x440:0x2440])
        f.seek(0x2440)
        with open_new(iso, os.path.join(sysdir, "apploader.img")) as o:
            o.write(f.read(0x20 + asize + atrail))
        f.seek(dol_off)
        with open_new(iso, os.path.join(sysdir, "main.dol")) as o:
            o.write(f.read(dol_size))
        with open_new(iso, os.path.join(sysdir, "fst.bin")) as o:
            o.write(fst)

        nfiles, nbytes = 0, 0
        make_dir(iso, os.path.join(out, "files"))
        for kind, path, off, size in plan:
            if kind == "dir":
                make_dir(iso, path)
                continue
            f.seek(off)
            with open_new(iso, path) as o:
                left = size
                while left:
                    chunk = f.read(min(left, 1 << 24))
                    if not chunk:
                        sys.exit(f"{iso}: truncated at {off + size - left:#x}")
                    o.write(chunk)
                    left -= len(chunk)
            nfiles += 1
            nbytes += size
    print(f"{nfiles} files, {nbytes} bytes -> {out}/files; sys/ has boot.bin, bi2.bin, "
          f"apploader.img ({0x20 + asize + atrail} bytes), main.dol ({dol_size} bytes), fst.bin")


if __name__ == "__main__":
    main()
