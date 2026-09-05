@echo off
rem XCC - compile one RealXV6 application, on DOS, to a RealXV6 a.out.
rem
rem   XCC PROG              PROG.C in the current directory, then PROG.AO
rem   XCC PROG 2000         ... with a 2000H byte stack instead of 1000H
rem
rem Three steps, the same three the DeSmet toolchain always runs, with the
rem target swapped underneath them:
rem
rem   C88   PROG.C to PROG.O          (C88 chains to GEN; both from C:\XCC)
rem   BIND  -A, CRT0 first, then the V6 runtime instead of DOS's CSTDIO,
rem         giving a small-model MZ .exe that is not a DOS program at all:
rem         no PSP, no DOS startup, entry at the first instruction
rem   D2A   that .exe to RealXV6 separated-I and D a.out, magic 0411
rem
rem PROG must fit a DOS 8.3 basename -- and 8 characters is also what keeps
rem the BIND line below inside the 127-character DOS command line, which is
rem why the runtime lives in a directory named as briefly as C:\XCC and why
rem no -P map is asked for.  None is needed: BIND writes e_sp = datatotal +
rem 7FH under -A, so d2a recovers the static data size from the .exe header
rem alone (see the layout notes at the top of D2A.C).
rem
rem UNIX.H is found through C88's -i, not through DSINC: DSINC only covers
rem the angle-bracket form, and RealXV6 sources spell it #include "UNIX.H".
rem That is measured, not assumed -- with DSINC alone C88 says "cannot open
rem UNIX.H".  So -i is what lets a program be compiled where it lies, with
rem no copy of the target header dropped beside it.  DSINC is set too, for
rem a program that prefers the angle-bracket spelling.
rem
rem The PATH matters twice below: it is how C88 finds GEN, and how BIND finds
rem CSTDIO.S, which it opens as the last file of EVERY link whether or not
rem anything is taken from it (nothing is here -- the V6 runtime resolves
rem first).  Both live in C:\XCC, so one entry covers both.
rem
rem The PATH is set HERE and nowhere else: C:\XCC IS the cross compiler, and
rem after this bat runs, the session no longer has the DOS toolchain in
rem front.  DSINC is the target's header directory only, so a RealXV6
rem program cannot quietly pick up a DOS header.
rem
rem No redirection character appears above, not even inside a rem: DOS strips
rem redirection before it works out what the command is, so an arrow in a
rem comment IS a redirect.
if .%1.==.. goto usage
path C:\XCC;Z:\
set DSINC=C:\XCC\
if exist %1.AO del %1.AO
if exist %1.EXE del %1.EXE

c88 %1 -iC:\XCC\
if not exist %1.O goto nocomp

bind C:\XCC\CRT0 %1 C:\XCC\SYSCALL C:\XCC\ULIB C:\XCC\PRINTF C:\XCC\CTIME C:\XCC\SETEXIT C:\XCC\UCOMPAT -A -O%1
if not exist %1.EXE goto nolink

if .%2.==.. goto defstk
d2a %1.EXE %1.AO %2
goto conv
:defstk
d2a %1.EXE %1.AO 1000
:conv
if not exist %1.AO goto noconv
echo XCC: %1.AO OK
goto end

:nocomp
echo XCC: FAILED - C88 did not produce %1.O
goto end
:nolink
echo XCC: FAILED - BIND did not produce %1.EXE
goto end
:noconv
echo XCC: FAILED - d2a did not produce %1.AO
goto end
:usage
echo usage: XCC PROG [stackhex]     compile PROG.C to the RealXV6 a.out PROG.AO
:end
