@echo off
setlocal

if "%~1"=="" (
  echo Usage: %~nx0 OUTPUT_PML [PROCMON_EXE] [CONFIG_PMC]
  exit /b 1
)

set "OUTPUT_PML=%~1"
set "PROCMON_EXE=%~2"
set "CONFIG_PMC=%~3"

if "%PROCMON_EXE%"=="" if exist "%ProgramFiles%\Procmon64.exe" set "PROCMON_EXE=%ProgramFiles%\Procmon64.exe"
if "%PROCMON_EXE%"=="" if exist "%ProgramFiles(x86)%\Procmon.exe" set "PROCMON_EXE=%ProgramFiles(x86)%\Procmon.exe"
if "%PROCMON_EXE%"=="" set "PROCMON_EXE=Procmon64.exe"

if "%CONFIG_PMC%"=="" (
  "%PROCMON_EXE%" /AcceptEula /Quiet /Minimized /BackingFile "%OUTPUT_PML%"
) else (
  "%PROCMON_EXE%" /AcceptEula /Quiet /Minimized /LoadConfig "%CONFIG_PMC%" /BackingFile "%OUTPUT_PML%"
)
