#!/usr/bin/env bash
set -euo pipefail

echo "=========================================================="
echo "  Compiling and Running TreeMCP Test Suite (Linux)"
echo "=========================================================="

CXX="${CXX:-g++}"

${CXX} -O3 -std=c++17 \
    -flto -s \
    -ffunction-sections -fdata-sections \
    -fno-exceptions -fno-rtti \
    -fstack-protector-strong \
    -D_FORTIFY_SOURCE=2 \
    -Wl,--gc-sections \
    tests/test_suite.cpp \
    src/core/file_tree.cpp \
    src/core/treemap.cpp \
    src/core/security.cpp \
    src/mcp/token_optimizer.cpp \
    -o test_runner \
    -pthread

echo ""
echo "Running tests..."
./test_runner
rm -f test_runner

echo "[SUCCESS] All tests passed!"
