@echo off
rem STAGE2 - rebuild the toolchain with the STAGE1 compiler, i.e. with the
rem tools that the shipped compiler produced (C:\BUILD\STAGE1, published by
rem STAGE1.BAT), then publish the result to C:\BUILD\STAGE2 under real names.
rem
rem   STAGE2           build all six components, then publish
rem   STAGE2 GEN       build one component only (no publish)
rem
rem Stage 1 is compiled by the shipped compiler, stage 2 by that stage-1
rem compiler.  Stage 2 is the self-reproducing fixpoint: feeding it back in
rem gives a stage 3 byte-identical to it -- that check is STAGE3.BAT, which
rem builds with what this bat publishes and publishes nothing itself.
rem
rem The PATH is set HERE and nowhere else, as in STAGE1.BAT / STAGE3.BAT.
rem Nothing is published unless all six components came out, so a failed run
rem leaves the previous STAGE2 intact.
rem C:\DESMET\BIN is NOT on this PATH: STAGE1 publishes every tool this build
rem runs, so behind it the 1987 directory could only ever be a silent
rem fallback -- and here that would falsify the result, rebuilding stage 1
rem while calling it stage 2.  C:\DESMET\LIB is the library BIND opens on
rem every link, not a compiler.
path C:\BUILD\STAGE1;C:\DESMET\LIB;Z:\
set DSINC=C:\DESMET\INCLUDE\
c:
cd \BUILD
rem Refuse to run on an empty STAGE1.  A missing dir on the PATH is not an
rem error in DOS, so without this the run would not fail -- every c88 would
rem be "Bad command or file name" and MK1 would carry on to the next one.
if not exist STAGE1\C88.EXE goto empty
if not exist STAGE1\GEN.EXE goto empty
if not exist STAGE1\ASM88.EXE goto empty
if not exist STAGE1\BIND.EXE goto empty
if not .%1.==.. goto one
call MKALL
if not exist C88\MY_C88.EXE goto bad
if not exist GEN\MY_GEN.EXE goto bad
if not exist ASM\MY_ASM88.EXE goto bad
if not exist BIND\MY_BIND.EXE goto bad
if not exist LIB88\MY_LIB88.EXE goto bad
if not exist D88\MY_D88.EXE goto bad
rem No redirects below: a redirect inside a CALLed bat ends the capture of
rem the whole run, so the log would stop right here.  DIR\NUL is the DOS
rem "does this directory exist" test.
if not exist STAGE2\NUL md STAGE2
if exist STAGE2\*.EXE del STAGE2\*.EXE
copy C88\MY_C88.EXE     STAGE2\C88.EXE
copy GEN\MY_GEN.EXE     STAGE2\GEN.EXE
copy ASM\MY_ASM88.EXE   STAGE2\ASM88.EXE
copy BIND\MY_BIND.EXE   STAGE2\BIND.EXE
copy LIB88\MY_LIB88.EXE STAGE2\LIB88.EXE
copy D88\MY_D88.EXE     STAGE2\D88.EXE
echo STAGE2: published to C:\BUILD\STAGE2
goto end
:one
call MK1 %1
goto end
:bad
echo STAGE2: not published - the build did not produce all six tools
goto end
:empty
echo STAGE2: C:\BUILD\STAGE1 is empty or incomplete - nothing built.
echo STAGE2: run STAGE1 first.
:end
