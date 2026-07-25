#!/usr/bin/env bash
set -euo pipefail

PRESET="${1:-linux-release}"
BUILD_DIR="build/${PRESET}"

echo "=== TheSeed Build ==="
echo "Preset: ${PRESET}"
echo "Build dir: ${BUILD_DIR}"
echo ""

if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake not found (need >= 3.25)"
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    echo "ERROR: ninja not found"
    exit 1
fi

if [ -z "${VCPKG_ROOT:-}" ]; then
    echo "INFO: VCPKG_ROOT not set, probing default locations..."
    for path in "$HOME/vcpkg" "/usr/local/vcpkg" "/opt/vcpkg"; do
        if [ -d "$path" ]; then
            export VCPKG_ROOT="$path"
            echo "Found vcpkg at: $VCPKG_ROOT"
            break
        fi
    done
fi

if [ -z "${VCPKG_ROOT:-}" ] || [ ! -d "${VCPKG_ROOT}" ]; then
    echo "ERROR: vcpkg not found. Install vcpkg and set VCPKG_ROOT."
    echo "  git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg"
    echo "  ~/vcpkg/bootstrap-vcpkg.sh"
    echo "  export VCPKG_ROOT=~/vcpkg"
    exit 1
fi

export CMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"

echo "--- Configuring ---"
cmake --preset "${PRESET}"

echo ""
echo "--- Building ---"
cmake --build "${BUILD_DIR}" --parallel

if [[ "${PRESET}" == *"debug"* ]] || [[ "${2:-}" == "--test" ]]; then
    echo ""
    echo "--- Testing ---"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

echo ""
echo "=== Build Complete ==="
