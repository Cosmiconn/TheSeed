#!/usr/bin/env bash
set -euo pipefail

echo "=== TheSeed Setup ==="

# 1. Git Submodules
echo "--- Initializing submodules ---"
git submodule update --init --recursive

# 2. vcpkg
echo "--- Checking vcpkg ---"
if [ -z "${VCPKG_ROOT:-}" ]; then
    if [ -d "${HOME}/vcpkg" ]; then
        export VCPKG_ROOT="${HOME}/vcpkg"
    elif [ -d "/usr/local/vcpkg" ]; then
        export VCPKG_ROOT="/usr/local/vcpkg"
    elif [ -d "/opt/vcpkg" ]; then
        export VCPKG_ROOT="/opt/vcpkg"
    fi
fi

if [ -z "${VCPKG_ROOT:-}" ] || [ ! -d "${VCPKG_ROOT}" ]; then
    echo "Cloning vcpkg to ~/vcpkg..."
    export VCPKG_ROOT="${HOME}/vcpkg"
    git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_ROOT"
    "$VCPKG_ROOT/bootstrap-vcpkg.sh"
fi

echo "vcpkg at: $VCPKG_ROOT"

# 3. Install dependencies
echo "--- Installing dependencies ---"
"$VCPKG_ROOT/vcpkg" install --triplet=x64-linux

# 4. Create build directory
mkdir -p build

echo ""
echo "=== Setup complete ==="
echo "Build with: cmake --preset linux-debug"
echo "Or:        ./scripts/build.sh linux-debug"
