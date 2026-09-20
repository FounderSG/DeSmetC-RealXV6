@echo off
rem XV6 - build the RealXV6 CROSS COMPILER: the DeSmet toolchain itself,
rem relinked to run ON RealXV6 instead of on DOS.  Builds in C:\XV6\SRC and
rem publishes eight a.out to C:\BUILD\XV6BIN, which MKBIN then collects:
rem
rem   C88.AO GEN.AO ASM88.AO DBIND.AO DLIB88.AO   the five tool ports
rem   D2A.AO                                      the .exe to a.out converter
rem   E2C.AO                                      the .exe to .COM converter
rem   MKBOOT.AO                                   the .COM to boot floppy
rem
rem Not a numbered stage: STAGE1/STAGE2 iterate the same DOS toolchain on
rem itself, while this retargets the settled one at another operating system.
rem It uses the stage-2 compiler (C:\BUILD\STAGE2, the fixpoint) -- and, just
rem as important, the compiler pass objects it links (C:\BUILD\COMP\*.O)
rem must be the ones a STAGE2 run left behind.  Objects from a stage-1 build
rem would produce a /bin the on-target rebuild cannot reproduce.  RUN STAGE1,
rem THEN STAGE2, THEN XV6 -- or just CROSS, which does all three.
rem
rem COMP above is a placeholder spelled without angle brackets on purpose:
rem DOS strips redirection before it works out what the command is, so a
rem bracket inside a rem IS a redirect.  Same warning as in MKALL.BAT.
rem
rem What makes a DOS tool a V6 tool: BIND -A (no DOS startup) links CRT0.A
rem plus the V6 runtime shim from C:\XV6\RT in place of DOS's -- V6CHAIN for
rem chain(), DCOMPAT for the DOS flavors of exit/creat/open, V6DOS for
rem CSTDIO-compatible stdio, V6MISC for _showsp/_MSDOS2_.  The tool sources
rem themselves are untouched; that is the whole point.  The object list of
rem each link lives in C:\XV6\RT\*.RSP (BIND response files), not here.
rem
rem The PATH carries no second compiler: C:\DESMET\BIN is absent because
rem STAGE2 publishes every tool this bat runs, so it could only ever be a
rem silent fallback -- in DOS a PATH entry that does not exist is not an
rem error.  C:\DESMET\LIB is not a compiler; it is where BIND finds CSTDIO.S,
rem which it opens as the last file of every link (BIND.C:63) even when
rem nothing is taken from it, as here.  DSINC is the staging directory, so a
rem V6 target cannot pick up a DOS header.
path C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
set DSINC=C:\XV6\SRC\
c:
cd \BUILD
if not exist STAGE2\C88.EXE goto nostage2
if not exist STAGE2\GEN.EXE goto nostage2
if not exist STAGE2\ASM88.EXE goto nostage2
if not exist STAGE2\BIND.EXE goto nostage2
if not exist C88\C88.O goto noobj
if not exist GEN\GEN.O goto noobj
if not exist ASM\ASM88.O goto noobj
if not exist BIND\BIND.O goto noobj
if not exist LIB88\LIB88.O goto noobj

rem --- stage sources + compiler pass objects into the scratch build dir ---
rem Wipe by extension: "del *.*" asks for confirmation and would hang a batch.
if not exist \XV6\SRC\NUL md \XV6\SRC
cd \XV6\SRC
if exist *.C del *.C
if exist *.H del *.H
if exist *.A del *.A
if exist *.O del *.O
if exist *.OA del *.OA
if exist *.RSP del *.RSP
if exist *.EXE del *.EXE
if exist *.MAP del *.MAP
if exist *.AO del *.AO
copy \XV6\RT\*.A
copy \XV6\RT\*.C
copy \XV6\RT\*.H
copy \XV6\RT\*.RSP
for %%f in (C88 C2 C3 C4 C5 C6 C7) do copy \BUILD\C88\%%f.O
for %%f in (GEN GEN2 GEN3 GEN4 GEN5 GEN6) do copy \BUILD\GEN\%%f.O
for %%f in (ASM88 ASM2 ASM3 ASM4 ASM5) do copy \BUILD\ASM\%%f.O
copy \BUILD\BIND\BIND.O
copy \BUILD\LIB88\LIB88.O

rem --- the V6 runtime the tool ports link against ---
rem DCRT0 must be first in every TOOL link (user programs use CRT0): BIND -A
rem starts at the first instruction of the first object, and d2a patches the
rem ENDBRK_ word at DS:000A.  DCRT0 is CRT0 with SYSCALL_ returning -1 rather
rem than -errno, which is what the vintage tools' `== -1` tests need.
echo.
echo === XV6 runtime ===
for %%f in (CRT0 DCRT0 V6MISC) do asm88 %%f
for %%f in (SYSCALL USTR DCOMPAT V6DOS V6CHAIN PRINTF D2A E2C MKBOOT) do c88 %%f
if not exist CRT0.O goto bad
if not exist DCRT0.O goto bad
if not exist V6CHAIN.O goto bad
if not exist D2A.O goto bad
if not exist E2C.O goto bad
if not exist MKBOOT.O goto bad

rem --- d2a: the DOS build converts every .exe below, and the -A build is
rem     itself a target (/bin/d2a), so a.out can also be made on RealXV6 ---
echo.
echo === XV6 d2a ===
bind d2a
if not exist D2A.EXE goto bad
bind DCRT0 D2A SYSCALL DCOMPAT PRINTF -A -PD2AV6.MAP -OD2AV6
d2a D2AV6.EXE D2A.AO 1000
if not exist D2A.AO goto bad
echo XV6: d2a OK

rem --- e2c: the same deal for the kernel .COM.  The -A build is /bin/e2c,
rem     so a bootable unix.com can be made on RealXV6 too ---
echo.
echo === XV6 e2c ===
bind e2c
if not exist E2C.EXE goto bad
bind DCRT0 E2C SYSCALL DCOMPAT PRINTF -A -PE2CV6.MAP -OE2CV6
d2a E2CV6.EXE E2C.AO 1000
if not exist E2C.AO goto bad
echo XV6: e2c OK

rem --- mkboot: the step after e2c.  It lays the boot sector and the kernel
rem     .COM onto a 360K floppy image: 720 zero sectors, the sector at 0, the
rem     kernel from 1.  The -A build is /bin/mkboot, so the target can
rem     produce the medium it boots from ---
echo.
echo === XV6 mkboot ===
bind mkboot
if not exist MKBOOT.EXE goto bad
bind DCRT0 MKBOOT SYSCALL DCOMPAT PRINTF -A -PMKBOOTV6.MAP -OMKBOOTV6
d2a MKBOOTV6.EXE MKBOOT.AO 1000
if not exist MKBOOT.AO goto bad
echo XV6: mkboot OK

echo.
echo === XV6 c88 ===
bind -FC88V6.RSP
d2a C88V6.EXE C88.AO 1000
if not exist C88.AO goto bad
echo XV6: c88 OK

echo.
echo === XV6 gen ===
bind -FGENV6.RSP
d2a GENV6.EXE GEN.AO 1000
if not exist GEN.AO goto bad
echo XV6: gen OK

echo.
echo === XV6 asm88 ===
bind -FASM88V6.RSP
d2a ASM88V6.EXE ASM88.AO 1000
if not exist ASM88.AO goto bad
echo XV6: asm88 OK

echo.
echo === XV6 dbind ===
bind -FBINDV6.RSP
d2a BINDV6.EXE DBIND.AO 1000
if not exist DBIND.AO goto bad
echo XV6: dbind OK

echo.
echo === XV6 dlib88 ===
bind -FLIB88V6.RSP
d2a LIB88V6.EXE DLIB88.AO 1000
if not exist DLIB88.AO goto bad
echo XV6: dlib88 OK

rem --- publish, like STAGE1/STAGE2 do.  DOS has no 4-letter extension, so
rem     they keep the .AO one d2a writes.  Delete before
rem     copying so a stale target cannot survive and so COPY is never in a
rem     position to ask about overwriting inside a batch.
if not exist \BUILD\XV6BIN\NUL md \BUILD\XV6BIN
if exist \BUILD\XV6BIN\*.AO del \BUILD\XV6BIN\*.AO
copy C88.AO    \BUILD\XV6BIN
copy GEN.AO    \BUILD\XV6BIN
copy ASM88.AO  \BUILD\XV6BIN
copy DBIND.AO  \BUILD\XV6BIN
copy DLIB88.AO \BUILD\XV6BIN
copy D2A.AO    \BUILD\XV6BIN
copy E2C.AO    \BUILD\XV6BIN
copy MKBOOT.AO \BUILD\XV6BIN
echo.
echo XV6: ALL OK - published to C:\BUILD\XV6BIN
goto end
:bad
echo.
echo XV6: FAILED
goto end
:nostage2
echo XV6: C:\BUILD\STAGE2 is empty or incomplete - run STAGE1 then STAGE2 first.
goto end
:noobj
echo XV6: no compiler pass objects in C:\BUILD - run STAGE1 then STAGE2 first.
:end
