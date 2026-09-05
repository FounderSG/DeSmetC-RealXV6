@echo off
rem BB NAME : quick compile (C88) + link (BIND) helper for interactive use.
rem Produces NAME.EXE in the current directory.
c88 %1
if errorlevel 1 goto err
bind %1
if errorlevel 1 goto err
echo [BB] OK: %1.EXE
goto end
:err
echo [BB] FAILED: %1
:end
