#include "security.h"
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <shlobj.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#endif

namespace TreeMCP {

// =========================================================================
// 1. CRYPTOGRAPHIC RANDOM TOKEN GENERATION
// =========================================================================

std::string Security::generate_secure_token() {
    unsigned char random_bytes[32] = {0};

#ifdef _WIN32
    HCRYPTPROV h_prov = 0;
    if (CryptAcquireContextW(&h_prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        CryptGenRandom(h_prov, sizeof(random_bytes), random_bytes);
        CryptReleaseContext(h_prov, 0);
    } else {
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        uint64_t seed = qpc.QuadPart ^ reinterpret_cast<uintptr_t>(&random_bytes);
        for (size_t i = 0; i < sizeof(random_bytes); ++i) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            random_bytes[i] = static_cast<unsigned char>(seed >> 32);
        }
    }
#else
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom.is_open()) {
        urandom.read(reinterpret_cast<char*>(random_bytes), sizeof(random_bytes));
    }
#endif

    std::stringstream ss;
    for (size_t i = 0; i < sizeof(random_bytes); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(random_bytes[i]);
    }
    return ss.str();
}

// =========================================================================
// 2. PATH CANONICALIZATION (UNIVERSAL SEGMENT COLLAPSE)
// =========================================================================

std::string Security::canonicalize_path(const std::string& raw_path) {
    if (raw_path.empty()) return "";

    char sep = (raw_path.find('\\') != std::string::npos) ? '\\' : '/';
    std::string prefix;
    std::string rest = raw_path;

    if (rest.size() >= 2 && rest[1] == ':') {
        prefix = rest.substr(0, 2);
        rest = rest.substr(2);
        if (!rest.empty() && (rest[0] == '/' || rest[0] == '\\')) {
            prefix += sep;
            rest = rest.substr(1);
        }
    } else if (!rest.empty() && (rest[0] == '/' || rest[0] == '\\')) {
        prefix = std::string(1, sep);
        rest = rest.substr(1);
    }

    std::vector<std::string> segments;
    std::string token;
    for (size_t i = 0; i <= rest.size(); ++i) {
        char c = (i < rest.size()) ? rest[i] : sep;
        if (c == '/' || c == '\\') {
            if (!token.empty()) {
                if (token == ".") {
                    // ignore current directory marker
                } else if (token == "..") {
                    if (!segments.empty() && segments.back() != "..") {
                        segments.pop_back();
                    } else if (prefix.empty()) {
                        segments.push_back("..");
                    }
                } else {
                    segments.push_back(token);
                }
                token.clear();
            }
        } else {
            token += c;
        }
    }

    std::string result = prefix;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (!result.empty() && result.back() != sep) {
            result += sep;
        }
        result += segments[i];
    }
    return result.empty() ? std::string(1, sep) : result;
}

// =========================================================================
// 3. SAFE PATH VALIDATION (TRAVERSAL, DOS DEVICES, AND CONTROL CHARS)
// =========================================================================

bool Security::is_path_safe_to_read(const std::string& path) {
    if (path.empty()) return false;

    // 0. Reject dangerous control characters, null bytes, and shell metacharacters
    if (contains_dangerous_chars(path)) return false;
    for (char c : path) {
        if (c == '"' || c == ';' || c == '&' || c == '|' || c == '`' || c == '$' || c == '\n' || c == '\r') {
            return false;
        }
    }

    std::string p = path;
    for (char& c : p) c = static_cast<char>(tolower(c));

    // 1. Reject path traversal sequences
    if (p.find("..\\") != std::string::npos || p.find("../") != std::string::npos ||
        p.find("\\..") != std::string::npos || p.find("/..") != std::string::npos ||
        p == ".." || p.find("...") != std::string::npos) {
        return false;
    }

    // 2. Reject dangerous Unix hardware device memory targets
    if (p.find("/dev/mem") != std::string::npos || p.find("/dev/kmem") != std::string::npos ||
        p.find("/dev/port") != std::string::npos) {
        return false;
    }

    // 3. Reject Windows raw volume namespaces unless safe drive accessor
    if (p.find("\\\\.\\") == 0 || p.find("\\\\?\\") == 0) {
        if (p.size() >= 6 && p.substr(0, 4) == "\\\\.\\" && isalpha(p[4]) && p[5] == ':') {
            return true;
        }
        return false;
    }

    // 4. Reject reserved Windows DOS device names
    std::vector<std::string> bad_devices = {
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
    };

    for (const auto& dev : bad_devices) {
        size_t pos = p.find(dev);
        if (pos != std::string::npos) {
            bool left_bound = (pos == 0 || p[pos - 1] == '\\' || p[pos - 1] == '/');
            bool right_bound = (pos + dev.size() == p.size() || p[pos + dev.size()] == '\\' || p[pos + dev.size()] == '/' || p[pos + dev.size()] == '.');
            if (left_bound && right_bound) {
                return false;
            }
        }
    }

    return true;
}

// =========================================================================
// 4. PROTECTED OS PATH BLACKLIST
// =========================================================================

bool Security::is_protected_system_path(const std::string& raw_path) {
    if (raw_path.empty()) return true;

    // 1. Unix-style system paths
    std::string norm = raw_path;
    for (char& c : norm) { if (c == '\\') c = '/'; c = static_cast<char>(tolower(c)); }
    while (norm.size() > 1 && norm.back() == '/') norm.pop_back();

    if (norm == "/" ||
        norm == "/bin" || norm.find("/bin/") == 0 ||
        norm == "/boot" || norm.find("/boot/") == 0 ||
        norm == "/dev" || norm.find("/dev/") == 0 ||
        norm == "/etc" || norm.find("/etc/") == 0 ||
        norm == "/lib" || norm.find("/lib/") == 0 ||
        norm == "/lib64" || norm.find("/lib64/") == 0 ||
        norm == "/proc" || norm.find("/proc/") == 0 ||
        norm == "/root" || norm == "/sys" || norm.find("/sys/") == 0 ||
        norm == "/usr" || norm.find("/usr/") == 0 ||
        norm == "/sbin" || norm.find("/sbin/") == 0 ||
        norm == "/home") {
        return true;
    }

    // 2. Windows drive roots and system directories
    std::string win_p = raw_path;
    for (char& c : win_p) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }
    while (win_p.size() > 3 && win_p.back() == '\\') win_p.pop_back();

    if (win_p.size() <= 3 && win_p.size() >= 2 && win_p[1] == ':') {
        return true;
    }

    std::vector<std::string> protected_win_patterns = {
        "\\windows", "\\system32", "\\syswow64", "\\program files",
        "\\program files (x86)", "\\programdata\\microsoft",
        "\\system volume information", "\\$recycle.bin", "\\recovery",
        "\\boot", "\\pagefile.sys", "\\swapfile.sys", "\\hiberfil.sys"
    };

    for (const auto& pat : protected_win_patterns) {
        if (win_p.find(pat) != std::string::npos) {
            if (win_p.size() >= 2 && win_p[1] == ':') {
                std::string rel = win_p.substr(2);
                if (rel == pat || rel.find(pat + "\\") == 0) return true;
            } else if (win_p == pat || win_p.find(pat + "\\") == 0) {
                return true;
            }
        }
    }

    // Also evaluate canonicalized variant
    std::string canon = canonicalize_path(raw_path);
    if (!canon.empty() && canon != raw_path) {
        std::string c_norm = canon;
        for (char& c : c_norm) { if (c == '\\') c = '/'; c = static_cast<char>(tolower(c)); }
        while (c_norm.size() > 1 && c_norm.back() == '/') c_norm.pop_back();

        if (c_norm == "/" || c_norm == "/etc" || c_norm == "/bin" || c_norm == "/boot" ||
            c_norm == "/proc" || c_norm == "/sys" || c_norm == "/usr" || c_norm == "/sbin") {
            return true;
        }

        std::string c_win = canon;
        for (char& c : c_win) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }
        while (c_win.size() > 3 && c_win.back() == '\\') c_win.pop_back();

        if (c_win.size() <= 3 && c_win.size() >= 2 && c_win[1] == ':') return true;
        for (const auto& pat : protected_win_patterns) {
            if (c_win.find(pat) != std::string::npos) {
                if (c_win.size() >= 2 && c_win[1] == ':') {
                    std::string rel = c_win.substr(2);
                    if (rel == pat || rel.find(pat + "\\") == 0) return true;
                } else if (c_win == pat || c_win.find(pat + "\\") == 0) {
                    return true;
                }
            }
        }
    }

#ifdef _WIN32
    wchar_t win_dir[MAX_PATH] = {0};
    if (GetWindowsDirectoryW(win_dir, MAX_PATH)) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, win_dir, -1, NULL, 0, NULL, NULL);
        if (sz > 0) {
            std::string s_win;
            s_win.resize(sz);
            WideCharToMultiByte(CP_UTF8, 0, win_dir, -1, &s_win[0], sz, NULL, NULL);
            if (!s_win.empty() && s_win.back() == '\0') s_win.pop_back();
            for (char& c : s_win) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }
            if (win_p == s_win || win_p.find(s_win + "\\") == 0) return true;
        }
    }

    // 3. User Personal Data Safeguards (Prevent AI agents from deleting desktop, documents, pictures, codebases)
    std::vector<std::string> protected_user_subdirs = {
        "\\desktop", "\\documents", "\\pictures", "\\videos", "\\music",
        "\\downloads", "\\onedrive", "\\contacts", "\\favorites", "\\links", "\\saved games", "\\searches"
    };

    wchar_t user_prof[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"USERPROFILE", user_prof, MAX_PATH)) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, user_prof, -1, NULL, 0, NULL, NULL);
        if (sz > 0) {
            std::string s_prof;
            s_prof.resize(sz);
            WideCharToMultiByte(CP_UTF8, 0, user_prof, -1, &s_prof[0], sz, NULL, NULL);
            if (!s_prof.empty() && s_prof.back() == '\0') s_prof.pop_back();
            for (char& c : s_prof) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }

            // Block entire user profile root
            if (win_p == s_prof) return true;

            // Block user primary personal folders
            for (const auto& udir : protected_user_subdirs) {
                std::string full_u = s_prof + udir;
                if (win_p == full_u) return true;
            }
        }
    }

    // 4. Source Code and Git Repository Guardrail (Prevent agents from deleting git databases or project roots)
    if (win_p.find("\\.git") != std::string::npos || win_p.find("/.git") != std::string::npos ||
        win_p.find("\\.ssh") != std::string::npos || win_p.find("/.ssh") != std::string::npos ||
        win_p.find("\\.aws") != std::string::npos || win_p.find("/.aws") != std::string::npos ||
        win_p.find("\\.gnupg") != std::string::npos || win_p.find("/.gnupg") != std::string::npos) {
        return true;
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        std::string s_home(home);
        if (norm == s_home) return true;
        std::vector<std::string> user_dirs = {
            "/Desktop", "/Documents", "/Pictures", "/Videos", "/Music", "/Downloads"
        };
        for (const auto& ud : user_dirs) {
            if (norm == s_home + ud) return true;
        }
    }
    if (norm.find("/.git") != std::string::npos || norm.find("/.ssh") != std::string::npos ||
        norm.find("/.aws") != std::string::npos || norm.find("/.gnupg") != std::string::npos) {
        return true;
    }
#endif

    return false;
}

// =========================================================================
// 5. NON-DESTRUCTIVE SAFE RECYCLE (WINDOWS RECYCLE BIN / LINUX TRASH)
// =========================================================================

Security::DeleteResult Security::safe_recycle_item(const std::string& raw_path) {
    DeleteResult res;
    res.success = false;
    res.was_protected = false;

    if (is_protected_system_path(raw_path)) {
        res.was_protected = true;
        res.error_message = "Path is protected by system security policy and cannot be deleted.";
        return res;
    }

    std::string path = canonicalize_path(raw_path);
    if (is_protected_system_path(path)) {
        res.was_protected = true;
        res.error_message = "Path is protected by system security policy and cannot be deleted.";
        return res;
    }

#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::vector<wchar_t> double_null_path(wlen + 2, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, double_null_path.data(), wlen);
    double_null_path[wlen] = L'\0';
    double_null_path[wlen + 1] = L'\0';

    SHFILEOPSTRUCTW file_op{};
    file_op.wFunc = FO_DELETE;
    file_op.pFrom = double_null_path.data();
    file_op.pTo = NULL;
    file_op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

    int ret = SHFileOperationW(&file_op);
    if (ret == 0 && !file_op.fAnyOperationsAborted) {
        res.success = true;
    } else {
        // If standard shell operation failed (e.g. admin-owned file or ACL permission),
        // try direct removal if running with administrative rights
        DWORD attr = GetFileAttributesW(double_null_path.data());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            if (attr & FILE_ATTRIBUTE_READONLY) {
                SetFileAttributesW(double_null_path.data(), attr & ~FILE_ATTRIBUTE_READONLY);
            }
            if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                if (RemoveDirectoryW(double_null_path.data())) {
                    res.success = true;
                } else {
                    res.error_message = "Windows Shell error: " + std::to_string(ret) + " (Win32 Err: " + std::to_string(GetLastError()) + ")";
                }
            } else {
                if (DeleteFileW(double_null_path.data())) {
                    res.success = true;
                } else {
                    res.error_message = "Windows Shell error: " + std::to_string(ret) + " (Win32 Err: " + std::to_string(GetLastError()) + ")";
                }
            }
        } else {
            res.error_message = "Windows Shell error code: " + std::to_string(ret);
        }
    }
#else
    // FreeDesktop.org Trash Specification
    const char* home = getenv("HOME");
    std::string trash_dir = home ? (std::string(home) + "/.local/share/Trash") : "/tmp/treemcp_trash";
    std::string trash_files = trash_dir + "/files";
    mkdir(trash_dir.c_str(), 0700);
    mkdir(trash_files.c_str(), 0700);

    size_t slash = path.find_last_of('/');
    std::string fname = (slash != std::string::npos) ? path.substr(slash + 1) : path;
    std::string dest = trash_files + "/" + fname;

    if (rename(path.c_str(), dest.c_str()) == 0) {
        res.success = true;
    } else {
        res.error_message = "Failed to move item to trash directory.";
    }
#endif

    return res;
}

// =========================================================================
// 6. SHARED UTILITIES (CONSTANT TIME COMPARE, HOST VALIDATION, CHAR CHECKS)
// =========================================================================

bool Security::constant_time_compare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= (static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]));
    }
    return diff == 0;
}

bool Security::is_valid_host_header(const std::string& host_header, int expected_port) {
    if (host_header.empty()) return false;

    std::string port_str = ":" + std::to_string(expected_port);
    std::string host_lower = host_header;
    for (char& c : host_lower) c = static_cast<char>(tolower(c));

    while (!host_lower.empty() && (host_lower.back() == '\r' || host_lower.back() == '\n' || host_lower.back() == ' ')) {
        host_lower.pop_back();
    }

    if (host_lower == "127.0.0.1" || host_lower == ("127.0.0.1" + port_str)) return true;
    if (host_lower == "localhost" || host_lower == ("localhost" + port_str)) return true;
    if (host_lower == "[::1]" || host_lower == ("[::1]" + port_str)) return true;

    return false;
}

bool Security::contains_dangerous_chars(const std::string& str) {
    for (unsigned char c : str) {
        if (c < 32 && c != '\t') return true;
    }
    return false;
}

} // namespace TreeMCP
