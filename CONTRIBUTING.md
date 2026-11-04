# Contributing to TreeMCP

Thank you for your interest in contributing to TreeMCP!

TreeMCP is an ultra-fast, memory-efficient disk space analyzer with a built-in Model Context Protocol (MCP) server. To maintain our strict standards for speed, security, and minimal token overhead, please review these guidelines before submitting contributions.

---

## Guiding Principles

1. **Lightweight First**: TreeMCP has zero heavy runtime dependencies (no Electron, no Chromium, no Node runtime). The standalone binary is < 300 KB. Any added feature must not bloat memory or disk footprint.
2. **Minimal MCP Tokens**: MCP prompts and tool outputs must remain strictly token-optimized. Never return verbose, nested JSON trees over LLM context. Always use compact, line-based summaries and bounded depth.
3. **Security by Default**:
   - Every file deletion must route through the OS safe trash/recycle bin (`FOF_ALLOWUNDO` on Windows, FreeDesktop Trash on Linux).
   - All root drives, system directories (`C:\Windows`, `/bin`, `/etc`, etc.), and device files (`CON`, `AUX`, `NUL`) are blacklisted.
   - All HTTP/SSE connections require 256-bit token authentication and DNS-rebinding protection via strict `Host` header checks.
4. **No Third-Party Bloat**: Core features rely strictly on modern standard C++17, native Win32 APIs, and POSIX system calls.

---

## Development Setup

### Windows (MinGW-w64)
- **Compiler**: GCC 11+ with C++17 support.
- **Build**:
  ```cmd
  build.bat
  ```
- **Run Tests**:
  ```cmd
  run_tests.bat
  ```

### Linux (GCC or Clang)
- **Compiler**: `g++` (10+) or `clang++` (11+).
- **Build**:
  ```bash
  make
  ```
- **Run Tests**:
  ```bash
  chmod +x run_tests.sh
  ./run_tests.sh
  ```

---

## Coding Standards

- **Language**: Modern C++17.
- **No Exceptions / No RTTI**: Built with `-fno-exceptions -fno-rtti` to minimize binary footprint and avoid runtime overhead. Use return codes and error structs.
- **Node Memory Constraints**: The node pool uses `CompactNode` (fixed 48 bytes per entry). Do not add fields to `CompactNode` without calculating cache-line alignment and memory overhead (1,000,000 nodes must fit in under 48 MB RAM).
- **Strings**: Use string intern pools or offset indices rather than allocating dynamic `std::string` inside high-frequency node records.
- **Clean Git History**:
  - Write concise, descriptive commit messages.
  - Never reference proprietary or competitor software names in source comments, commit messages, or documentation.

---

## Pull Request Workflow

1. Fork the repository and create a feature branch (`git checkout -b feature/my-feature`).
2. Implement your changes adhering to memory, token, and security constraints.
3. Run the automated test suite locally:
   - All Unit tests, QA/QC security tests, and A/B token ratio benchmarks must pass.
4. Verify binary size does not regress significantly.
5. Submit a pull request with a concise description of changes and test evidence.

---

## Reporting Issues

- Bug reports and feature proposals should be submitted via [GitHub Issues](https://github.com/AMG555/TreeMCP/issues).
- For security vulnerabilities, please refer to [SECURITY.md](SECURITY.md) and report via private channels.

