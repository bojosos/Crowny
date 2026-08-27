@echo off
if not defined CROWNY_REAL_CL goto msvc_not_configured
where sccache.exe >nul 2>nul
if errorlevel 1 goto msvc
sccache.exe "%CROWNY_REAL_CL%" %*
if not errorlevel 1 exit /b 0

:msvc
"%CROWNY_REAL_CL%" %*
exit /b %errorlevel%

:msvc_not_configured
echo CROWNY_REAL_CL is not configured. 1>&2
exit /b 2
