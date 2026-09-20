#!/usr/bin/env python3
"""Lay a boot sector and a kernel .COM onto a 360K floppy image.

The host counterpart of /bin/mkboot, the way mkfs.py is of /bin/mkfs: the
same three placements, so the floppy a reader builds here and the one the
target builds with `drun mkimg` are the same bytes.

    [ boot sector, 512 B ][ kernel .COM, from sector 1 ][ zero fill ]

360K floppy = 2 heads * 40 tracks * 9 sectors * 512 = 368640 bytes.  There is
no filesystem on it: the boot sector finds the kernel by LBA, not by name, so
the kernel's placement at sector 1 IS the file format.

Two things are refused rather than fixed up, both because the symptom
otherwise arrives long after the cause, in the BIOS:

  - a boot sector that is not exactly 512 bytes ending in 55 AA.  A file that
    needs the signature added is not a boot sector.
  - a kernel longer than the loader reads.  It occupies sectors 1..n, the
    boot sector reads 1..119, and a kernel one sector over boots with its
    tail missing -- a wild jump with nothing to diagnose it at run time.

Usage:
    python mkboot.py BOOT KERNEL IMAGE [--sectors N] [--maxsect N]
"""
import argparse
import sys
from pathlib import Path

BLOCK = 512
HEADS, TRACKS, SECTORS = 2, 40, 9
DEF_NSECT = HEADS * TRACKS * SECTORS            # 720 sectors = 368640 bytes
DEF_MAXSECT = 119                               # what the boot sector reads
SIGNATURE = b"\x55\xAA"


class MkbootError(Exception):
    pass


def build(bootpath, kernpath, image, nsect=DEF_NSECT, maxsect=DEF_MAXSECT,
          verbose=True):
    boot = Path(bootpath).read_bytes()
    kern = Path(kernpath).read_bytes()

    if len(boot) != BLOCK:
        raise MkbootError("%s is %d bytes, a boot sector is %d"
                          % (bootpath, len(boot), BLOCK))
    if boot[BLOCK - 2:] != SIGNATURE:
        raise MkbootError("%s does not end in 55 AA" % bootpath)

    used = (len(kern) + BLOCK - 1) // BLOCK
    if used > maxsect:
        raise MkbootError("%s needs %d sectors but the boot sector reads %d"
                          % (kernpath, used, maxsect))
    if BLOCK + len(kern) > nsect * BLOCK:
        raise MkbootError("%s does not fit in %d sectors" % (kernpath, nsect))

    img = bytearray(nsect * BLOCK)
    img[0:BLOCK] = boot
    img[BLOCK:BLOCK + len(kern)] = kern
    Path(image).write_bytes(img)

    # Read it back rather than trust the write, as /bin/mkboot does -- there
    # the realistic failure is a short write into a filesystem with barely
    # room for 720 blocks, and checking costs nothing either way.
    back = Path(image).read_bytes()
    if len(back) != nsect * BLOCK or back[0:BLOCK] != boot \
            or back[BLOCK:BLOCK + len(kern)] != kern \
            or back[BLOCK + len(kern):].strip(b"\x00"):
        raise MkbootError("%s does not read back as written" % image)

    if verbose:
        fill = nsect * BLOCK - BLOCK - len(kern)
        print("%s: %s + %s" % (image, bootpath, kernpath))
        print("  floppy      : %d bytes (%d sectors)" % (nsect * BLOCK, nsect))
        print("  boot sector : %d bytes at sector 0 (55 AA ok)" % BLOCK)
        print("  kernel      : %d bytes, sectors 1..%d (the boot sector "
              "reads 1..%d)" % (len(kern), used, maxsect))
        print("  zero fill   : %d bytes" % fill)
        print("  verify      : re-read and compared -- OK")
    return used


def main():
    ap = argparse.ArgumentParser(
        description="Lay a boot sector and a kernel .COM onto a floppy image.")
    ap.add_argument("boot", help="the 512-byte boot sector")
    ap.add_argument("kernel", help="the kernel .COM, laid from sector 1")
    ap.add_argument("image", help="output floppy image")
    ap.add_argument("--sectors", type=int, default=DEF_NSECT, metavar="N",
                    help="floppy size in sectors (default %d)" % DEF_NSECT)
    ap.add_argument("--maxsect", type=int, default=DEF_MAXSECT, metavar="N",
                    help="sectors the boot sector reads (default %d)"
                         % DEF_MAXSECT)
    args = ap.parse_args()

    try:
        build(args.boot, args.kernel, args.image, args.sectors, args.maxsect)
    except (MkbootError, OSError) as exc:
        sys.exit("mkboot: %s" % exc)


if __name__ == "__main__":
    main()
