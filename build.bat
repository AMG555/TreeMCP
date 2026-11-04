@echo off
setlocal
echo ==========================================================
echo  Building TreeMCP (Hardened Edition) - Security Hardened
echo ==========================================================

g++ -O3 -std=c++17 ^
    -fstack-protector-strong ^
    -D_FORTIFY_SOURCE=2 ^
    -Wl,--nxcompat ^
    -Wl,--dynamicbase ^
    -Wl,--high-entropy-va ^
    src/main.cpp ^
    src/core/file_tree.cpp ^
    src/core/fast_scanner.cpp ^
    src/core/mft_reader.cpp ^
    src/core/treemap.cpp ^
    src/core/security.cpp ^
    src/mcp/token_optimizer.cpp ^
    src/mcp/mcp_protocol.cpp ^
    -o TreeMCP.exe ^
    -lws2_32 -ladvapi32 -lole32 -lshell32

if errorlevel 1 goto :fail

echo.
echo [SUCCESS] Hardened TreeMCP.exe built successfully!
echo Security Features Active:
echo   [x] DEP / NXCompat (Data Execution Prevention)
echo   [x] ASLR (Address Space Layout Randomization + High Entropy VA)
echo   [x] Stack Protector Strong (Anti-Buffer-Overflow Canary)
echo   [x] Anti-DNS Rebinding (Strict Host validation)
echo   [x] 256-bit High Entropy Session Auth Token (Anti-CSRF)
echo   [x] Windows Recycle Bin Guardrails (Non-destructive clean)
echo   [x] Protected System Path Blacklist (No C:\Windows clobbering)
echo   [x] 64 KB Max Payload Cap (Anti-Memory-Exhaustion DoS)
goto :end

:fail
echo.
echo [ERROR] Build failed! Check compiler output above.
exit /b 1

:end
endlocal
