#!/bin/bash
set -e

echo "============================================"
echo "The Seed Engine V13.1 - Build Script"
echo "============================================"

if [ ! -d "vcpkg" ]; then
    echo "[1/4] Cloning vcpkg..."
    git clone https://github.com/Microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
else
    echo "[1/4] vcpkg already exists"
fi

echo "[2/4] Installing dependencies via vcpkg..."
./vcpkg/vcpkg install --triplet x64-linux

echo "[3/4] Configuring CMake..."
mkdir -p build
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DENGINE_BUILD_EDITOR=ON \
    -DCMAKE_BUILD_TYPE=Release

echo "[4/4] Building..."
cmake --build build --parallel $(nproc)

echo "============================================"
echo "Build complete!"
echo "Executable: build/TheSeed"
echo "============================================"
