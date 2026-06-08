#!/bin/bash
# -----------------------------------------------------------------------------
# test-host.sh — build and run a firmware project's HOST unit tests.
#
# "Host" tests compile with the machine's native gcc and run as a normal
# executable. They need NO ESP-IDF and NO hardware, so they are fast and run
# in CI on every push. They live in <firmware>/tests/host/ and use Unity.
#
# Each firmware project (firmware-c6, firmware-h2) has its own independent
# host test suite. This script targets one of them.
#
# Usage:
#   scripts/test-host.sh firmware-c6      # from the repo root
#   scripts/test-host.sh firmware-h2
#   scripts/test-host.sh                  # no arg -> use the current directory
#
# Examples:
#   ./scripts/test-host.sh firmware-c6
#   cd firmware-h2 && ../scripts/test-host.sh      # arg defaults to "."
#
# Exit status is the ctest result, so CI fails when any test fails.
# -----------------------------------------------------------------------------
set -euo pipefail

# First argument is the firmware project directory; default to the current dir.
FW_DIR="${1:-.}"

# Resolve to an absolute path so the build works regardless of where we cd to.
FW_DIR="$(cd "$FW_DIR" && pwd)"

# The host test project (its own CMakeLists.txt, separate from the IDF build).
SRC_DIR="$FW_DIR/tests/host"

# Out-of-tree build dir, kept beside the firmware's IDF build/ (gitignored).
BUILD_DIR="$FW_DIR/build/host"

if [ ! -f "$SRC_DIR/CMakeLists.txt" ]; then
    echo "error: no host tests found at $SRC_DIR/CMakeLists.txt" >&2
    echo "       pass a firmware dir, e.g.: scripts/test-host.sh firmware-c6" >&2
    exit 1
fi

echo ">> Host tests for: $FW_DIR"

# 1. Configure: point CMake at tests/host and select the host platform.
cmake -DTARGET_PLATFORM=host -B "$BUILD_DIR" -G Ninja "$SRC_DIR"

# 2. Compile every test executable.
cmake --build "$BUILD_DIR"

# 3. Run them under ctest. --output-on-failure prints logs only for failures;
#    -j parallelizes across all CPU cores.
ctest --test-dir "$BUILD_DIR" --output-on-failure -j"$(nproc)"
