@echo off
rem Launches ClassMetrics 2.0 (separate copy: own data folder next to this file).
rem GTK libraries come from MSYS2 (C:\msys64\mingw64\bin).
set "PATH=C:\msys64\mingw64\bin;%PATH%;C:\msys64\usr\bin"
cd /d "%~dp0"

rem Already open? A running program locks the .exe and cannot be rebuilt.
tasklist /fi "imagename eq ClassMetrics_V2.exe" 2>nul | "%SystemRoot%\System32\find.exe" /i "ClassMetrics_V2.exe" >nul
if not errorlevel 1 (
    echo ClassMetrics 2.0 is already running. Close it first to start a freshly built copy.
    timeout /t 4 >nul
    exit /b 0
)

rem Always rebuild first: make only recompiles what changed, so this is quick when nothing changed.
call "%~dp0build.bat"
if errorlevel 1 (
    echo.
    echo Build failed - see the messages above. The program was not started.
    pause
    exit /b 1
)
if not exist ClassMetrics_V2.exe (
    echo ClassMetrics_V2.exe was not created.
    pause
    exit /b 1
)
start "" ClassMetrics_V2.exe
