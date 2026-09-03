@echo off
setlocal
python "%~dp0..\Tools\crowny" %*
exit /b %ERRORLEVEL%
