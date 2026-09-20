@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "OUT=%ROOT%build"
set "CFLAGS=/nologo /W4 /WX /O2 /MT /D_CRT_SECURE_NO_WARNINGS"
set "LIBS=psapi.lib"

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

cl /? >nul 2>nul
if errorlevel 1 (
  if not exist "!VSWHERE!" (
    echo MSVC bulunamadi. Developer Command Prompt icinden calistir.
    exit /b 1
  )
  for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
  if not defined VSPATH (
    echo MSVC araclari kurulu degil.
    exit /b 1
  )
  call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist "%OUT%" mkdir "%OUT%"

cl %CFLAGS% /Fe:"%OUT%\procmon.exe" /Fo:"%OUT%\\" "%ROOT%src\main.c" "%ROOT%src\args.c" "%ROOT%src\proc_common.c" "%ROOT%src\proc_win.c" "%ROOT%src\render.c" %LIBS%
if errorlevel 1 exit /b 1

cl %CFLAGS% /Fe:"%OUT%\test_args.exe" /Fo:"%OUT%\\" "%ROOT%tests\test_args.c" "%ROOT%src\args.c"
if errorlevel 1 exit /b 1

cl %CFLAGS% /Fe:"%OUT%\test_model.exe" /Fo:"%OUT%\\" "%ROOT%tests\test_model.c" "%ROOT%src\proc_common.c" "%ROOT%src\proc_win.c" %LIBS%
if errorlevel 1 exit /b 1

if /i "%~1"=="test" (
  "%OUT%\test_args.exe" || exit /b 1
  "%OUT%\test_model.exe" || exit /b 1
)

echo build tamam: %OUT%
