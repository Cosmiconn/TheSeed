@echo off
setlocal enabledelayedexpansion

set "PRESET=%~1"
if "%~1"=="" set "PRESET=windows-release"
set "BUILD_DIR=build\%PRESET%"

echo === TheSeed Build ===
echo Preset: %PRESET%
echo Build dir: %BUILD_DIR%
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake not found ^(need ^>= 3.25^)
    exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo ERROR: ninja not found
    exit /b 1
)

if "%VCPKG_ROOT%"=="" (
    echo INFO: VCPKG_ROOT not set, probing default locations...
    if exist "C:\vcpkg" set "VCPKG_ROOT=C:\vcpkg"
    if exist "%USERPROFILE%\vcpkg" set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    if exist "%USERPROFILE%\source\vcpkg" set "VCPKG_ROOT=%USERPROFILE%\source\vcpkg"
)

if "%VCPKG_ROOT%"=="" (
    echo ERROR: vcpkg not found. Install vcpkg and set VCPKG_ROOT.
    exit /b 1
)

set "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

echo --- Configuring ---
cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1

echo.
echo --- Building ---
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b 1

echo %PRESET% | findstr "debug" >nul
if not errorlevel 1 (
    echo.
    echo --- Testing ---
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
)
if "%~2"=="--test" (
    echo.
    echo --- Testing ---
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
)

echo.
echo === Build Complete ===
