@echo off
setlocal

set "QUATTRO_ROOT=%~dp0.."
set "QUATTRO_BUILD=%QUATTRO_ROOT%\build-local-windows"
if not defined QUATTRO_VERSION set "QUATTRO_VERSION=0.1.3"
set "VS_VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
rem CMake/Ninja records MSVC /showIncludes output verbatim. Use UTF-8 so a
rem localized compiler prefix is preserved instead of becoming question marks.
chcp 65001 >nul

if not exist "%VS_VCVARS%" (
  echo Visual Studio 2022 Build Tools were not found.
  exit /b 1
)

call "%VS_VCVARS%"
if errorlevel 1 exit /b %errorlevel%

if not exist "%QUATTRO_BUILD%" mkdir "%QUATTRO_BUILD%"
if errorlevel 1 exit /b %errorlevel%

rem Use the commit-pinned and SHA-256-checked routing catalogue.
python "%QUATTRO_ROOT%\script\fetch_srslist.py" "%QUATTRO_BUILD%"
if errorlevel 1 exit /b 1

"%ProgramFiles%\CMake\bin\cmake.exe" ^
  -S "%QUATTRO_ROOT%" ^
  -B "%QUATTRO_BUILD%" ^
  -GNinja ^
  -DCMAKE_MAKE_PROGRAM="%QUATTRO_ROOT%\tools\ninja.exe" ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_PREFIX_PATH="%QUATTRO_ROOT%\tools\Qt\lib\cmake" ^
  -DOPENSSL_ROOT_DIR="%QUATTRO_ROOT%\tools\openssl" ^
  -DINPUT_VERSION=%QUATTRO_VERSION% ^
  -DNKR_PACKAGE=1
if errorlevel 1 exit /b %errorlevel%

"%ProgramFiles%\CMake\bin\cmake.exe" --build "%QUATTRO_BUILD%" --parallel 2
exit /b %errorlevel%
