#!/usr/bin/env bash
# ==============================================================================
# Run HydraCore unit tests
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_TYPE="${1:-Debug}"
BUILD_DIR="${ROOT_DIR}/build/${BUILD_TYPE}"
TEST_BINARY="${BUILD_DIR}/bin/HydraCore_Tests"

if [ ! -f "${TEST_BINARY}" ]; then
    echo "Test binary not found. Running build first..."
    "${SCRIPT_DIR}/build.sh" "${BUILD_TYPE}"
fi

echo "Running HydraCore unit tests..."
"${TEST_BINARY}" --gtest_color=yes "$@"
