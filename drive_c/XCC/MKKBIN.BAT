@echo off
rem MKKBIN - build the KERNEL and the BOOT SECTOR on DOS, into C:\XV6\KBIN.
rem
rem   CROSS, then MKBIN, then:  MKKBIN
rem
rem Out come the three files the filesystem image and the boot floppy need,
rem and none of them ships in this tree:
rem
rem   REFK.COM    the kernel.  /test/REFK.COM on the image, and sector 1 of
rem               the floppy.  mkkern on the target rebuilds it from the
rem               source under /src/sys and dcmp's its own result against
rem               this one, so IDENTICAL there means the target reproduced
rem               the kernel it is running.
rem   BOOT.COM    the 512-byte boot sector.  /src/sys/BOOT.COM on the image,
rem               and sector 0 of the floppy.  mkimg and selfboot assemble
rem               BOOT/BOOTSEC.A on the target and dcmp against this.
rem   REFF.IMG    an 8-sector image laid out the way the floppy is, which
rem               mkimg compares its own mkboot's work against.  Its payload
rem               is the boot sector again: any file drives the same code
rem               path, and that one is already here.
rem
rem Three steps, each one a bat this tree already has:
rem
rem   1  \XV6\KERNEL\MKKERN.BAT   compiles 35 objects and links UNIX.EXE
rem   2  e2c                      folds that two-segment .exe into the
rem                               CS==DS==SS .COM the boot sector loads
rem   3  \XV6\KERNEL\BOOT\MKBOOTS.BAT plus mkboot -s, which strips the MZ
rem                               header, zero-fills to 510 and stamps 55 AA
rem
rem e2c and mkboot are the DOS builds XV6.BAT leaves in C:\XV6\SRC, the same
rem two sources the target runs as /bin/e2c and /bin/mkboot.  So the kernel a
rem reader builds here and the one the target rebuilds come off one recipe.
rem
rem Allow a few minutes: 32 compiles under emulation, and DPAD alone emits
rem 33 KB of initialized data.
rem
rem Step 1 is reached with "command /c" and not "call", which is the one
rem thing in this bat that is not obvious.  MKKERN.BAT and CC1.BAT redirect
rem into BUILD.LOG as they go, and in DOSBox-X a redirect performed inside a
rem CALLed bat ends the capture of the ENCLOSING run: with "call" here, a
rem captured MKKBIN stops dead at the banner below and says nothing about
rem why.  Its own shell keeps that contained.  Steps 2 and 3 redirect
rem nothing, so MKBOOTS.BAT is still CALLed.
rem
rem So the two logs are: this run, captured from outside, and the per-compile
rem detail of step 1, which MKKERN.BAT writes to C:\XV6\KERNEL\BUILD.LOG
rem itself -- the reason step 1 prints nothing here but its banner.
rem
rem No redirection character appears above, not even inside a rem: DOS strips
rem redirection before it works out what the command is, so an arrow in a
rem comment IS a redirect.  To capture a whole run, redirect from OUTSIDE:
rem     command /c \XCC\MKKBIN.BAT  then the redirect  \MKKBIN.LOG
c:
if not exist \BUILD\STAGE2\C88.EXE goto nostage2
if not exist \XV6\SRC\E2C.EXE goto notools
if not exist \XV6\SRC\MKBOOT.EXE goto notools

echo.
echo === MKKBIN 1 of 3: the kernel (35 objects) ===
command /c \XV6\KERNEL\MKKERN.BAT
c:
cd \XV6\KERNEL
if not exist UNIX.EXE goto badkern

echo.
echo === MKKBIN 2 of 3: fold the .exe into the .COM the boot sector loads ===
path C:\XV6\SRC;C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
if exist REFK.COM del REFK.COM
e2c UNIX.EXE REFK.COM UNIX.MAP
if not exist REFK.COM goto badcom

echo.
echo === MKKBIN 3 of 3: the boot sector, and the layout golden ===
call \XV6\KERNEL\BOOT\MKBOOTS.BAT
c:
path C:\XV6\SRC;C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
cd \XV6\KERNEL\BOOT
if not exist BOOTSEC.EXE goto badsec
if exist BOOT.COM del BOOT.COM
mkboot -s BOOTSEC.EXE BOOT.COM
if not exist BOOT.COM goto badsec
if exist REFF.IMG del REFF.IMG
mkboot BOOT.COM BOOT.COM REFF.IMG 8
if not exist REFF.IMG goto badgold

rem --- publish, the way STAGE1/STAGE2 and XV6.BAT do.  Delete first so a
rem     stale file cannot survive and so COPY is never in a position to ask
rem     about overwriting inside a batch.
if not exist \XV6\KBIN\NUL md \XV6\KBIN
if exist \XV6\KBIN\*.COM del \XV6\KBIN\*.COM
if exist \XV6\KBIN\*.IMG del \XV6\KBIN\*.IMG
copy \XV6\KERNEL\REFK.COM \XV6\KBIN
copy BOOT.COM \XV6\KBIN
copy REFF.IMG \XV6\KBIN
if not exist \XV6\KBIN\REFK.COM goto badpub
if not exist \XV6\KBIN\BOOT.COM goto badpub

echo.
echo ==============================================================
echo  MKKBIN: OK - the kernel and the boot sector are in C:\XV6\KBIN,
echo  built here from the source beside them.  Next, on the host:
echo      python scripts/mkboot.py ...   for the floppy
echo      python scripts/mkfs.py ...     for the disk
echo  with -D KBIN=drive_c/XV6/KBIN, and see the README.
echo ==============================================================
dir \XV6\KBIN
goto end

:nostage2
echo.
echo MKKBIN: FAILED - C:\BUILD\STAGE2 is empty.  Run CROSS first.
goto end
:notools
echo.
echo MKKBIN: FAILED - no E2C.EXE or MKBOOT.EXE in C:\XV6\SRC.
echo MKKBIN: those are XV6.BAT's DOS builds.  Run MKBIN first.
goto end
:badkern
echo.
echo MKKBIN: FAILED - MKKERN.BAT produced no UNIX.EXE.
echo MKKBIN: see C:\XV6\KERNEL\BUILD.LOG for the compile that stopped.
goto end
:badcom
echo.
echo MKKBIN: FAILED - e2c produced no REFK.COM.
goto end
:badsec
echo.
echo MKKBIN: FAILED - the boot sector did not assemble.
goto end
:badgold
echo.
echo MKKBIN: FAILED - mkboot produced no REFF.IMG.
goto end
:badpub
echo.
echo MKKBIN: FAILED - C:\XV6\KBIN was not filled.
:end
