@echo off
setlocal
set VSDIR=D:\Program Files\Microsoft Visual Studio\2022\Professional
set CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
"%CMAKE%" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto fail
"%CMAKE%" --build build
if errorlevel 1 goto fail
echo.
echo [OK] %~dp0build\bin\Skiff.exe
if not defined SKIFF_NO_PAUSE pause
exit /b 0
:fail
echo.
echo [FAIL] build error
if not defined SKIFF_NO_PAUSE pause
exit /b 1
