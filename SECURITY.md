# Security Policy

TreeMCP is engineered from the ground up with defensive security, non-destructive defaults, and binary hardening.

---

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

---

## Reporting a Vulnerability

We take the security of TreeMCP seriously. If you discover a security vulnerability, please **do not open a public issue**. Instead, follow these steps:

1. **Email**: Send detailed advisory information to `security@treemcp.org` or open a private advisory at [GitHub Security Advisories](https://github.com/AMG555/TreeMCP/security/advisories/new).
2. **Details to Include**:
   - Attack scenario description and impact.
   - Proof of concept (PoC) or reproduction steps.
   - Affected platform (Windows / Linux) and environment.
3. **Response Timeline**:
   - Initial triage and acknowledgment: within 24 hours.
   - Patch and advisory coordination: within 5 business days.

---

## Threat Model & Defensive Mitigations

TreeMCP runs locally with access to disk metadata and accepts local Model Context Protocol (MCP) tool requests. To protect user systems from malicious AI prompts, unauthorized local callers, and destructive commands, the following defenses are strictly enforced:

### 1. Non-Destructive Safe Deletions
- Direct file unlinking (`DeleteFileW`, `unlink`) is prohibited in normal operation.
- **Windows**: All cleanup operations invoke the Win32 Shell API with `FOF_ALLOWUNDO`, redirecting files to the Windows Recycle Bin.
- **Linux**: Cleanup operations relocate targets into the FreeDesktop Trash spec directory (`~/.local/share/Trash`), preserving metadata.

### 2. Path Traversal & Device Name Defense
- Absolute paths are canonicalized and resolved against real drive anchors before execution.
- Path traversal sequences (`..`, `/.`, `\.`) are rejected.
- Reserved Windows DOS device identifiers (`CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`) are blocked to prevent kernel hangs or privilege abuse.

### 3. Protected OS Path Blacklist
- The engine rejects any delete or write operations targeted at:
  - Root volumes (`C:\`, `/`, etc.)
  - Critical operating system paths (`C:\Windows`, `C:\Program Files`, `/bin`, `/boot`, `/etc`, `/sbin`, `/usr`, `/lib`, `/sys`, `/proc`).
  - System recovery and volume metadata (`System Volume Information`, `$Recycle.Bin`).

### 4. Anti-DNS Rebinding
- The local MCP server validates the HTTP `Host` header against an allowlist of loopback endpoints (`127.0.0.1`, `localhost`, `[::1]`).
- Any external hostname or non-loopback header triggers an immediate `403 Forbidden` response.

### 5. Session Authentication (Anti-CSRF)
- A 256-bit cryptographically secure session token is generated at process startup.
- All non-GET MCP requests require an `Authorization: Bearer <token>` header or `?token=<token>` parameter to prevent Cross-Site Request Forgery from local web browsers.

### 6. DoS Prevention via Request Limits
- The HTTP/MCP server enforces a strict `64 KB` payload limit. Malformed or excessively large JSON payloads are rejected before memory allocation.

### 7. Binary Hardening
- **DEP / NX**: Data Execution Prevention enabled (`-Wl,--nxcompat`).
- **ASLR**: High Entropy 64-bit Address Space Layout Randomization (`-Wl,--dynamicbase -Wl,--high-entropy-va`).
- **Stack Canaries**: Buffer overflow protection via `-fstack-protector-strong`.
- **Fortify Source**: Runtime memory boundary checks enabled via `-D_FORTIFY_SOURCE=2`.

