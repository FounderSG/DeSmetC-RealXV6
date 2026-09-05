# DeSmet C 3.03 — a RealXV6 cross compiler, built in DOS

This tree builds a **cross compiler that runs on DOS and produces RealXV6
applications**, starting from nothing but the shipped 1987 **DeSmet C 3.03**
binaries and the compiler's own source. Every step happens inside DOS: there is
no build system, no scripts on the host side, nothing to install. Mount the
drive, type one word.

## Build it

There is no launcher and no DOSBox-X config here. The only thing the host has
to do is mount `drive_c` as the DOS `C:` drive — from any DOS prompt, in
whatever emulator you already have:

```
mount c /path/to/DeSmet/drive_c        (Linux, macOS)
mount c X:\path\to\DeSmet\drive_c      (Windows)
c:
cross
```

That argument is the only place the host's own path syntax shows through.
Everything from `c:` onwards is DOS and is the same everywhere. Or as one
command line:

```
dosbox-x -c "mount c /path/to/DeSmet/drive_c" -c "c:" -c "cross"
```

(`dosbox-x.exe` on Windows.)

Nothing needs setting up first — not `PATH`, not `DSINC`, not the working
directory. Every bat sets its own (see [The environment](#the-environment)), so
the tree runs wherever you unpack it and however you got to the prompt.

One thing to watch on the host side, and the only one: started without `-conf`,
DOSBox-X loads a default config of its own — beside the executable on a
portable install, a per-user file otherwise, depending on platform. If that
config already mounts a `C:`, the `mount` above fails with *Drive C is already
mounted* and you quietly end up building whatever tree it pointed at instead of
this one. Either `mount -u c` first, or give `-conf` a config that mounts
nothing. It does have to be `C:`: the bats address the tree by absolute path
throughout, which is what keeps them independent of the directory you happen to
be standing in.

About half a minute later the cross compiler is installed in `C:\XCC`
(`drive_c/XCC` on the host). That half minute assumes `cycles=max`; at
DOSBox-X's default `cycles=auto` the same build takes considerably longer.

## Use it

One worked example ships with the tree, `C:\XCC\HELLO.C`, and `CROSS` leaves the
session standing in `C:\XCC`. So the moment it finishes, with no directory to
change into and no `PATH` to set:

```
xcc hello                     rem  HELLO.C  ->  HELLO.AO, a RealXV6 a.out
```

Start from it rather than from a hello you already have. Its comment header
is the short version of everything below — the quoted `#include "UNIX.H"`,
K&R function definitions, the V6 `printf` rather than CSTDIO's, `return` from
`main` as the exit status — and a modern hello does **not** compile here.
The two errors it earns are worth recognising:

```
cannot open C:\XCC\stdio.h              #include <stdio.h>: DSINC is the
                                        target's headers only, on purpose
3 int main( $$ void)
       error:duplicate argument         int main(void): 3.03 predates ANSI,
                                        so `void` is read as a parameter name
```

Otherwise put a `PROG.C` in any directory and:

```
\XCC\XCC.BAT PROG             rem  PROG.C  ->  PROG.AO, a RealXV6 a.out
```

Put `C:\XCC` on the `PATH` and it is just `XCC PROG`. `XCC` runs the three
steps the DeSmet toolchain always runs, with the target swapped underneath
them:

```
C88   PROG.C to PROG.O            C88 chains to GEN; both from C:\XCC
BIND  -A, CRT0 first, then the V6 runtime in place of DOS's CSTDIO
D2A   that .exe to RealXV6 separated-I and D a.out, magic 0411
```

`PROG` must fit a DOS 8.3 basename. `XCC PROG 2000` gives it a 2000H-byte
stack instead of the default 1000H.

## What CROSS actually does

Four steps, one bat each. **Which** compiler does the building is decided by
the `PATH` those bats set, and nowhere else:

| step | bat | |
|------|-----|---|
| 1 | `BUILD\STAGE1` | the **shipped** 3.03 tools rebuild the whole toolchain from source; published to `C:\BUILD\STAGE1` under real names |
| 2 | `BUILD\STAGE2` | the **stage-1** tools rebuild it again; published to `C:\BUILD\STAGE2` |
| 3 | `BUILD\STAGE3` | the **stage-2** tools rebuild it once more, publishing nothing, and `FCMP` checks every result byte-identical to its `STAGE2` counterpart |
| 4 | `BUILD\MKXCC` | install the cross compiler into `C:\XCC` |

A stage is named after the compiler that *produced* it, counting from the
pre-existing one, as in GCC's bootstrap. Step 3 is the point of the exercise:
stage 2 is a **fixpoint**, so stage 3 comes out byte-identical to it, and the
compiler installed in step 4 is therefore a settled one rather than merely one
that ran. Expect:

```
FCMP: IDENTICAL C88\MY_C88.EXE (40448 bytes)
FCMP: IDENTICAL GEN\MY_GEN.EXE (49664 bytes)
FCMP: IDENTICAL ASM\MY_ASM88.EXE (37376 bytes)
FCMP: IDENTICAL BIND\MY_BIND.EXE (17408 bytes)
FCMP: IDENTICAL LIB88\MY_LIB88.EXE (9216 bytes)
FCMP: IDENTICAL D88\MY_D88.EXE (37888 bytes)
```

Each bat is equally usable on its own (`cd \BUILD`, then `stage1`, `stage2`,
`stage3`, `mkxcc`), and each refuses to run on a missing predecessor rather
than quietly falling through to the shipped tools.

`FCMP.C` exists because the check has to run *in DOS*: DOSBox-X's shell has
neither `FC` nor `COMP`. `CROSS` compiles it first, with the shipped compiler.

To capture a whole run, redirect from **outside** the bat — a redirect *inside*
a called batch silently ends the capture of the enclosing run:

```
command /c \CROSS.BAT > \CROSS.LOG
```

## What is in C:\XCC

Nothing in the compiler itself is RealXV6-specific. What retargets it is the
runtime it links and the conversion that follows:

| | |
|---|---|
| `C88 GEN ASM88 BIND LIB88` | the stage-2 toolchain, i.e. the rebuilt compiler at its fixpoint |
| `CRT0.O` | the V6 startup — **linked first in every program** |
| `SYSCALL ULIB PRINTF CTIME SETEXIT UCOMPAT` (`.O`) | the V6 C runtime, in place of DOS's CSTDIO |
| `UNIX.H` | the target API header |
| `D2A.EXE` | `.exe` to `a.out` converter |
| `CSTDIO.S` | see below — BIND demands it, nothing is taken from it |
| `XCC.BAT` | the driver |
| `HELLO.C` | the worked example — `xcc hello`, where `CROSS` leaves you |

## The environment

Nothing outside `drive_c\` sets anything up, and that is deliberate rather than
merely tidy: the `PATH` is what decides **which** compiler does the building,
so it belongs in the bat that defines a stage and nowhere else. A config file
setting it once, for the whole session, is precisely the thing that would make
a stage-2 build quietly fall through to the shipped tools.

| bat | the `PATH` it sets, and what that makes it mean |
|-----|------------------------------------------------|
| `CROSS.BAT` | `C:\DESMET\BIN` — the shipped tools, to compile `FCMP` before the stages start |
| `BUILD\STAGE1.BAT` | `C:\DESMET\BIN` first — *build with the shipped compiler* |
| `BUILD\STAGE2.BAT` | `C:\BUILD\STAGE1` first — *build with the stage-1 compiler* |
| `BUILD\STAGE3.BAT` | `C:\BUILD\STAGE2` first — *build with the stage-2 compiler* |
| `BUILD\MKXCC.BAT` | `C:\BUILD\STAGE2` first — install from the fixpoint |
| `XCC\XCC.BAT` | `C:\XCC` **only** — the cross compiler, with no DOS toolchain in front and no DOS header reachable |

`MKALL.BAT` and `MK1.BAT` set no environment at all, on purpose: they build
with whatever is already in front, which is how one set of build bats serves
all three stages. The consequence is worth knowing — run either on its own in a
fresh session and nothing builds, because no compiler is on the `PATH` yet.
Start from `CROSS`, or from `STAGE1`.

`DSINC` travels with the `PATH` in each of those bats and must end with a
trailing `\`.

## Layout

```
DeSmet/                          <-- unpack anywhere; no path is baked in
├─ README.md                     this file
└─ drive_c/                      <-- mount this as the DOS C: drive
                                     (host paths above, DOS paths below)
     ├─ CROSS.BAT               the one command
     ├─ DESMET\                 the shipped DeSmet C 3.03 install
     │    BIN\  INCLUDE\  LIB\
     ├─ BUILD\                  the bootstrap
     │    STAGE1.BAT STAGE2.BAT STAGE3.BAT MKXCC.BAT
     │    MKALL.BAT MK1.BAT     the build itself (MK1 = one component)
     │    FCMP.C                in-DOS byte compare, for the stage-3 check
     │    C88\ GEN\ ASM\ BIND\ LIB88\ D88\      compiler source
     ├─ XV6\RT\                 the RealXV6 runtime source
     │    CRT0.A SETEXIT.A SYSCALL.C ULIB.C PRINTF.C CTIME.C
     │    UCOMPAT.C UNIX.H D2A.C
     └─ XCC\                    the cross compiler (XCC.BAT + HELLO.C ship;
                                CROSS fills in the rest)
```

Everything the build writes — `STAGE1\`, `STAGE2\`, `XV6\SRC\`, the binaries in
`XCC\`, and the `.O` / `MY_*.EXE` in the component directories — is regenerated
by `CROSS`, so the tree ships as source plus the shipped 1987 binaries.

## Notes

* **Why 3.03 and not 3.1H.** The point is to bootstrap the *whole* toolchain
  with one compiler. `ASM88`/`BIND`/`D88` are 3.03-era K&R sources using
  pre-ANSI idioms — `signed` as an ordinary identifier, "global struct member"
  type-punning like `(&unsigned_var)->bytes[1]` — that the strict ANSI 3.1H
  compiler rejects and 3.03 accepts as warnings. One such warning
  (`member not in structure`, `ASM3.C:325`) is expected in every run.
* **What makes a DOS .exe a RealXV6 program.** `BIND -A` suppresses BIND's own
  `CSETUP`, so execution starts at the first instruction of the first object —
  which is why `CRT0.O` must come first, and why `d2a` can patch the `ENDBRK_`
  word CRT0 contributes at the very start of `DSEG`. The application source
  itself is untouched; that is the whole point.
* **BIND always opens `CSTDIO.S`**, as the last file of every link, and `-A`
  does not change that (`BIND.C:63`). So it is installed in `C:\XCC` and found
  there by BIND's own `PATH` search — not by `-L`, which would push the link
  line past the 127-character DOS command line. Nothing is taken from it: the
  V6 runtime resolves everything first.
* **`UNIX.H` is found through C88's `-i`, not through `DSINC`.** `DSINC` only
  covers the angle-bracket form, and RealXV6 sources spell it
  `#include "UNIX.H"` — measured, not assumed: with `DSINC` alone C88 says
  `cannot open UNIX.H`. `-i` is what lets a program be compiled where it lies.
* **Never write `<` or `>` in a `.BAT`,** not even inside a `rem`. DOS strips
  redirection before it decides what the command is, so an arrow in a comment
  is a real redirect; combined with the capture rule above, one of those can
  empty the log of an entire run.
* `DSINC` must end with a trailing `\`.
* **Not in this tree:** the other direction — the DeSmet toolchain rebuilt to
  run *on* RealXV6, so the target compiles its own source. That is a separate
  job (`BIND -A` against a V6 shim for `chain()`, DOS-flavour `exit`/`creat`,
  and CSTDIO-compatible stdio) and a later step.
