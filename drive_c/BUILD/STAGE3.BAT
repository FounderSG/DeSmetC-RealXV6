@echo off
rem STAGE3 - rebuild the toolchain with the STAGE2 compiler, i.e. with the
rem tools that C:\BUILD\STAGE2 holds.  This is the fixpoint CHECK: stage 2 is
rem self-reproducing, so every MY_*.EXE this leaves behind must come out
rem byte-identical to its STAGE2 counterpart.
rem
rem   STAGE3           build all six components
rem   STAGE3 GEN       build one component only
rem
rem The PATH is set HERE and nowhere else, exactly as in STAGE1.BAT /
rem STAGE2.BAT; this bat IS the definition of "build with the stage-2
rem compiler".
rem
rem Deliberately publishes NOTHING - there is no STAGE3\ directory.  A
rem stage-2 vs stage-3 difference has to stay visible instead of being
rem overwritten by the run that should have detected it, so the result is
rem left as MY_*.EXE in each component directory for comparison.  The comparer
rem is already in the tree and no host tool is needed for it: FCMP, which CROSS
rem builds in C:\BUILD, takes the two files and a file to record differences in:
rem     FCMP C88\MY_C88.EXE STAGE2\C88.EXE S3.DIF
rem Note that the session's PATH keeps C:\BUILD\STAGE2 in front afterwards.
rem C:\DESMET\BIN is NOT on this PATH, for the reason STAGE2.BAT gives -- here
rem it would turn the fixpoint check into stage 2 compared against itself.
path C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
set DSINC=C:\DESMET\INCLUDE\
c:
cd \BUILD
rem Refuse to run on an empty STAGE2.  A missing dir on the PATH is not an
rem error in DOS, so without this every c88 would be "Bad command or file
rem name" and MKALL would carry on to the next component regardless.
if not exist STAGE2\C88.EXE goto empty
if not exist STAGE2\GEN.EXE goto empty
if not exist STAGE2\ASM88.EXE goto empty
if not exist STAGE2\BIND.EXE goto empty
if not .%1.==.. goto one
call MKALL
goto end
:one
call MK1 %1
goto end
:empty
echo STAGE3: C:\BUILD\STAGE2 is empty or incomplete - nothing built.
echo STAGE3: run STAGE1 then STAGE2 first.
:end
