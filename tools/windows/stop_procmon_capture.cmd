@echo off
setlocal

set "PROCMON_EXE=%~1"

if "%PROCMON_EXE%"=="" if exist "%ProgramFiles%\Procmon64.exe" set "PROCMON_EXE=%ProgramFiles%\Procmon64.exe"
if "%PROCMON_EXE%"=="" if exist "%ProgramFiles(x86)%\Procmon.exe" set "PROCMON_EXE=%ProgramFiles(x86)%\Procmon.exe"
if "%PROCMON_EXE%"=="" set "PROCMON_EXE=Procmon64.exe"

"%PROCMON_EXE%" /Terminate
