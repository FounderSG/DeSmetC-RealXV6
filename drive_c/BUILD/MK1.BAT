@echo off
rem MK1 - rebuild ONE DeSmet C 3.03 toolchain component from its source.
rem
rem   MK1 GEN     component = its directory under C:\BUILD
rem               (C88 GEN ASM BIND LIB88 D88; case does not matter)
rem
rem Runs whichever MAKE*.BAT is staged in that directory and reports
rem whether the MY_*.EXE it should produce came out.  MKALL.BAT calls this
rem once per component; it is equally usable on its own from C:\BUILD, once
rem the PATH points at the compiler you mean to build with.  Like MKALL it
rem sets no environment itself: whatever is already in front is what picks
rem the compiler, so run a stage bat first unless the session already points
rem at one.
rem
rem The component's MAKE bat is CALLed, never run as "command /c BAT" with
rem a redirect: in DOSBox-X ANY redirect performed inside a CALLed bat ends
rem the capture of the enclosing run, so everything from that point on would
rem be missing from the log (see MKALL.BAT).
if .%1.==.. goto usage
echo.
echo === %1 ===
cd \BUILD\%1
if exist MY_*.EXE del MY_*.EXE
for %%b in (MAKE*.BAT) do call %%b
if not exist MY_*.EXE goto fail
echo MK1: %1 OK
cd \BUILD
goto end
:fail
echo MK1: %1 FAILED
cd \BUILD
goto end
:usage
echo usage: MK1 COMPONENT     (C88 GEN ASM BIND LIB88 D88)
:end
