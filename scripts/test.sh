#!/usr/bin/env bash
set -euo pipefail

# TheSeed Test Runner
# Usage: ./scripts/test.sh [filter] [config]
#   filter: regex for test names (default: ".*")
#   config: debug | release (default: debug)

FILTER="${1:-.*}"
CONFIG="${2:-debug}"
PRESET="linux-${CONFIG}"
BUILD_DIR="build/linux-${CONFIG}"

echo "=== TheSeed Test Runner ==="
echo "Filter: ${FILTER}"
echo "Config: ${CONFIG}"
echo "Build dir: ${BUILD_DIR}"
echo ""

# Ensure build exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo "Build directory not found. Configuring..."
    cmake --preset "${PRESET}"
    cmake --build "${BUILD_DIR}" --target seed_tests --parallel
fi

# Run tests with filter
echo "--- Running tests ---"
ctest --test-dir "${BUILD_DIR}" -R "${FILTER}" --output-on-failure -j$(nproc)

echo ""
echo "=== Test run complete ==="
