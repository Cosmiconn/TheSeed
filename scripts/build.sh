#!/bin/bash
set -e

# Usage: ./build.sh [preset]
# Example: ./build.sh linux-release

PRESET=${1:-"linux-release"}

echo "=== Building TheSeed ($PRESET) ==="
cmake --preset "$PRESET"
cmake --build "build/$PRESET" --parallel
echo "=== Build complete ==="
