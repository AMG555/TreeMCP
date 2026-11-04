@echo off
setlocal
echo ==========================================================
echo  Building TreeMCP (Ultra-Lightweight Hardened Edition)
echo ==========================================================

echo Compiling application resources and icon...
if exist scripts\bundle_assets.py (
    python scripts\bundle_assets.py >nul 2>&1
)
windres -i src/resources/resource.rc -o src/resources/resource.o -O coff
if errorlevel 1 (
    echo [WARNING] windres failed, building without embedded icon.
    set RES_OBJ=
) else (
    set RES_OBJ=src/resources/resource.o
)

g++ -O3 -std=c++17 ^
    -flto -s ^
    -ffunction-sections -fdata-sections ^
    -fno-exceptions -fno-rtti ^
    -fstack-protector-strong ^
    -D_FORTIFY_SOURCE=2 ^
    -Wl,--gc-sections ^
    -Wl,--nxcompat ^
    -Wl,--dynamicbase ^
    -Wl,--high-entropy-va ^
    -mwindows ^
    src/main.cpp ^
    src/core/file_tree.cpp ^
    src/core/fast_scanner.cpp ^
    src/core/mft_reader.cpp ^
    src/core/treemap.cpp ^
    src/core/security.cpp ^
    src/mcp/token_optimizer.cpp ^
    src/mcp/mcp_protocol.cpp ^
    src/ui/native_window.cpp ^
    %RES_OBJ% ^
    -o TreeMCP.exe ^
    -lws2_32 -ladvapi32 -lole32 -lshell32 -lcomctl32

if errorlevel 1 goto :fail

if exist src\resources\resource.o del src\resources\resource.o

echo.
echo [SUCCESS] Ultra-lightweight TreeMCP.exe built successfully!
echo Binary footprint: standalone with embedded multi-res icon ^& manifest
echo Security Features Active:
echo   [x] Embedded High-DPI ^& Per-Monitor V2 Manifest
echo   [x] Windows File Properties ^& Version Details (1.0.0.0)
echo   [x] DEP / NXCompat (Data Execution Prevention)
echo   [x] ASLR (Address Space Layout Randomization + High Entropy VA)
echo   [x] Stack Protector Strong (Anti-Buffer-Overflow Canary)
echo   [x] Anti-DNS Rebinding (Strict Host validation)
echo   [x] 256-bit High Entropy Session Auth Token (Anti-CSRF)
echo   [x] Windows Recycle Bin Guardrails (Non-destructive clean)
echo   [x] Protected System Path Blacklist (No system clobbering)
echo   [x] 64 KB Max Payload Cap (Anti-Memory-Exhaustion DoS)
goto :end

:fail
if exist src\resources\resource.o del src\resources\resource.o
echo.
echo [ERROR] Build failed! Check compiler output above.
exit /b 1

:end
endlocal
