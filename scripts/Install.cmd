@echo off
set "INFRAME_INSTALL_SCRIPT=%~dp0Install-InFrame.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File ""' + $env:INFRAME_INSTALL_SCRIPT + '""'); exit $p.ExitCode"
if errorlevel 1 pause
