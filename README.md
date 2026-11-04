<div align="center">

# 🌳 TreeMCP

### Ultra-Fast Lightweight WizTree Replica & Hardened Model Context Protocol (MCP) Server

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?logo=windows&logoColor=white)](https://microsoft.com/windows)
[![MCP](https://img.shields.io/badge/MCP-2024--11--05-10ac84?logo=anthropic&logoColor=white)](https://modelcontextprotocol.io/)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-~347%20KB-20bf6b)](#-highlights)
[![Security Hardened](https://img.shields.io/badge/Security-Hardened%20%26%20Sandboxed-3867d6)](#-enterprise-grade-security-guardrails)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <b>Instant NTFS MFT scanning</b>, <b>interactive squarified cushion treemaps</b>, and <b>96% token-thrifty MCP connectivity</b> for Claude, Antigravity, Cursor, and Windsurf.
</p>

[Quickstart](#-quickstart) • [Token Benchmark](#-token-efficiency-benchmark) • [MCP Client Setup](#-mcp-client-configuration) • [Security Features](#-enterprise-grade-security-guardrails) • [Architecture](#-architecture)

---

</div>

## 📌 About The Project

**TreeMCP** is an exact, high-speed, lightweight replica of the iconic **WizTree** disk space utility for Windows, extended with a native **Model Context Protocol (MCP)** server.

Standard file inspection tools dump massive JSON trees that consume **15,000 to 50,000+ tokens** in a single AI agent turn. TreeMCP introduces an **adaptive compact protocol** that slashes token consumption by **94%–98%**, allowing AI coding assistants to diagnose disk hogs, analyze directory trees, and preview cleanups in **under 100 tokens**.

---

## ✨ Highlights

* 🚀 **Hardware-Speed Scanning:** Direct NTFS Master File Table (`$MFT`) reader via Win32 `FSCTL_GET_NTFS_VOLUME_DATA`; multithreaded Win32 `FindFirstFileExW` crawler fallback. Indexes 100,000 files in under 2 seconds.
* 🪶 **347 KB Standalone Binary:** Native C++ with zero Electron, zero Python/Node runtimes, and under 35 MB idle RAM usage.
* 🗺️ **Interactive Squarified Treemap:** Visual cushion rectangles proportional to file size, color-coded by extension type (Video, Archive, Executable, Code, Documents), with click-to-select, double-click drilldown, and hover tooltips.
* ⚡ **Integrated Model Context Protocol (MCP):**
  * **Stdio Mode:** Direct integration for Antigravity, Claude Code, Cursor, Windsurf, Claude Desktop.
  * **Local HTTP/SSE Mode:** Binds to `127.0.0.1:4242/mcp` with anti-DNS rebinding and 256-bit token authentication.
* 🪙 **Minimum Token Protocol:** Replaces verbose JSON trees with compact tabular representations, depth cutoffs, and smart size rollups.
* 🛡️ **Non-Destructive Safety:** File deletions route safely through the **Windows Recycle Bin** (`SHFileOperationW` with `FOF_ALLOWUNDO`); system directories (`C:\Windows`, root drives, user profiles) are strictly blocked from accidental deletion.
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
| **Cross-Origin / CSRF** | Unauthorized browser probing | 256-bit cryptographic session token (`CryptGenRandom`). Requests without valid token receive `401 Unauthorized`. |
| **OS Protection** | AI accidental deletion of Windows | **Protected Path Blacklist:** Blocks deletion of `C:\Windows`, `System32`, root drives (`C:\`, `D:\`), `Program Files`, and user profile roots. |
| **File Deletion Safety** | Irreversible data loss | Non-destructive deletion: uses Windows Shell `SHFileOperationW` with `FOF_ALLOWUNDO`, routing all deleted files safely to the **Windows Recycle Bin**. |
| **Device Injection** | Windows device name exploits (`\\.\nul`, `CON`, `AUX`) | Strict path validation rejects reserved Windows DOS devices and invalid path characters. |
| **Denial of Service** | Memory exhaustion via oversized JSON | Strict **64 KB request payload ceiling** enforced on all socket and stdio inputs. |
| **HTTP Security Headers** | Clickjacking, MIME sniffing, XSS | Responses include strict CSP, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer`. |

---

## 🏗️ Architecture

```
                  ┌───────────────────────────────────────────────┐
                  │                 TreeMCP.exe                   │
                  │             (347 KB Native C++)               │
                  └───────┬───────────────────────────────┬───────┘
                          │                               │
        ┌─────────────────┴─────────────┐   ┌─────────────┴─────────────────┐
        │        Scanning Engine        │   │          MCP Protocol         │
        ├───────────────────────────────┤   ├───────────────────────────────┤
        │ • NTFS MFT Direct Parser      │   │ • JSON-RPC 2.0 (2024-11-05)   │
        │ • Fast Win32 FindFirstFileExW │   │ • Stdio & Local SSE Transport │
        │ • 48-byte Compact Node Pool   │   │ • Token Optimization Engine   │
        │ • Squarified Treemap Layout   │   │ • Live Token Savings Meter    │
        │ • Windows Recycle Bin Guard   │   │ • Anti-DNS Rebinding & CSRF   │
        └───────────────┬───────────────┘   └─────────────┬─────────────────┘
                        │                                 │
                        ▼                                 ▼
         ┌──────────────────────────────┐  ┌────────────────────────────────┐
         │     WizTree GUI Desktop      │  │      AI Coding Assistants      │
         │  • Drive Selector & Gauge    │  │ • Antigravity / Claude Code    │
         │  • Tree & Extension Tabs     │  │ • Cursor / Windsurf / Desktop  │
         │  • Interactive Canvas Treemap│  │ • Optional Cloudflare Bridge   │
         └──────────────────────────────┘  └────────────────────────────────┘
```

---

## 🚀 Quickstart

### 1. Build from Source
Requirements: MinGW-w64 (`g++` supporting C++17).
```cmd
build.bat
```
Produces hardened `TreeMCP.exe` (~347 KB).

### 2. Run the Desktop GUI
```cmd
TreeMCP.exe
```
Launches the native desktop application with live drive metrics, interactive cushion treemap, and active local server.

### 3. Run as Headless Stdio MCP Server
```cmd
TreeMCP.exe --mcp
```

### 4. Fast CLI Scan
```cmd
TreeMCP.exe --scan C:\
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

### Cursor / Windsurf:
* **Command:** `d:\PROJECTS\RN_PROJECTS\TreeMCP\TreeMCP.exe`
* **Args:** `["--mcp"]`

---

## 🛠️ MCP Tools Reference

* **`treemcp_drive_summary`**: Instant summary of all local drives, capacity, free space, and file system with minimum tokens (~18–25 tokens).
* **`treemcp_scan`**: Scans and indexes a directory or drive for instant tree and treemap analysis (safe paths only).
* **`treemcp_top_hogs`**: Finds largest files/folders consuming disk space (`limit`, `min_mb`, `filter`).
* **`treemcp_dir_tree`**: Compact depth-limited directory tree with threshold rollup (`max_depth`, `min_mb`).
* **`treemcp_extension_stats`**: Breakdown of disk space consumed by file extension (`.mp4`, `.zip`, `.dll`, etc.).
* **`treemcp_search`**: Instant search for files matching substring/extension with size filtering.
* **`treemcp_clean`**: Previews or safely deletes space-consuming directories (routed to **Windows Recycle Bin**; blocks protected system paths).

---

## 🌐 Cloudflare Remote MCP vs. Local MCP

A deep architectural breakdown is available in [`cloudflare-bridge/ARCHITECTURE_AND_COMPARISON.md`](cloudflare-bridge/ARCHITECTURE_AND_COMPARISON.md).

* **Local MCP (Default):** Recommended for 99% of workflows. 100% private, zero network latency (<1ms), zero cloud cost, works completely offline.
* **Cloudflare Remote Bridge:** Turn-key Cloudflare Worker relay in [`cloudflare-bridge/worker.ts`](cloudflare-bridge/worker.ts) for remote multi-machine fleet monitoring.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
