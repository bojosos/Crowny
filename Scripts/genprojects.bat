@echo off
setlocal
call "%~dp0crowny.bat" gen --force
exit /b %ERRORLEVEL%
