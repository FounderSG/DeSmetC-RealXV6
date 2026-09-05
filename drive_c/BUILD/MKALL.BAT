@echo off
rem MKALL - rebuild the WHOLE DeSmet C 3.03 toolchain from source in one
rem DOS session (the self-hosting build).  Each component is built by MK1,
rem and the first failure stops the run.
rem
rem   MKALL           build all six components
rem   MK1 GEN         build just one
rem
rem Which compiler does the building is decided by the PATH, not here.  The
rem three wrapper bats each set one, and are named for the stage they PRODUCE:
rem   STAGE1.BAT   PATH C:\DESMET\BIN first     stage 1, from the shipped tools
rem   STAGE2.BAT   PATH C:\BUILD\STAGE1 first   stage 2, the self-reproducing fixpoint
rem   STAGE3.BAT   PATH C:\BUILD\STAGE2 first   stage 3, which must equal stage 2
rem
rem So this bat sets NO environment at all, and that is exactly what lets one
rem set of build bats serve all three stages.  Whatever PATH the session
rem already has is what it would build with, so start from CROSS, or from
rem STAGE1, rather than from here.
rem
rem No redirection character anywhere above, not even inside a rem and not
rem even quoted: DOS strips redirection before it works out what the command
rem is, so an arrow in a comment IS a redirect, and a redirect inside a
rem CALLed bat ends the capture of the whole enclosing run (see MK1.BAT).
rem One arrow written in this very comment block left C:\BUILD\MKALL.LOG
rem completely empty.
rem
rem Output is MY_*.EXE in each component directory; the installed tools in
rem C:\DESMET\BIN are never overwritten.  CROSS.BAT runs this same bat
rem three times over, once per stage, and a captured run collects the
rem whole session into one log.
c:
cd \BUILD
call MK1 C88
if not exist C88\MY_C88.EXE goto bad
call MK1 GEN
if not exist GEN\MY_GEN.EXE goto bad
call MK1 ASM
if not exist ASM\MY_ASM88.EXE goto bad
call MK1 BIND
if not exist BIND\MY_BIND.EXE goto bad
call MK1 LIB88
if not exist LIB88\MY_LIB88.EXE goto bad
call MK1 D88
if not exist D88\MY_D88.EXE goto bad
echo.
echo MKALL: ALL OK
goto end
:bad
echo.
echo MKALL: FAILED
:end
