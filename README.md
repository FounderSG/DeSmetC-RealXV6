# DeSmet C 3.03 — from 1987 binaries to self-hosting on RealXV6

The shipped 1987 binaries rebuild the whole DeSmet C toolchain from its own
source, drive that rebuild to its self-reproducing fixpoint, retarget it at
RealXV6, and build every program a RealXV6 filesystem carries.  Then the
target rebuilds that toolchain again, on itself, from the same source, and
lands on the same bytes.

## The DOS half

Mount `drive_c` as the DOS `C:` drive and type one word:

```
mount c /path/to/this/drive_c        (Linux, macOS)
mount c X:\path\to\this\drive_c      (Windows)
c:
cross
```

About half a minute later `C:\XCC` holds a cross compiler that turns a `.C`
into a RealXV6 a.out. Nothing to install, no config file — every bat sets its
own `PATH` and `DSINC`, which is also what decides *which* compiler does the
building.

Then one more word builds every program the image carries:

```
mkbin
```

Another minute, and `C:\XV6\BIN` holds all 41 of them as RealXV6 a.out —
`init`, the userland ported to K&R in `drive_c\XV6\USR`, the tool ports, and
(through `BUILD\XV6.BAT`) the toolchain itself relinked to run on the target.
**Nothing on the image arrives here as a binary.** The only binaries in this
tree are the 1987 ones under `DESMET` — `BIN`'s tools and `LIB`'s objects —
that the whole chain starts from; everything else is compiled from the source
beside it, by the toolchain `CROSS` has just driven to its own fixpoint.

## The RealXV6 half

The same toolchain, compiled to V6 a.out and run as ordinary target programs.
The source is on the image, under `/src`: `/src/rt` builds the shared runtime,
`/src/desmet` rebuilds `gen`, `c88`, `asm88`, `dbind`, `dlib88` and `d2a`,
and `/src/usr` rebuilds the other 35 — `init`, `sh`, `ed`, `ls` and the rest.
Each rebuilt binary is compared against the `/bin` binary that produced it —
the one `MKBIN` built on DOS from the same source, so the fixpoint closes
across the two hosts as well as on the target. Six `IDENTICAL` lines from
`mkall` and thirty-five from `mkusr` is every program on the image, rebuilt
by the image, with no host tool anywhere in the chain:

```
cd /src/rt     ; drun mkrt
cd /src/desmet ; drun mkall
cd /src/usr    ; drun mkusr
```

### What you need besides this tree

**A boot floppy, from the RealXV6 project, branch `vmm-experiment`.** That
branch supplies the kernel and the 360K boot image the target boots from; what
is here is the *filesystem* the machine then runs on, and the log under
`evidence/` was recorded against that floppy.

Also `qemu` and a Python 3 for `mkfs.py`.

### Building the filesystem image

`mkfs.py` is the one host-side program here; everything else runs on
DOS or on the target. `proto` describes the whole image, and its tokens point
back into this tree so that nothing is duplicated: `$RT` and `$BUILD` are
where `/src` is read from, and `$BIN` is what `MKBIN` just built.

```
python scripts/mkfs.py unix.img image/boot/proto \
    -D RT=drive_c/XV6/RT -D BUILD=drive_c/BUILD -D BIN=drive_c/XV6/BIN
python -c "open('unix.img','r+b').truncate(10321920)"
```

Run `MKBIN` first — `$BIN` is a directory this tree does not ship, and `mkfs`
says so by name if it is not there.

`mkfs` writes only the prefix it used, so the second line pads to the fixed
disk size the IDE drive is configured for. Timestamps are written as zero, so
the result is reproducible: the same tree always gives the same image.

Then boot the floppy from `vmm-experiment` with that image as the IDE disk:

```
qemu-system-i386 -nographic -display none -serial file:console.txt \
    -drive file=Unix360.img,format=raw,if=floppy \
    -drive file=unix.img,format=raw,if=ide -boot a
```

## What was checked, and what it printed

One run on the target, captured verbatim from the serial console.

| evidence | what it shows |
|---|---|
| `evidence/mkall.log` | **the toolchain rebuilds itself on RealXV6, at the fixpoint** |

`mkall` is the capstone: the compiler reproduces itself while running on the
system it targets.

What the log checks is the toolchain, and `MKBIN` reproduces every one of
those binaries byte for byte. So the `IDENTICAL` lines are about the `/bin`
this tree builds, not about a binary that came from somewhere else.

## How the port works

Each tool is **one K&R source that builds twice** — as a DOS program against
DeSmet's `CSTDIO`, and as a RealXV6 program against `CRT0`/`SYSCALL`/`DCOMPAT`.
`DCOMPAT.C` supplies the DOS-flavoured calls the tool sources expect on top of
V6 syscalls, `DCRT0.A` is the startup for the tool ports, `V6CHAIN.C` replaces
DeSmet's `chain()` between compiler passes, and `V6MISC.A` fills in the rest.
`D2A.C` follows the same arrangement, which is what lets the target produce
its own `.aout` with no host tool at all.
