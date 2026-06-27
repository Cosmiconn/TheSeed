@echo off
setlocal enabledelayedexpansion

echo ============================================
echo The Seed Engine V13.1 - Build Script
echo ============================================

if not exist "vcpkg\" (
    echo [1/4] Cloning vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git
    call vcpkg\bootstrap-vcpkg.bat
) else (
    echo [1/4] vcpkg already exists
)

echo [2/4] Installing dependencies via vcpkg...
vcpkg\vcpkg install --triplet x64-windows

echo [3/4] Configuring CMake...
if not exist "build\" mkdir build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake -DENGINE_BUILD_EDITOR=ON

echo [4/4] Building...
cmake --build build --config Release --parallel

echo ============================================
echo Build complete!
echo Executable: build\Release\TheSeed.exe
echo ============================================
pause
