#!/usr/bin/env bash
set -euo pipefail

# TheSeed Code Formatter
# Usage: ./scripts/format.sh [check]
#   check: dry-run, exits with error if formatting needed

MODE="${1:-format}"

echo "=== TheSeed Code Format ==="

if ! command -v clang-format &> /dev/null; then
    echo "ERROR: clang-format not found"
    exit 1
fi

# Find all source files
FILES=$(find submodules/seed-core/src submodules/seed-core/tests -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \))

if [ "$MODE" == "check" ]; then
    echo "--- Checking format (dry-run) ---"
    clang-format --dry-run --Werror $FILES
else
    echo "--- Formatting files ---"
    clang-format -i $FILES
fi

echo "=== Format complete ==="
