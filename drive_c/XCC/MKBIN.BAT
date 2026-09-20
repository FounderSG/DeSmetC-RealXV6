@echo off
rem MKBIN - build EVERY program the RealXV6 image carries, from source, on DOS.
rem
rem   CROSS first; then, from C:\XCC where CROSS leaves the session:  MKBIN
rem
rem Out comes C:\XV6\BIN: one NAME.AO per program, RealXV6 a.out, built by
rem the toolchain CROSS drove to its fixpoint.  The image description reaches
rem them through its $BIN token, so mkfs lays each one down under its
rem lower-case /bin name.
rem
rem Two halves, because they link two different runtimes.  The toolchain
rem ports are C:\BUILD\XV6.BAT's job -- DCRT0 plus DCOMPAT, V6DOS, V6MISC
rem and V6CHAIN -- and it needs what a STAGE2 run leaves behind, which CROSS
rem has just produced.  Everything else is one C:\XCC\XCC.BAT call per
rem program, the same recipe as compiling one by hand.
rem
rem The build happens in C:\XV6\SRC, where XV6.BAT has just staged C:\XV6\RT,
rem so only the application sources have to be copied in.  Nothing is ever
rem copied over an existing file: DOS COPY asks before overwriting, and a
rem batch that asks anything is a batch that hangs.
rem
rem No redirection character appears above, not even inside a rem: DOS strips
rem redirection before it works out what the command is, so an arrow in a
rem comment IS a redirect.  To capture a whole run, redirect from OUTSIDE:
rem     command /c \XCC\MKBIN.BAT  then the redirect  \MKBIN.LOG
c:
if not exist \XCC\XCC.BAT goto noxcc
if not exist \XCC\C88.EXE goto noxcc
if not exist \BUILD\XV6.BAT goto nobat

rem --- where the a.out are published.  Emptied first, so a program that
rem     fails to build cannot leave the last run's copy standing, and so
rem     COPY is never in a position to ask about an overwrite.
if not exist \XV6\BIN\NUL md \XV6\BIN
if exist \XV6\BIN\*.AO del \XV6\BIN\*.AO

echo.
echo === MKBIN 1 of 2: the toolchain ports (BUILD\XV6.BAT) ===
call \BUILD\XV6.BAT
c:
if not exist \BUILD\XV6BIN\C88.AO goto notools
cd \XV6\SRC
if not exist DRUN.C goto nosrc

rem --- XV6.BAT leaves its own .AO behind in the scratch directory, but the
rem     copies that count are the ones it published.  Drop them, so the
rem     wildcard copy at the end names only what this half builds.
if exist *.AO del *.AO
copy \XV6\USR\*.C
copy \XCC\HELLO.C

echo.
echo === MKBIN 2 of 2: init, the user programs, the ported tools ===
call \XCC\XCC.BAT INIT
if not exist INIT.AO goto bad
call \XCC\XCC.BAT CAT
if not exist CAT.AO goto bad
call \XCC\XCC.BAT CHMOD
if not exist CHMOD.AO goto bad
call \XCC\XCC.BAT CP
if not exist CP.AO goto bad
call \XCC\XCC.BAT DATE
if not exist DATE.AO goto bad
call \XCC\XCC.BAT ECHO
if not exist ECHO.AO goto bad
call \XCC\XCC.BAT ED
if not exist ED.AO goto bad
call \XCC\XCC.BAT FIB
if not exist FIB.AO goto bad
call \XCC\XCC.BAT FORKTEST
if not exist FORKTEST.AO goto bad
call \XCC\XCC.BAT GREP
if not exist GREP.AO goto bad
call \XCC\XCC.BAT ID
if not exist ID.AO goto bad
call \XCC\XCC.BAT KILL
if not exist KILL.AO goto bad
call \XCC\XCC.BAT LN
if not exist LN.AO goto bad
call \XCC\XCC.BAT LOGIN
if not exist LOGIN.AO goto bad
call \XCC\XCC.BAT LS
if not exist LS.AO goto bad
call \XCC\XCC.BAT MKDIR
if not exist MKDIR.AO goto bad
call \XCC\XCC.BAT MKFS
if not exist MKFS.AO goto bad
call \XCC\XCC.BAT MKNOD
if not exist MKNOD.AO goto bad
call \XCC\XCC.BAT MORE
if not exist MORE.AO goto bad
call \XCC\XCC.BAT MOUNT
if not exist MOUNT.AO goto bad
call \XCC\XCC.BAT NICE
if not exist NICE.AO goto bad
call \XCC\XCC.BAT PS
if not exist PS.AO goto bad
call \XCC\XCC.BAT PWD
if not exist PWD.AO goto bad
call \XCC\XCC.BAT RM
if not exist RM.AO goto bad
call \XCC\XCC.BAT SH
if not exist SH.AO goto bad
call \XCC\XCC.BAT SIGTEST
if not exist SIGTEST.AO goto bad
call \XCC\XCC.BAT SLEEP
if not exist SLEEP.AO goto bad
call \XCC\XCC.BAT STTY
if not exist STTY.AO goto bad
call \XCC\XCC.BAT SYNC
if not exist SYNC.AO goto bad
call \XCC\XCC.BAT UMOUNT
if not exist UMOUNT.AO goto bad
call \XCC\XCC.BAT WC
if not exist WC.AO goto bad
call \XCC\XCC.BAT DCMP
if not exist DCMP.AO goto bad
call \XCC\XCC.BAT DRUN
if not exist DRUN.AO goto bad
call \XCC\XCC.BAT REBOOT
if not exist REBOOT.AO goto bad
call \XCC\XCC.BAT FDTEST
if not exist FDTEST.AO goto bad
call \XCC\XCC.BAT HELLO
if not exist HELLO.AO goto bad
call \XCC\XCC.BAT ZOMBIE
if not exist ZOMBIE.AO goto bad

copy *.AO \XV6\BIN
copy \BUILD\XV6BIN\C88.AO \XV6\BIN
copy \BUILD\XV6BIN\GEN.AO \XV6\BIN
copy \BUILD\XV6BIN\ASM88.AO \XV6\BIN
copy \BUILD\XV6BIN\DBIND.AO \XV6\BIN
copy \BUILD\XV6BIN\DLIB88.AO \XV6\BIN
copy \BUILD\XV6BIN\D2A.AO \XV6\BIN
copy \BUILD\XV6BIN\E2C.AO \XV6\BIN
copy \BUILD\XV6BIN\MKBOOT.AO \XV6\BIN
if not exist \XV6\BIN\SH.AO goto badpub
if not exist \XV6\BIN\C88.AO goto badpub

echo.
echo ==============================================================
echo  MKBIN: OK - every program the image carries is in C:\XV6\BIN,
echo  built here from the source beside it.  Next, on the host:
echo      python scripts/mkfs.py unix.img image/boot/proto ...
echo  with -D BIN=drive_c/XV6/BIN, and see the README.
echo ==============================================================
dir \XV6\BIN\*.AO
goto end

:bad
echo.
echo MKBIN: FAILED - the XCC line above names the program that did not build.
goto end
:notools
echo.
echo MKBIN: FAILED - XV6.BAT published no C88.AO to C:\BUILD\XV6BIN.
echo MKBIN: it needs C:\BUILD\STAGE2 and the pass objects a STAGE2 run
echo MKBIN: leaves behind.  Run CROSS first.
goto end
:nosrc
echo.
echo MKBIN: FAILED - C:\XV6\SRC does not hold the staged C:\XV6\RT source.
goto end
:badpub
echo.
echo MKBIN: FAILED - C:\XV6\BIN was not filled.
goto end
:nobat
echo.
echo MKBIN: FAILED - C:\BUILD\XV6.BAT is missing from this tree.
goto end
:noxcc
echo.
echo MKBIN: FAILED - no cross compiler in C:\XCC.  Run CROSS first.
:end
