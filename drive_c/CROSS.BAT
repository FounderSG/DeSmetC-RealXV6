@echo off
rem CROSS - build the RealXV6 APPLICATION cross compiler, entirely inside DOS.
rem
rem   Mount this tree's drive_c as the DOS C: drive, then just:   CROSS
rem
rem That mount is the ONLY thing the host does.  No config file and no launcher
rem stand behind this tree: every bat that runs a tool sets the PATH and DSINC
rem it needs, so nothing depends on how the session was started or on where the
rem tree was unpacked.  Any session that can reach C:\ can build it.
rem
rem What comes out is a DOS-hosted cross compiler installed in C:\XCC: tools
rem that RUN on DOS and PRODUCE RealXV6 a.out.  CROSS ends standing IN C:\XCC,
rem where its driver XCC.BAT lies, so the next thing typed can be a compile:
rem "XCC PROG" compiles PROG.C to PROG.AO.  To compile a program where it
rem lies instead, spell the driver out from any directory: C:\XCC\XCC.BAT.
rem
rem Four steps.  WHICH compiler does the building is decided by the PATH the
rem bats set, and nowhere else:
rem
rem   STAGE1  the SHIPPED 3.03 tools rebuild the whole toolchain from source,
rem           published to C:\BUILD\STAGE1 under real names
rem   STAGE2  the stage-1 tools rebuild it again, published to C:\BUILD\STAGE2
rem   STAGE3  the stage-2 tools rebuild it once more, publishing nothing, and
rem           FCMP checks every result byte-identical to its STAGE2
rem           counterpart.  That is the fixpoint: the compiler reproduces
rem           itself exactly, so the toolchain the next step installs is a
rem           settled one rather than merely one that ran.
rem   MKXCC   install the cross compiler into C:\XCC -- the stage-2 tools,
rem           the RealXV6 runtime they link instead of DOS's CSTDIO, UNIX.H,
rem           and d2a, the .exe to a.out converter
rem
rem A stage is named after the compiler that PRODUCED it, counting from the
rem pre-existing one, as in GCC's bootstrap.
rem
rem No redirection character appears anywhere above, not even inside a rem and
rem not even quoted: DOS strips redirection before it works out what the
rem command is, so an arrow in a comment IS a redirect.  Say "to" instead.  To
rem capture a whole run, redirect from OUTSIDE this bat:
rem     command /c \CROSS.BAT  then the redirect  \CROSS.LOG
c:
path C:\DESMET\BIN;C:\DESMET\LIB;Z:\
set DSINC=C:\DESMET\INCLUDE\
cd \BUILD

rem --- the in-DOS byte compare the stage-3 check needs.  DOSBox-X's shell has
rem     neither FC nor COMP, and no host tool takes part in this build, so the
rem     comparer is compiled here first, by the shipped compiler.
if exist FCMP.EXE goto havecmp
echo.
echo === FCMP (the stage-3 comparer) ===
c88 fcmp
bind fcmp
if not exist FCMP.EXE goto nocmp
:havecmp

echo.
echo === CROSS 1 of 4: stage 1, built by the SHIPPED compiler ===
call STAGE1
if not exist \BUILD\STAGE1\C88.EXE goto bad1
if not exist \BUILD\STAGE1\GEN.EXE goto bad1

echo.
echo === CROSS 2 of 4: stage 2, built by the stage-1 compiler ===
call STAGE2
if not exist \BUILD\STAGE2\C88.EXE goto bad2
if not exist \BUILD\STAGE2\GEN.EXE goto bad2

echo.
echo === CROSS 3 of 4: stage 3, built by the stage-2 compiler ===
call STAGE3
cd \BUILD
if exist S3.DIF del S3.DIF
echo.
echo === CROSS: fixpoint check, stage 3 against stage 2 ===
FCMP C88\MY_C88.EXE     STAGE2\C88.EXE     S3.DIF
FCMP GEN\MY_GEN.EXE     STAGE2\GEN.EXE     S3.DIF
FCMP ASM\MY_ASM88.EXE   STAGE2\ASM88.EXE   S3.DIF
FCMP BIND\MY_BIND.EXE   STAGE2\BIND.EXE    S3.DIF
FCMP LIB88\MY_LIB88.EXE STAGE2\LIB88.EXE   S3.DIF
FCMP D88\MY_D88.EXE     STAGE2\D88.EXE     S3.DIF
if exist S3.DIF goto nofix

echo.
echo === CROSS 4 of 4: install the cross compiler into C:\XCC ===
call \BUILD\MKXCC.BAT
if not exist \XCC\C88.EXE goto bad4
if not exist \XCC\D2A.EXE goto bad4
if not exist \XCC\CRT0.O goto bad4
if not exist \XCC\UCOMPAT.O goto bad4
if not exist \XCC\UNIX.H goto bad4

rem --- end standing in the cross compiler's own directory, so the session is
rem     left ready to compile rather than ready to go looking.  XCC.BAT needs
rem     no PATH entry from there: DOS searches the current directory first.
cd \XCC

echo.
echo ==============================================================
echo  CROSS: OK - the RealXV6 cross compiler is installed in C:\XCC
echo.
echo  This session is now IN C:\XCC, so a program compiles as it
echo  stands.  One ships here as a worked example:
echo      XCC HELLO                  then HELLO.AO is a RealXV6 a.out
echo.
echo  To compile a program where it lies instead, from any
echo  directory:
echo      \XCC\XCC.BAT PROG          then PROG.AO beside PROG.C
echo ==============================================================
dir
goto end

:nocmp
echo.
echo CROSS: FAILED - could not build FCMP.EXE with the shipped compiler.
goto end
:bad1
echo.
echo CROSS: FAILED in stage 1 - C:\BUILD\STAGE1 was not published.
goto end
:bad2
echo.
echo CROSS: FAILED in stage 2 - C:\BUILD\STAGE2 was not published.
goto end
:nofix
echo.
echo CROSS: FAILED - stage 3 is NOT byte-identical to stage 2.
echo CROSS: the DIFFER line above names the tool; its stage-3 build is the
echo CROSS: MY_*.EXE left in the component directory, for comparison.
goto end
:bad4
echo.
echo CROSS: FAILED - the cross compiler was not installed into C:\XCC.
:end
