<div align="center">

# 🌳 TreeMCP

### Ultra-Fast Lightweight Disk Space Analyzer & Hardened Model Context Protocol (MCP) Server
**Cross-Platform: Windows & Linux**

[![Download TreeMCP](https://img.shields.io/badge/⬇️_Download-TreeMCP.exe_(v1.0.0)-20bf6b?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/AMG555/TreeMCP/releases/download/v1.0.0/TreeMCP.exe)
[![GitHub Release](https://img.shields.io/github/v/release/AMG555/TreeMCP?style=for-the-badge&color=3867d6)](https://github.com/AMG555/TreeMCP/releases/latest)

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-0078D6?logo=linux&logoColor=white)](https://github.com/AMG555/TreeMCP)
[![MCP](https://img.shields.io/badge/MCP-2024--11--05-10ac84?logo=anthropic&logoColor=white)](https://modelcontextprotocol.io/)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-~347%20KB-20bf6b)](#-highlights)
[![Security Hardened](https://img.shields.io/badge/Security-Hardened%20%26%20Sandboxed-3867d6)](#-enterprise-grade-security-guardrails)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <b>Instant NTFS MFT & POSIX kernel scanning</b>, <b>interactive squarified cushion treemaps</b>, and <b>96% token-thrifty MCP connectivity</b> for Claude, Antigravity, Cursor, and Windsurf.
</p>

[⬇️ Download .EXE](#-direct-download) • [Quickstart](#-quickstart) • [Linux Setup](#-linux-support) • [Token Benchmark](#-token-efficiency-benchmark) • [MCP Setup](#-mcp-client-configuration) • [Security](#-enterprise-grade-security-guardrails)

---

</div>

## 📥 Direct Download (Windows)

No installation required. Single standalone executable:

* **[⬇️ Download TreeMCP.exe (v1.0.0 - Windows x64)](https://github.com/AMG555/TreeMCP/releases/download/v1.0.0/TreeMCP.exe)** (~347 KB)
* All releases & checksums: **[GitHub Releases Page](https://github.com/AMG555/TreeMCP/releases)**

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
* 🪶 **347 KB Standalone Binary:** Native C++ with zero Electron, zero Python/Node runtimes, and under 35 MB idle RAM usage.
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
                  │         (347 KB Windows & Linux)              │
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
# 1. Download pre-built binary
# https://github.com/AMG555/TreeMCP/releases/download/v1.0.0/TreeMCP.exe

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

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
