#!/bin/bash
set -e

# Usage: ./test.sh [filter] [preset]
# Example: ./test.sh unit linux-debug

FILTER=${1:-".*"}
PRESET=${2:-"linux-release"}
BUILD_DIR="build/$PRESET"

echo "=== Running Tests ($PRESET) ==="
ctest --test-dir "$BUILD_DIR" -R "$FILTER" --output-on-failure -j$(nproc)

if [ "$PRESET" == "linux-debug" ]; then
    echo "=== Running Sanitizer Tests ==="
    ctest --test-dir "$BUILD_DIR" -R "sanitizer" --output-on-failure || true
fi

echo "=== Tests complete ==="
