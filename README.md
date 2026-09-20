# DeSmet C 3.03 — from 1987 binaries to a system that rebuilds itself

**Stage 3.** Everything stage 2 has — the shipped 1987 binaries, the DOS
self-host fixpoint, the DOS-hosted RealXV6 cross compiler, and that toolchain
rebuilding itself *on* the target — plus the system underneath it: the kernel
source, the scripts that compile it on the machine that runs it, and the
medium it boots from.

It is also the first stage whose *running system* needs nothing from anywhere
else. Stage 2 sent you to the RealXV6 project for a bootable one; here the
kernel is this project's own, and has to be — `reboot(2)` is slot 49 of *this*
kernel's `sysent`, `nosys` upstream, and `/dev/fd0` is its `dmr/FD.C`, which
upstream's `bdevsw` does not have. Once it is up, everything it needs to
rebuild itself — compiler, userland, kernel, boot sector — is on its own
disk.

## The DOS half

Unchanged. Mount `drive_c` as the DOS `C:` drive and type one word:

```
mount c /path/to/this/drive_c        (Linux, macOS)
mount c X:\path\to\this\drive_c      (Windows)
c:
cross
```

About half a minute later `C:\XCC` holds a cross compiler that turns a `.C`
into a RealXV6 a.out. Nothing to install, no config file — every bat sets its
own `PATH` and `DSINC`. Then one more word builds every program the image
carries — all 45 of them, from `init` to the toolchain itself — into
`C:\XV6\BIN`:

```
mkbin
```

And one more for the system underneath them — the kernel, the boot sector it
loads, and the 8-sector image `mkimg` checks its own layout against. A few
minutes: 32 compiles under emulation, and `DPAD` alone emits 33 KB of
initialized data.

```
mkkbin
```

Out comes `C:\XV6\KBIN`, which the protos reach through a `$KBIN` token the
way they reach `/bin` through `$BIN`.

**Nothing in this tree is compiled except the 1987 binaries under `DESMET`** —
`BIN`'s tools and `LIB`'s objects, the seed the whole chain grows from. Not the
userland, not the toolchain ports, not the kernel, not the boot sector, and not
the floppy. Every one of them is built here, from the source beside it, by the
compiler `CROSS` drove to its own fixpoint.

## The RealXV6 half

The same toolchain, compiled to V6 a.out and run as ordinary target programs.
The source is on the image under `/src`, which lives on its own filesystem:

```
/bin/mount /dev/rk1 /src
cd /src/rt     ; drun mkrt
cd /src/desmet ; drun mkall
cd /src/usr    ; drun mkusr
```

Each rebuilt binary is compared against the `/bin` binary that produced it.
Eight `IDENTICAL` lines from `mkall` — the toolchain ports — and thirty-seven
from `mkusr`, which is everything else the image carries, from `init` to
`sh`. Forty-five in all: **every program on this disk is rebuilt by this
disk**, with no host tool anywhere in the chain.

## The kernel half

`/src/sys` carries the kernel in the same shape the repository keeps it —
`KEN/` machine-independent, `DMR/` the PC half, `H/` the headers, `BOOT/` the
boot sector — and three scripts that take it the rest of the way:

```
cd /src/sys
drun mkkern          # 35 objects -> UNIX.COM, IDENTICAL to /test/REFK.COM
drun mkimg           # assemble the boot sector, lay down a 360K floppy
drun selfboot        # edit the kernel, rebuild it, write it, boot it
```

**`mkkern`** compiles all 35 objects with `/bin/{c88,gen,asm88}`, links them
through `KERNL`, folds the two-segment `.exe` into the `CS==DS==SS` `.COM`
the boot sector loads, and `dcmp`s the result against `/test/REFK.COM`. That
golden is what `MKKBIN` built on DOS — the same source, compiled by the same
fixpoint toolchain `/bin` is linked from — so `IDENTICAL` means the target
reproduced, on itself, the kernel it is running. Allow half an hour: the same
32 compiles, on an 8088 under emulation rather than on DOS.

**`mkimg`** assembles `BOOT/BOOTSEC.A` and checks the result against
`BOOT.COM`, byte for byte, before laying a 720-sector floppy image the way the
host does. That golden is the same source assembled on DOS by `MKKBIN`, so
`IDENTICAL` is the assembler reproducing itself across the two hosts — the
same shape as `/bin` and as the kernel.

**`selfboot`** is the one that is not reproduction. `ed` edits the banner in
`KEN/MAIN.C` on the machine itself, `MAIN` is recompiled, the kernel is
relinked and folded, the boot sector is assembled and checked, `mkboot` lays
the floppy, `cp` writes it to `/dev/fd0`, and `reboot(2)` flushes the buffer
cache, waits for the disk and resets the CPU — all without returning to user
mode, because a `sync(2)` followed by a reset would leave behind exactly the
corruption it meant to avoid. What comes back prints the new banner. Run
`mkkern` first: `selfboot` recompiles one source and reuses the other 34
objects, which is a minute rather than another half hour.

So `selfboot` deliberately does **not** `dcmp` against `REFK.COM`. After the
edit the source no longer matches the golden, which is the whole point.

`REFK.COM` is also exactly what the boot floppy is made of: the same bytes
`mkboot.py` lays at sector 1 below. So `mkkern` does not merely reproduce *a*
build of the kernel — it reproduces **the kernel that booted the machine
running it**. The 1987 binaries are the seed this whole tree grows from; they
are not in this chain.

## Building the two media

Two host-side programs, and nothing else off-target: `mkfs.py` for the disk
and `mkboot.py` for the floppy. Each is the host counterpart of a program the
target runs on itself — `/bin/mkfs` and `/bin/mkboot` — so what you build here
and what `drun mkimg` builds there are the same bytes.

The floppy first, since it is one line over what `MKKBIN` has already built:
the boot sector at sector 0, the kernel from sector 1, zeros to the end of a
360K disk.

```
python scripts/mkboot.py drive_c/XV6/KBIN/BOOT.COM \
    drive_c/XV6/KBIN/REFK.COM Unix360-desmet.img
```

Then the disk. Two protos describe the two filesystems, and they share one
file: the `rk` driver maps unit *N* to block *N*×4872 of the same IDE disk, so
rk1 *is* `unix.img` at block 4872. Build rk0 first — it creates the file —
then patch rk1 in at its offset:

```
python scripts/mkfs.py unix.img image/boot/proto \
    -D BIN=drive_c/XV6/BIN -D KBIN=drive_c/XV6/KBIN
python scripts/mkfs.py unix.img image/boot/proto.rk1 --offset 4872 \
    -D RT=drive_c/XV6/RT -D BUILD=drive_c/BUILD \
    -D KERNEL=drive_c/XV6/KERNEL -D KBIN=drive_c/XV6/KBIN
python -c "open('unix.img','r+b').truncate(10321920)"
```

`$BIN` is what `MKBIN` built and `$KBIN` what `MKKBIN` built, so run those
first; `$RT`, `$BUILD` and `$KERNEL` point back into this tree, so `/src` is
read straight from where the source is developed rather than duplicated.
`mkfs` writes only the prefix it used, so the last line pads to the fixed size
the IDE drive is configured for. Timestamps are written as zero, so the result
is reproducible: the same tree always gives the same image.

## Booting it

```
qemu-system-i386 -snapshot -boot a -serial file:console.txt \
    -drive file=Unix360-desmet.img,format=raw,if=floppy \
    -drive file=unix.img,format=raw,if=ide
```

The commands above are typed at the qemu window, because that is the only way
in: the console reads the PS/2 keyboard — `KL_BACKEND_KBD` in
`drive_c/XV6/KERNEL/H/PC.H` — and COM1 is a one-way copy of it. `KL_SERIAL_TEE`
tees every console byte out to the serial line and nothing ever reads the line
back, so `console.txt` is a transcript, which is how the logs under `evidence/`
were recorded, and `-display none` would leave a machine with no way to type at
it. A headless run is possible, but the keystrokes then have to be injected as
scancodes through the qemu monitor (`-monitor`, then `sendkey`).

`-snapshot` is not optional for `selfboot`, and is the reason it works: the
write to `/dev/fd0` goes to a temporary overlay, so the machine must come back
up *without* the qemu process exiting — which is exactly why `reboot` is a
program that resets the CPU rather than something done between two runs. It
also means the edit never persists: every run starts from an unedited source.

## What was checked, and what it printed

Recorded verbatim from the serial console.

| evidence | what it shows |
|---|---|
| `evidence/mkall.log` | the toolchain rebuilds itself on RealXV6, at the fixpoint |
| `evidence/usr.log` | the other 37 programs, rebuilt the same way — `/bin` closes |
| `evidence/kernel.log` | **`mkkern`: the target rebuilds its own kernel, `IDENTICAL`** |
| `evidence/selfboot.log` | **the banner changes, and the machine comes up on the change** |

`mkall` is stage 2's capstone: the compiler reproduces itself while running on
the system it targets. `usr` is the rest of that sentence — the other 37
programs, so nothing in `/bin` is left that the machine cannot rebuild.
`kernel` is the same claim one level down: it reproduces the system as well.
`selfboot` is the different one: not that the machine can reproduce itself,
but that it can *change* itself and come up on the change, with nothing
outside the disk involved in any step.
