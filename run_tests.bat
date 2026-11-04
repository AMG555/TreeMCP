@echo off
setlocal enabledelayedexpansion
echo ==========================================================
echo  Compiling and Running TreeMCP Test Suite (Windows)
echo ==========================================================

g++ -O3 -std=c++17 ^
    -flto -s ^
    -ffunction-sections -fdata-sections ^
    -fno-exceptions -fno-rtti ^
    -fstack-protector-strong ^
    -D_FORTIFY_SOURCE=2 ^
    -Wl,--gc-sections ^
    tests/test_suite.cpp ^
    src/core/file_tree.cpp ^
    src/core/treemap.cpp ^
    src/core/security.cpp ^
    src/mcp/token_optimizer.cpp ^
    -o test_runner.exe ^
    -ladvapi32 -lole32 -lshell32

if errorlevel 1 (
    echo.
    echo [ERROR] Test compilation failed!
    exit /b 1
)

echo.
echo Running tests...
test_runner.exe
set TEST_EXIT=!ERRORLEVEL!

if exist test_runner.exe del test_runner.exe

if !TEST_EXIT! neq 0 (
    echo.
    echo [FAILURE] One or more tests failed!
    exit /b !TEST_EXIT!
)

echo [SUCCESS] All tests passed!
exit /b 0
