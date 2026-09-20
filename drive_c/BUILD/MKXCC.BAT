@echo off
rem MKXCC - install the RealXV6 APPLICATION cross compiler into C:\XCC.
rem
rem A DOS-hosted cross compiler: these tools RUN on DOS and PRODUCE RealXV6
rem a.out.  (The other direction -- the toolchain rebuilt to run ON RealXV6 --
rem is a separate job and is not part of this tree.)
rem
rem Three things go into C:\XCC:
rem
rem   the compiler   C88 GEN ASM88 BIND LIB88, copied from C:\BUILD\STAGE2,
rem                  i.e. the rebuilt toolchain at its self-reproducing
rem                  fixpoint.  Nothing about them is RealXV6-specific; what
rem                  retargets them is the runtime they link and the a.out
rem                  conversion that follows.
rem   the runtime    CRT0.O first, then SYSCALL ULIB PRINTF CTIME SETEXIT
rem                  UCOMPAT -- the V6 startup and C library that replace
rem                  DOS's CSTDIO, plus UNIX.H, the target API header.
rem   the converter  D2A.EXE, which rewrites a BIND -A small-model MZ .exe
rem                  into RealXV6 separated-I and D a.out (magic 0411).
rem
rem CSTDIO.S is installed alongside them, and not because a RealXV6 program
rem wants DOS stdio: BIND ALWAYS opens its default library as the last file
rem of every link (BIND.C:63, and -A only suppresses the CSETUP startup
rem object, not this), so the link fails outright without it.  Nothing is
rem taken from it -- the V6 runtime resolves everything first.  It is copied
rem into C:\XCC rather than left in C:\DESMET\LIB so the cross compiler is
rem self-contained, and found there by BIND's own PATH search rather than by
rem a -L option, which would push the link line past the 127-character DOS
rem command line.
rem
rem CRT0.O must be linked FIRST in every program: BIND -A starts execution at
rem the first instruction of the first object, and d2a patches the ENDBRK_
rem word that CRT0 contributes at the very start of DSEG.
rem
rem Needs C:\BUILD\STAGE2 -- run STAGE1 then STAGE2 first, or just CROSS.
rem The driver that uses all this is C:\XCC\XCC.BAT: "XCC PROG".
rem The PATH carries no second compiler: STAGE2 publishes every tool this bat
rem runs, so C:\DESMET\BIN could only ever be a silent fallback.  Only the
rem library is taken from C:\DESMET -- CSTDIO.S, which BIND opens on every
rem link, as above.
path C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
set DSINC=C:\XV6\RT\
c:
cd \BUILD
if not exist STAGE2\C88.EXE goto nostage2
if not exist STAGE2\GEN.EXE goto nostage2
if not exist STAGE2\ASM88.EXE goto nostage2
if not exist STAGE2\BIND.EXE goto nostage2
if not exist STAGE2\LIB88.EXE goto nostage2

rem --- build the runtime in a scratch directory, not in XV6\RT, so the
rem     shipped source tree stays free of objects.  Wipe by extension:
rem     "del *.*" asks for confirmation and would hang a batch.
if not exist \XV6\SRC\NUL md \XV6\SRC
cd \XV6\SRC
if exist *.C del *.C
if exist *.H del *.H
if exist *.A del *.A
if exist *.O del *.O
if exist *.EXE del *.EXE
copy \XV6\RT\*.A
copy \XV6\RT\*.C
copy \XV6\RT\*.H

echo.
echo === XCC: the RealXV6 runtime ===
for %%f in (CRT0 SETEXIT) do asm88 %%f
for %%f in (SYSCALL ULIB PRINTF CTIME UCOMPAT) do c88 %%f
if not exist CRT0.O goto bad
if not exist SETEXIT.O goto bad
if not exist SYSCALL.O goto bad
if not exist ULIB.O goto bad
if not exist PRINTF.O goto bad
if not exist CTIME.O goto bad
if not exist UCOMPAT.O goto bad

rem --- the converter, as an ordinary DOS program (plain BIND, CSTDIO) ---
echo.
echo === XCC: d2a, the .exe to a.out converter ===
c88 D2A
bind d2a
if not exist D2A.EXE goto bad

rem --- install.  Delete first so a stale file cannot survive and so COPY is
rem     never in a position to ask about overwriting inside a batch.
echo.
echo === XCC: install to C:\XCC ===
if not exist \XCC\NUL md \XCC
if exist \XCC\*.EXE del \XCC\*.EXE
if exist \XCC\*.O del \XCC\*.O
if exist \XCC\*.H del \XCC\*.H
if exist \XCC\*.S del \XCC\*.S
copy \DESMET\LIB\CSTDIO.S    \XCC
copy \BUILD\STAGE2\C88.EXE   \XCC
copy \BUILD\STAGE2\GEN.EXE   \XCC
copy \BUILD\STAGE2\ASM88.EXE \XCC
copy \BUILD\STAGE2\BIND.EXE  \XCC
copy \BUILD\STAGE2\LIB88.EXE \XCC
copy D2A.EXE     \XCC
copy CRT0.O      \XCC
copy SETEXIT.O   \XCC
copy SYSCALL.O   \XCC
copy ULIB.O      \XCC
copy PRINTF.O    \XCC
copy CTIME.O     \XCC
copy UCOMPAT.O   \XCC
copy \XV6\RT\UNIX.H \XCC
if not exist \XCC\C88.EXE goto bad
if not exist \XCC\D2A.EXE goto bad
if not exist \XCC\CRT0.O goto bad
if not exist \XCC\CSTDIO.S goto bad
if not exist \XCC\XCC.BAT goto nodriver
echo.
echo XCC: installed to C:\XCC
goto end
:bad
echo.
echo XCC: FAILED - the cross compiler was not installed.
goto end
:nodriver
echo.
echo XCC: FAILED - C:\XCC\XCC.BAT is missing from the source tree.
goto end
:nostage2
echo XCC: C:\BUILD\STAGE2 is empty or incomplete - run STAGE1 then STAGE2 first.
:end
