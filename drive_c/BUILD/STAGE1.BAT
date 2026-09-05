@echo off
rem STAGE1 - rebuild the toolchain with the SHIPPED 3.03 tools (the bootstrap
rem compiler in C:\DESMET\BIN), then publish the six rebuilt binaries to
rem C:\BUILD\STAGE1 under their real names: C88.EXE, GEN.EXE, ASM88.EXE,
rem BIND.EXE, LIB88.EXE, D88.EXE.
rem
rem   STAGE1           build all six components, then publish
rem   STAGE1 GEN       build one component only (no publish)
rem
rem Stage 1 is the usual bootstrap numbering (as in GCC's): the stage is named
rem after the compiler that PRODUCED it, counting from the pre-existing one.
rem So stage 1 is what the shipped compiler produces, and STAGE2.BAT feeds
rem stage 1 back in to produce stage 2.
rem
rem The PATH is set HERE and nowhere else: this bat IS the definition of
rem "build with the shipped compiler".  What comes out is a complete toolchain.
rem Nothing is published unless all six components came out, so a failed run
rem leaves the previous STAGE1 intact.  Note that the session's PATH stays
rem pointed at the shipped tools afterwards.
path C:\DESMET\BIN;C:\DESMET\LIB;Z:\
set DSINC=C:\DESMET\INCLUDE\
c:
cd \BUILD
if not .%1.==.. goto one
call MKALL
if not exist C88\MY_C88.EXE goto bad
if not exist GEN\MY_GEN.EXE goto bad
if not exist ASM\MY_ASM88.EXE goto bad
if not exist BIND\MY_BIND.EXE goto bad
if not exist LIB88\MY_LIB88.EXE goto bad
if not exist D88\MY_D88.EXE goto bad
rem No redirects below (not even "greater-than NUL" to quieten copy): a
rem redirect inside a CALLed bat ends the capture of the whole run, so the
rem log would stop right here.  DIR\NUL is the DOS "does this directory
rem exist" test; without it md would natter on every rebuild.
if not exist STAGE1\NUL md STAGE1
if exist STAGE1\*.EXE del STAGE1\*.EXE
copy C88\MY_C88.EXE     STAGE1\C88.EXE
copy GEN\MY_GEN.EXE     STAGE1\GEN.EXE
copy ASM\MY_ASM88.EXE   STAGE1\ASM88.EXE
copy BIND\MY_BIND.EXE   STAGE1\BIND.EXE
copy LIB88\MY_LIB88.EXE STAGE1\LIB88.EXE
copy D88\MY_D88.EXE     STAGE1\D88.EXE
echo STAGE1: published to C:\BUILD\STAGE1
goto end
:one
call MK1 %1
goto end
:bad
echo STAGE1: not published - the build did not produce all six tools
:end
