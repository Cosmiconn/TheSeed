#!/usr/bin/env bash
set -euo pipefail

# TheSeed Linter
# Usage: ./scripts/lint.sh

echo "=== TheSeed Lint ==="

BUILD_DIR="build/linux-debug"

# clang-tidy
if command -v clang-tidy &> /dev/null; then
    echo "--- Running clang-tidy ---"
    if [ -f "${BUILD_DIR}/compile_commands.json" ]; then
        run-clang-tidy -p "${BUILD_DIR}" submodules/seed-core/src/ submodules/seed-core/tests/ || true
    else
        echo "WARNING: compile_commands.json not found. Run cmake configure first."
    fi
else
    echo "WARNING: clang-tidy not found"
fi

# cppcheck
if command -v cppcheck &> /dev/null; then
    echo "--- Running cppcheck ---"
    cppcheck --enable=all \
        -I submodules/seed-core/src \
        --suppress=missingIncludeSystem \
        --suppress=missingInclude \
        --suppress=unusedFunction \
        --suppress=constVariablePointer \
        --suppress=knownConditionTrueFalse \
        --error-exitcode=0 \
        submodules/seed-core/src/ submodules/seed-core/tests/ || true
else
    echo "WARNING: cppcheck not found"
fi

echo "=== Lint complete ==="
