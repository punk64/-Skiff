@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

if not defined DEVECO_HOME set "DEVECO_HOME=D:\Program Files\Huawei\DevEco Studio"
if not defined HVIGOR_USER_HOME set "HVIGOR_USER_HOME=D:\WorkBuddy_Work\MyEnv\.hvigor"
set "PATH=%DEVECO_HOME%\tools\node;%PATH%"
set "SKIFF_NO_PAUSE=1"
set "NOPAUSE="
echo %*| findstr /c:"--no-pause" >nul 2>nul
if not errorlevel 1 set "NOPAUSE=1"

set "PY=python"
where python >nul 2>nul
if errorlevel 1 set "PY=py"
where %PY% >nul 2>nul
if errorlevel 1 (
  echo [skiff][FAIL] Python not found in PATH.
  echo.
  pause
  exit /b 1
)

%PY% "%~dp0release.py" %*
set "RC=%ERRORLEVEL%"

echo.
echo ------------------------------------------------------------
echo exit code: %RC%
echo ------------------------------------------------------------
if not defined NOPAUSE pause
exit /b %RC%
