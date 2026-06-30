#!/usr/bin/env bash
# ==============================================================================
# HydraFlow Build Script
# Usage: ./scripts/build.sh [Debug|Release] [--tests] [--examples] [--clean]
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_TYPE="${1:-Debug}"
BUILD_DIR="${ROOT_DIR}/build/${BUILD_TYPE}"
ENABLE_TESTS="ON"
ENABLE_EXAMPLES="ON"
CLEAN_BUILD=false

# Parse extra flags
for arg in "$@"; do
    case "$arg" in
        --clean)    CLEAN_BUILD=true ;;
        --no-tests) ENABLE_TESTS="OFF" ;;
        --no-examples) ENABLE_EXAMPLES="OFF" ;;
    esac
done

echo "============================================"
echo " HydraFlow Build"
echo " Build type : ${BUILD_TYPE}"
echo " Build dir  : ${BUILD_DIR}"
echo " Tests      : ${ENABLE_TESTS}"
echo " Examples   : ${ENABLE_EXAMPLES}"
echo "============================================"

if [ "${CLEAN_BUILD}" = true ] && [ -d "${BUILD_DIR}" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

cmake \
    -S "${ROOT_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DHYDRAFLOW_BUILD_TESTS="${ENABLE_TESTS}" \
    -DHYDRAFLOW_BUILD_EXAMPLES="${ENABLE_EXAMPLES}" \
    -G Ninja

cmake --build "${BUILD_DIR}" --parallel

echo ""
echo "Build complete: ${BUILD_DIR}/bin/"
