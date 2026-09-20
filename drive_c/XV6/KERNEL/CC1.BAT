@echo off
rem CC1 NAME : compile one kernel source, appending to ..\BUILD.LOG.
rem Called from KEN\ or DMR\ by MKKERN.BAT, so the log is one level up.
echo --- C88 %1 --- >> ..\BUILD.LOG
c88 %1 >> ..\BUILD.LOG
