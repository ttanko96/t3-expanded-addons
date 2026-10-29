@echo off
rem TEMPORARY: the Expanded's setup, then the TMST add-on's (setup\tmst_setup.py),
rem then the game built again with the add-on. Once the Expanded's add-on hook
rem exists, its own setup runs ours and this file goes.
setlocal
set "ROOT=%~dp0..\..\.."
call "%ROOT%\Setup Tekken 3.cmd"
if errorlevel 1 exit /b %errorlevel%
set "PY=%ROOT%\.setup\venv\Scripts\python.exe"
"%PY%" "%~dp0..\setup\tmst_setup.py" %*
if errorlevel 1 exit /b %errorlevel%
"%PY%" "%~dp0build.py"
exit /b %errorlevel%
