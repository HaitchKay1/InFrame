@echo off
set "STEADYFRAME_INSTALL_SCRIPT=%~dp0Install-SteadyFrame.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File ""' + $env:STEADYFRAME_INSTALL_SCRIPT + '""'); exit $p.ExitCode"
if errorlevel 1 pause
