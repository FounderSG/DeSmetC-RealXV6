@echo off
rem MKKERN.BAT - compile the RealXV6 kernel with DeSmet C 3.03.
rem
rem Each source is compiled in its own directory (KEN, DMR) so C88 drops the
rem .O next to it; DSINC carries the kernel's own H\ ahead of the DeSmet
rem headers, which is how #include "os.h" resolves without touching the
rem source.  The trailing backslash on every DSINC prefix is required.
rem
rem Everything is appended to BUILD.LOG; the host reads the per-file
rem "Number of Errors" lines out of it, because C88 also returns 1 on a
rem mere warning and its exit status cannot be used to decide success.
rem
rem The REBUILT stage-2 toolchain compiles the kernel -- the same one
rem RealXV6's /bin is linked from.  That is what makes the on-target build
rem (image\tests\kernel.spec) able to reproduce THE KERNEL THAT BOOTED it
rem rather than a second host build of the same source: same source, same
rem toolchain, same bytes.  Building with the shipped 1987 tools instead
rem yields a kernel 90 bytes tighter that nothing on the target can
rem reproduce, since the GPL GEN source is not the 1987 shipping source.
rem
rem C:\BUILD\STAGE2 must therefore be populated -- run STAGE1 then STAGE2
rem first, or just CROSS.  C:\DESMET\BIN is off the PATH above for the
rem same reason it is off every other bat's past the stage-1 fixpoint:
rem in DOS a PATH entry that does not exist is not an error, so leaving
rem it there would quietly fall through to the shipped tools and build a
rem kernel nothing on the target can reproduce.
rem
path C:\BUILD\STAGE2;C:\DESMET\LIB;Z:\
set DSINC=C:\XV6\KERNEL\H\;C:\DESMET\INCLUDE\
c:
cd \XV6\KERNEL

echo === RealXV6 kernel build (DeSmet C 3.03) === > BUILD.LOG

cd KEN
for %%f in (ALLOC CLOCK FIO IGET MAIN MALLOC NAMI PIPE PRF RDWRI) do call ..\CC1.BAT %%f
for %%f in (SIG SLP SUBR SYS1 SYS2 SYS3 SYS4 SYSENT TEXT TRAP) do call ..\CC1.BAT %%f
cd ..

cd DMR
for %%f in (BIO FD IDE KBD KL MEM PC RK TTY UART) do call ..\CC1.BAT %%f
call ..\CC1.BAT DPAD
call ..\CC1.BAT DPAD2
echo --- ASM88 M86 --- >> ..\BUILD.LOG
asm88 M86 >> ..\BUILD.LOG
echo --- ASM88 CPAD --- >> ..\BUILD.LOG
asm88 CPAD >> ..\BUILD.LOG
echo --- ASM88 ZEND --- >> ..\BUILD.LOG
asm88 ZEND >> ..\BUILD.LOG
cd ..

rem Link.  The object list is far past the DOS 127-char command line, so it
rem goes in a BIND response file.  Order is load-bearing: CPAD (code spacer,
rem no DSEG) then DPAD (data spacer, no CSEG) then M86, whose STARTX must
rem therefore land at code offset 100H.  -A suppresses BIND's own CSETUP,
rem which would otherwise be linked ahead of both spacers as file zero.
echo --- BIND --- >> BUILD.LOG
bind -FUNIX.RSP >> BUILD.LOG

echo === END === >> BUILD.LOG
