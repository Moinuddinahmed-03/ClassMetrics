@echo off
rem Builds ClassMetrics_V2.exe with GCC + GTK3 from MSYS2 (only changed files are recompiled).
rem MSYS2's tools go LAST in PATH so they never hide Windows commands such as find.
set "PATH=C:\msys64\mingw64\bin;%PATH%;C:\msys64\usr\bin"
cd /d "%~dp0"
make
exit /b %errorlevel%
