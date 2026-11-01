@echo off
rem The TMST add-on for Tekken 3 Expanded: its setup, then the game built again
rem with it. Run after the Expanded's own setup (Setup Tekken 3.cmd), with the
rem game closed. Drop a Tekken 2 disc (.cue, .bin or .chd) on this file to give it.
rem It reuses the Expanded's .setup\venv and toolchain; nothing of the
rem Expanded's own files is changed.
setlocal
cd /d "%~dp0"
for %%i in ("%~dp0..\..") do set "ROOT=%%~fi"
set "PY=%ROOT%\.setup\venv\Scripts\python.exe"
set "T2DISC=%~1"
title TMST add-on setup
echo TMST add-on setup
echo Tekken 3 Expanded: %ROOT%
echo.
if not exist "%ROOT%\game.toml" (
    echo This folder is not inside Tekken 3 Expanded.
    echo Put it exactly at ^<Tekken 3 Expanded^>\addons\tmst and run this again.
    goto failed
)
if not exist "%PY%" goto not_set_up
if not exist "%ROOT%\.setup\ready.json" goto not_set_up
if not exist "%ROOT%\build-release\CMakeCache.txt" goto not_set_up
tasklist /fi "imagename eq Tekken_3_Expanded.exe" 2>nul | "%SystemRoot%\System32\find.exe" /i "Tekken_3_Expanded.exe" >nul && (
    echo Tekken 3 Expanded is running: close the game, then run this again.
    goto failed
)
if defined T2DISC (
    "%PY%" setup\tmst_setup.py --interactive --t2-disc "%T2DISC%" || goto failed
) else (
    "%PY%" setup\tmst_setup.py --interactive || goto failed
)
echo.
echo Building the game with the add-on. The first time this takes several minutes.
"%PY%" dev\build.py || goto failed
echo.
echo The TMST add-on is installed. Start Tekken 3 Expanded, open the Mods tab
echo and switch on the mods of the TMST group, then restart the game.
pause
exit /b 0

:not_set_up
echo Tekken 3 Expanded has not been set up yet.
echo Run "Setup Tekken 3.cmd" in %ROOT% first and let it finish
echo ^(the game starts^). Close the game, then run this again.
:failed
echo.
echo TMST setup stopped: see the lines above.
pause
exit /b 1
