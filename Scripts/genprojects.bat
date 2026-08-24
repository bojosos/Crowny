@echo off
setlocal
pushd "%~dp0.."
3rdparty\premake\bin\premake5.exe vs2022 --with-nodes %*
set "CROWNY_EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %CROWNY_EXIT_CODE%
