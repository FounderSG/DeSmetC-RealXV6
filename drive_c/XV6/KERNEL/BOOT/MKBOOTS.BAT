@echo off
rem MKBOOTS - assemble the RealXV6 boot sector with DeSmet ASM88.
rem Driven headlessly by scripts\build_bootsect.py, which turns the .EXE
rem this leaves into the 512-byte sector (strip the MZ header, zero-fill to
rem 510, stamp 55 AA).  On the target the same two steps are mkimg's, and
rem the cut is made by mkboot -s.
rem
rem The REBUILT toolchain assembles it, like everything else past the stage-1
rem fixpoint.  The shipped ASM88 gives the same 512 bytes -- but this sector
rem is the golden the target's own asm88 is dcmp'd against, and a golden made
rem by a compiler the target does not have is one that only happens to match.
rem C:\DESMET\BIN is therefore off the PATH, where it could only ever be a
rem silent fallback; C:\DESMET\LIB stays, for the CSTDIO.S that BIND opens on
rem every link.
rem
rem BIND -A is what makes the first instruction of the first object the
rem image entry, so BOOTSEC lands at CS:0 and the sector image starts with
rem it.
path C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
c:
cd \XV6\KERNEL\BOOT
if exist BOOTSEC.O del BOOTSEC.O
if exist BOOTSEC.EXE del BOOTSEC.EXE
if exist BOOTSEC.MAP del BOOTSEC.MAP
echo.
echo === ASM88 BOOTSEC ===
asm88 BOOTSEC
if not exist BOOTSEC.O goto bad
echo.
echo === BIND BOOTSEC ===
bind BOOTSEC -A -PBOOTSEC.MAP -OBOOTSEC
if not exist BOOTSEC.EXE goto bad
echo MKBOOTS: OK
goto end
:bad
echo MKBOOTS: FAILED
:end
