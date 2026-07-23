#!/bin/bash
set -e

echo "=== TheSeed Setup ==="

# 1. Git Submodules
echo "[1/4] Initializing submodules..."
git submodule update --init --recursive

# 2. vcpkg
echo "[2/4] Setting up vcpkg..."
if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
fi

# 3. Install dependencies
echo "[3/4] Installing dependencies..."
./vcpkg/vcpkg install --triplet x64-linux

# 4. Create build directory
echo "[4/4] Creating build directory..."
mkdir -p build

echo "=== Setup complete ==="
echo "Build with: cmake --preset linux-release"
