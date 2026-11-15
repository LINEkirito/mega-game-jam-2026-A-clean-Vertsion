@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Game_Jam_Clean_Vers\Scripts\OpenProject.ps1" %*
set "result=%ERRORLEVEL%"
if not "%result%"=="0" pause
exit /b %result%
