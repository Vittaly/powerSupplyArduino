@echo off
setlocal EnableDelayedExpansion
set file=%1

for /F "tokens=1-3 delims=N " %%a in (%file%) do (
   for %%A in (%%a %%b %%c) do set "i=%%A"
)
echo %i%
set /a i=i+1
echo #define BUILD_N %i% > %file%