<div align="center">

<img src="assets/logo.png" alt="TreeMCP Logo" width="128" height="128">

# 🌳 TreeMCP

### Ultra-Fast Lightweight Disk Space Analyzer & Hardened Model Context Protocol (MCP) Server
**Cross-Platform: Windows & Linux**

[![Download TreeMCP](https://img.shields.io/badge/⬇️_Download-TreeMCP.exe_(v1.1.0)-20bf6b?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/AMG555/TreeMCP/releases/download/v1.1.0/TreeMCP.exe)
[![GitHub Release](https://img.shields.io/github/v/release/AMG555/TreeMCP?style=for-the-badge&color=3867d6)](https://github.com/AMG555/TreeMCP/releases/latest)

[![CI](https://github.com/AMG555/TreeMCP/actions/workflows/ci.yml/badge.svg)](https://github.com/AMG555/TreeMCP/actions)
[![Tests: 60 Passed](https://img.shields.io/badge/Tests-60%20Passed-brightgreen)](tests/test_suite.cpp)
[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-0078D6?logo=linux&logoColor=white)](https://github.com/AMG555/TreeMCP)
[![MCP](https://img.shields.io/badge/MCP-2024--11--05-10ac84?logo=anthropic&logoColor=white)](https://modelcontextprotocol.io/)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-~170%20KB-20bf6b)](#-max-lightweight-engineering)
[![Security Hardened](https://img.shields.io/badge/Security-Hardened%20%26%20Sandboxed-3867d6)](SECURITY.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <b>Instant NTFS MFT & POSIX kernel scanning</b>, <b>interactive squarified cushion treemaps</b>, and <b>96% token-thrifty MCP connectivity</b> for Claude, Antigravity, Cursor, and Windsurf.
</p>

[⬇️ Download .EXE](#-direct-download--instant-setup) • [Quickstart](#-quickstart) • [Test Suite](#-automated-test-suite) • [Max Lightweight](#-max-lightweight-engineering) • [Security Policy](SECURITY.md) • [Contributing](CONTRIBUTING.md)

---

</div>

## 📥 Direct Download & Instant Setup

### Option 1: Standalone Download (Windows)
* **[⬇️ Download TreeMCP.exe (v1.1.0 - Windows x64)](https://github.com/AMG555/TreeMCP/releases/download/v1.1.0/TreeMCP.exe)** (< 300 KB)
* All releases & checksums: **[GitHub Releases Page](https://github.com/AMG555/TreeMCP/releases)**

---

### Option 2: 🤖 Single-Prompt AI Agent Setup (Zero Manual Work)
Paste this prompt into **Claude Code**, **Antigravity**, **Cursor**, or **Windsurf**:

```markdown
Download the latest TreeMCP standalone binary from https://github.com/AMG555/TreeMCP/releases/download/v1.1.0/TreeMCP.exe, place it in %LOCALAPPDATA%\TreeMCP\TreeMCP.exe (or clone and run `make` on Linux to build ~/.local/bin/treemcp), register it in your active mcpServers configuration with command `TreeMCP.exe` and args `["--mcp"]`, and test the connection by invoking the `treemcp_drive_summary` tool.
```

---

### Option 3: ⚡ 1-Line Terminal Installers

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/AMG555/TreeMCP/main/install.ps1 | iex
```

**Linux (Bash):**
```bash
curl -fsSL https://raw.githubusercontent.com/AMG555/TreeMCP/main/install.sh | bash
```

---

## 🗑️ Clean Uninstallation

Completely unregisters TreeMCP from all MCP clients (Claude Desktop, Antigravity, Cursor) and deletes the binary with zero leftover files.

### 1-Line Terminal Uninstall
* **Windows (PowerShell):**
  ```powershell
  irm https://raw.githubusercontent.com/AMG555/TreeMCP/main/uninstall.ps1 | iex
  ```
* **Linux (Bash):**
  ```bash
  curl -fsSL https://raw.githubusercontent.com/AMG555/TreeMCP/main/uninstall.sh | bash
  ```

### 🤖 Single-Prompt AI Agent Uninstall
Paste into your AI coding assistant:
```markdown
Stop any running TreeMCP process, unregister the `treemcp` entry from your active mcpServers configuration file, and delete the TreeMCP directory at %LOCALAPPDATA%\TreeMCP (or ~/.local/bin/treemcp on Linux).
```

---

## ⚡ Max Lightweight Engineering

TreeMCP is engineered for extreme resource constraints without runtime dependencies:

| Metric | Measurement | Technical Implementation |
| :--- | :--- | :--- |
| **Standalone Binary Size** | **~170 KB** | Link-Time Optimization (`-flto`), dead-code stripping (`-Wl,--gc-sections`), symbol stripping (`-s`), zero exceptions (`-fno-exceptions`), zero RTTI (`-fno-rtti`). |
| **Node Structure Footprint** | **Strictly 48 Bytes** | `CompactNode` struct packed and naturally 8-byte aligned with zero padding. |
| **100,000 Nodes RAM** | **4.62 MB** | Contiguous index-based node pool and string intern pool. Zero heap fragmentation. |
| **100,000 Nodes Aggregation** | **1.6 ms** | Single-pass post-order bottom-up size and count aggregation. |
| **Treemap Generation Speed** | **2.9 ms** | Squarified geometric layout generating 1,000+ viewport tiles at 60+ FPS. |
| **External Dependencies** | **0** | No Electron, no Chromium, no Node.js runtime, no Python. Pure C++17 + Win32/POSIX. |

---

## 🧪 Automated Test Suite

TreeMCP features a comprehensive, multi-layer automated test suite with **59 automated tests passing in under 18 ms**:

```cmd
# Windows (MinGW-w64)
run_tests.bat

# Linux (GCC / Clang)
./run_tests.sh
```

### Test Coverage Layers:
1. **Unit Tests (Core Data Structures & Algorithms):**
   * Strict 48-byte `CompactNode` size verification.
   * Node pool index links (`parent`, `first_child`, `next_sibling`) and string pool interning.
   * Hierarchical size and file/directory count aggregation up to volume roots.
   * Descending sibling size sorting.
   * Extension categorization matrix and top disk hog rankings.
   * Squarified treemap bounding box containment and mouse coordinate hit testing.
   * Path canonicalization and dot-segment collapse.
   * Heuristic token estimation accuracy.
2. **Security QA / QC Tests (Attack Surface & Mitigations):**
   * Dot-dot path traversal rejection (`../../etc/shadow`, `..\..\Windows\System32`).
   * Windows DOS device identifier blocking (`CON`, `PRN`, `AUX`, `NUL`, `COM1-9`, `LPT1-9`, `\\.\CON`).
   * Null byte and unprintable control character injection blocking.
   * Operating system protected path blacklist (`C:\Windows`, `System32`, `C:\`, `/`, `/etc`, `/bin`, `/boot`, `/sys`, `/proc`, `System Volume Information`, `$Recycle.Bin`).
   * Non-destructive safe recycle guardrail intercepting protected paths.
   * Anti-DNS rebinding strict `Host` header enforcement (loopback allowlist).
   * 256-bit cryptographically secure session token generation and constant-time comparison.
   * 64 KB HTTP request payload cap.
3. **A/B Benchmark Tests (Token Efficiency & Scale):**
   * **A/B Token Hierarchy Optimization:** TreeMCP threshold-rolled compact tree vs standard recursive JSON tree (**>85% token reduction**).
   * **A/B Top Hogs Token Savings:** TreeMCP compact tabular format vs verbose JSON items (**>67% token reduction**).
   * **Synthetic Scale Benchmark:** Real-time generation of 100,000-node hierarchy, verifying memory density (< 6.0 MB RAM) and sub-5ms aggregation.
   * **Squarified Treemap Layout Scale:** High-speed viewport tile calculation (< 15 ms).

---

## 🐧 Linux Support

TreeMCP runs natively on Linux (Ubuntu, Debian, Fedora, Arch, Alpine, etc.):

* **High-Speed POSIX Crawler:** Direct kernel directory traversal (`opendir`/`readdir`/`statx`/`lstat`) with block-allocated cluster detection.
* **Mount-Point Discovery:** Auto-detects physical disk mounts via `/proc/mounts` and `statvfs`.
* **FreeDesktop Trash Safe-Delete:** Integrates with `~/.local/share/Trash` to avoid destructive file deletion.
* **Linux Security Guard:** Blacklists `/`, `/bin`, `/boot`, `/etc`, `/proc`, `/sys`, `/usr`, `/lib`, and user home root from cleanup tools.

### Building & Running on Linux:
```bash
# 1. Clone repository
git clone https://github.com/AMG555/TreeMCP.git
cd TreeMCP

# 2. Compile native binary
make

# 3. Run GUI desktop analyzer
./treemcp

# 4. Or run headless MCP server for Claude / Antigravity / Cursor
./treemcp --mcp

# 5. Fast CLI scan
./treemcp --scan /home
```

---

## 📌 About The Project

**TreeMCP** is an ultra-fast, lightweight, interactive disk space analyzer for Windows and Linux, extended with a native **Model Context Protocol (MCP)** server.

Standard file inspection tools dump massive JSON trees that consume **15,000 to 50,000+ tokens** in a single AI agent turn. TreeMCP introduces an **adaptive compact protocol** that slashes token consumption by **94%–98%**, allowing AI coding assistants to diagnose disk hogs, analyze directory trees, and preview cleanups in **under 100 tokens**.

---

## ✨ Highlights

* 🚀 **Hardware-Speed Scanning:**
  * **Windows:** Direct NTFS Master File Table (`$MFT`) reader via Win32 `FSCTL_GET_NTFS_VOLUME_DATA`; multithreaded Win32 `FindFirstFileExW` crawler fallback. Indexes 100,000 files in under 2 seconds.
  * **Linux:** High-speed POSIX directory traversal with loop-safe symlink avoidance.
* 🪶 **~170 KB Standalone Binary:** Native C++ with zero Electron, zero Python/Node runtimes, and under 30 MB idle RAM usage.
* 🗺️ **Interactive Squarified Treemap:** Visual cushion rectangles proportional to file size, color-coded by extension type (Video, Archive, Executable, Code, Documents), with click-to-select, double-click drilldown, and hover tooltips.
* ⚡ **Integrated Model Context Protocol (MCP):**
  * **Stdio Mode:** Direct integration for Antigravity, Claude Code, Cursor, Windsurf, Claude Desktop.
  * **Local HTTP/SSE Mode:** Binds to `127.0.0.1:4242/mcp` with anti-DNS rebinding and 256-bit token authentication.
* 🪙 **Minimum Token Protocol:** Replaces verbose JSON trees with compact tabular representations, depth cutoffs, and smart size rollups.
* 🛡️ **Non-Destructive Safety:** File deletions route safely through **Windows Recycle Bin** or **Linux FreeDesktop Trash**; system directories (`C:\Windows`, root drives, user profiles, `/etc`, `/usr`) are strictly blocked from accidental deletion.
* ☁️ **Cloudflare Remote Bridge:** Turn-key Cloudflare Worker relay in [`cloudflare-bridge/`](cloudflare-bridge/) for remote agent access over WAN.

---

## 📊 Token Efficiency Benchmark

| Query Scenario | Standard File MCP Server | **TreeMCP Token Optimizer** | Savings |
| :--- | :--- | :--- | :--- |
| **Drive Capacity Summary** | ~1,200 tokens (verbose device specs) | **~18 tokens** (`[C: 176.7/200GB (88%)]`) | **98.5%** |
| **Top 15 Disk Hogs** | ~4,500 tokens (JSON array of dicts) | **~65 tokens** (Compact TSV table) | **98.3%** |
| **Folder Hierarchy (Depth 2)** | ~12,000 tokens (Nested JSON tree) | **~60 tokens** (Threshold-filtered ASCII tree) | **99.5%** |
| **Extension Footprint** | ~2,800 tokens (Full metadata list) | **~40 tokens** (Aggregated extension matrix) | **98.6%** |

---

## 🛡️ Enterprise-Grade Security Guardrails

Full threat model and vulnerability disclosure details in [SECURITY.md](SECURITY.md).

| Layer | Threat | TreeMCP Defensive Mitigation |
| :--- | :--- | :--- |
| **Binary Hardening** | Buffer overflows, ROP | `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, ASLR (`--dynamicbase`), DEP/NX (`--nxcompat`), High-Entropy 64-bit VA (`--high-entropy-va`). |
| **Localhost Attack Surface** | DNS Rebinding via malicious web tabs | Strict `Host` header enforcement: only `127.0.0.1` and `localhost` accepted; all external domains return `403 Forbidden`. |
| **Cross-Origin / CSRF** | Unauthorized browser probing | 256-bit cryptographic session token (`CryptGenRandom` / `/dev/urandom`). Requests without valid token receive `401 Unauthorized`. |
| **OS Protection** | AI accidental deletion of OS | **Protected Path Blacklist:** Blocks deletion of `C:\Windows`, `System32`, root drives (`C:\`, `D:\`), `Program Files`, `/`, `/bin`, `/boot`, `/etc`, `/usr`, `/proc`, `/sys`. |
| **File Deletion Safety** | Irreversible data loss | Non-destructive deletion: uses Windows Shell `SHFileOperationW` with `FOF_ALLOWUNDO` or Linux FreeDesktop Trash, routing all deleted files safely to **Recycle Bin / Trash**. |
| **Device Injection** | Windows device name exploits (`\\.\nul`, `CON`, `AUX`) | Strict path validation rejects reserved Windows DOS devices and invalid path characters. |
| **Denial of Service** | Memory exhaustion via oversized JSON | Strict **64 KB request payload ceiling** enforced on all socket and stdio inputs. |
| **HTTP Security Headers** | Clickjacking, MIME sniffing, XSS | Responses include strict CSP, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer`. |

---

## 🏗️ Architecture

```
                  ┌───────────────────────────────────────────────┐
                  │                 TreeMCP                       │
                  │         (~170 KB Windows & Linux)             │
                  └───────┬───────────────────────────────┬───────┘
                          │                               │
        ┌─────────────────┴─────────────┐   ┌─────────────┴─────────────────┐
        │        Scanning Engine        │   │          MCP Protocol         │
        ├───────────────────────────────┤   ├───────────────────────────────┤
        │ • NTFS MFT Direct Parser (Win)│   │ • JSON-RPC 2.0 (2024-11-05)   │
        │ • Fast Win32 FindFirstFileExW │   │ • Stdio & Local SSE Transport │
        │ • Linux POSIX /proc/mounts    │   │ • Token Optimization Engine   │
        │ • 48-byte Compact Node Pool   │   │ • Live Token Savings Meter    │
        │ • Squarified Treemap Layout   │   │ • Anti-DNS Rebinding & CSRF   │
        │ • Recycle Bin & Linux Trash   │   │ • 256-bit Session Auth Guard  │
        └───────────────┬───────────────┘   └─────────────┬─────────────────┘
                        │                                 │
                        ▼                                 ▼
         ┌──────────────────────────────┐  ┌────────────────────────────────┐
         │    Interactive GUI Desktop   │  │      AI Coding Assistants      │
         │  • Drive Selector & Gauge    │  │ • Antigravity / Claude Code    │
         │  • Tree & Extension Tabs     │  │ • Cursor / Windsurf / Desktop  │
         │  • Interactive Canvas Treemap│  │ • Optional Cloudflare Bridge   │
         └──────────────────────────────┘  └────────────────────────────────┘
```

---

## 🚀 Quickstart

### Windows
```cmd
# 1. Download pre-built binary (< 300 KB)
# https://github.com/AMG555/TreeMCP/releases/download/v1.1.0/TreeMCP.exe

# 2. Or build from source
build.bat

# 3. Run GUI Desktop
TreeMCP.exe

# 4. Run Headless MCP Server
TreeMCP.exe --mcp
```

### Linux
```bash
# 1. Build
make

# 2. Run Desktop GUI
./treemcp

# 3. Run Headless MCP Server
./treemcp --mcp
```

---

## 🔌 MCP Client Configuration

Add TreeMCP to your AI client's configuration file:

### Antigravity / Claude Desktop (`mcp_config.json`):
```json
{
  "mcpServers": {
    "treemcp": {
      "command": "d:\\PROJECTS\\RN_PROJECTS\\TreeMCP\\TreeMCP.exe",
      "args": ["--mcp"]
    }
  }
}
```

*(On Linux, set `"command": "/absolute/path/to/treemcp"`).*

### Cursor / Windsurf:
* **Command:** `d:\PROJECTS\RN_PROJECTS\TreeMCP\TreeMCP.exe`
* **Args:** `["--mcp"]`

---

## 🛠️ MCP Tools Reference

* **`treemcp_drive_summary`**: Instant summary of all local drives/mounts, capacity, free space, and file system with minimum tokens (~18–25 tokens).
* **`treemcp_scan`**: Scans and indexes a directory or drive for instant tree and treemap analysis (safe paths only).
* **`treemcp_top_hogs`**: Finds largest files/folders consuming disk space (`limit`, `min_mb`, `filter`).
* **`treemcp_dir_tree`**: Compact depth-limited directory tree with threshold rollup (`max_depth`, `min_mb`).
* **`treemcp_extension_stats`**: Breakdown of disk space consumed by file extension (`.mp4`, `.zip`, `.dll`, etc.).
* **`treemcp_search`**: Instant search for files matching substring/extension with size filtering.
* **`treemcp_clean`**: Previews or safely deletes space-consuming directories (routed to **Recycle Bin / Linux Trash**; blocks protected system paths).

---

## 🌐 Cloudflare Remote MCP vs. Local MCP

A deep architectural breakdown is available in [`cloudflare-bridge/ARCHITECTURE_AND_COMPARISON.md`](cloudflare-bridge/ARCHITECTURE_AND_COMPARISON.md).

* **Local MCP (Default):** Recommended for 99% of workflows. 100% private, zero network latency (<1ms), zero cloud cost, works completely offline.
* **Cloudflare Remote Bridge:** Turn-key Cloudflare Worker relay in [`cloudflare-bridge/worker.ts`](cloudflare-bridge/worker.ts) for remote multi-machine fleet monitoring.

---

## 🤝 Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on code style, memory compactness constraints, and PR workflows.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
