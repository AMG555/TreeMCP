#include "security.h"
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <shlobj.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace TreeMCP {

std::string Security::generate_secure_token() {
    HCRYPTPROV h_prov = 0;
    unsigned char random_bytes[32] = {0};

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

    std::stringstream ss;
    for (size_t i = 0; i < sizeof(random_bytes); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(random_bytes[i]);
    }
    return ss.str();
}

std::string Security::canonicalize_path(const std::string& path) {
    if (path.empty()) return "";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

    wchar_t full_buf[MAX_PATH * 2] = {0};
    DWORD len = GetFullPathNameW(wpath.c_str(), sizeof(full_buf) / sizeof(wchar_t), full_buf, NULL);
    if (len == 0 || len >= sizeof(full_buf) / sizeof(wchar_t)) {
        return path;
    }

    int sz = WideCharToMultiByte(CP_UTF8, 0, full_buf, -1, NULL, 0, NULL, NULL);
    std::string s(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, full_buf, -1, &s[0], sz, NULL, NULL);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

bool Security::is_path_safe_to_read(const std::string& path) {
    if (path.empty()) return false;

    std::string p = path;
    for (char& c : p) c = static_cast<char>(tolower(c));

    if (p.find("\\\\.\\") == 0 || p.find("\\\\?\\") == 0) {
        if (p.size() >= 6 && p.substr(0, 4) == "\\\\.\\" && isalpha(p[4]) && p[5] == ':') {
            return true;
        }
        return false;
    }

    std::vector<std::string> bad_devices = {
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4",
        "lpt1", "lpt2", "lpt3", "lpt4"
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

bool Security::is_protected_system_path(const std::string& raw_path) {
    std::string path = canonicalize_path(raw_path);
    if (path.empty()) return true;

    for (char& c : path) {
        if (c == '/') c = '\\';
        c = static_cast<char>(tolower(c));
    }

    while (path.size() > 3 && path.back() == '\\') {
        path.pop_back();
    }

    // 1. Root drives (e.g. C: or C:\)
    if (path.size() <= 3 && path.size() >= 2 && path[1] == ':') {
        return true;
    }

    // 2. Windows system paths
    wchar_t win_dir[MAX_PATH] = {0};
    if (GetWindowsDirectoryW(win_dir, MAX_PATH)) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, win_dir, -1, NULL, 0, NULL, NULL);
        std::string s_win(sz, 0);
        WideCharToMultiByte(CP_UTF8, 0, win_dir, -1, &s_win[0], sz, NULL, NULL);
        if (!s_win.empty() && s_win.back() == '\0') s_win.pop_back();
        for (char& c : s_win) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }

        if (path == s_win || path.find(s_win + "\\") == 0) {
            return true;
        }
    }

    // 3. System directories
    std::vector<std::string> protected_patterns = {
        "\\windows", "\\system32", "\\syswow64", "\\program files",
        "\\program files (x86)", "\\programdata\\microsoft",
        "\\system volume information", "\\$recycle.bin", "\\recovery",
        "\\boot", "\\pagefile.sys", "\\swapfile.sys", "\\hiberfil.sys"
    };

    for (const auto& pat : protected_patterns) {
        if (path.find(pat) != std::string::npos) {
            if (path.size() >= 2 && path[1] == ':') {
                std::string relative_from_drive = path.substr(2);
                if (relative_from_drive == pat || relative_from_drive.find(pat + "\\") == 0) {
                    return true;
                }
            }
        }
    }

    // 4. User profile root
    wchar_t user_prof[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"USERPROFILE", user_prof, MAX_PATH)) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, user_prof, -1, NULL, 0, NULL, NULL);
        std::string s_prof(sz, 0);
        WideCharToMultiByte(CP_UTF8, 0, user_prof, -1, &s_prof[0], sz, NULL, NULL);
        if (!s_prof.empty() && s_prof.back() == '\0') s_prof.pop_back();
        for (char& c : s_prof) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }
        if (path == s_prof) return true;
    }

    return false;
}

Security::DeleteResult Security::safe_recycle_item(const std::string& raw_path) {
    DeleteResult res;
    res.success = false;
    res.was_protected = false;

    std::string path = canonicalize_path(raw_path);

    if (is_protected_system_path(path)) {
        res.was_protected = true;
        res.error_message = "Path is protected by system security policy and cannot be deleted.";
        return res;
    }

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
        res.error_message = "Windows Shell error code: " + std::to_string(ret);
    }

    return res;
}

} // namespace TreeMCP

#else
// Linux / POSIX Implementation
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

namespace TreeMCP {

std::string Security::generate_secure_token() {
    unsigned char random_bytes[32] = {0};
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom.is_open()) {
        urandom.read(reinterpret_cast<char*>(random_bytes), sizeof(random_bytes));
    }

    std::stringstream ss;
    for (size_t i = 0; i < sizeof(random_bytes); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(random_bytes[i]);
    }
    return ss.str();
}

std::string Security::canonicalize_path(const std::string& path) {
    if (path.empty()) return "";
    char resolved[PATH_MAX] = {0};
    if (realpath(path.c_str(), resolved)) {
        return std::string(resolved);
    }
    return path;
}

bool Security::is_path_safe_to_read(const std::string& path) {
    if (path.empty()) return false;
    // Disallow dangerous relative path traversal to /dev or /proc sensitive files
    if (path.find("/dev/mem") != std::string::npos || path.find("/dev/kmem") != std::string::npos) {
        return false;
    }
    return true;
}

bool Security::is_protected_system_path(const std::string& raw_path) {
    std::string path = canonicalize_path(raw_path);
    if (path.empty()) return true;

    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }

    // Never allow wiping root or essential Linux system directories
    if (path == "/" ||
        path == "/bin" || path.find("/bin/") == 0 ||
        path == "/boot" || path.find("/boot/") == 0 ||
        path == "/dev" || path.find("/dev/") == 0 ||
        path == "/etc" || path.find("/etc/") == 0 ||
        path == "/lib" || path.find("/lib/") == 0 ||
        path == "/lib64" || path.find("/lib64/") == 0 ||
        path == "/proc" || path.find("/proc/") == 0 ||
        path == "/root" || path == "/sys" || path.find("/sys/") == 0 ||
        path == "/usr" || path.find("/usr/") == 0 ||
        path == "/sbin" || path.find("/sbin/") == 0 ||
        path == "/home") {
        return true;
    }

    const char* home = getenv("HOME");
    if (home && path == home) {
        // Protect entire user home directory from root deletion
        return true;
    }

    return false;
}

Security::DeleteResult Security::safe_recycle_item(const std::string& raw_path) {
    DeleteResult res;
    res.success = false;
    res.was_protected = false;

    std::string path = canonicalize_path(raw_path);

    if (is_protected_system_path(path)) {
        res.was_protected = true;
        res.error_message = "Path is protected by system security policy and cannot be deleted.";
        return res;
    }

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

    return res;
}

} // namespace TreeMCP
#endif

namespace TreeMCP {

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
    for (char c : str) {
        if (c == '\0' || c == '\r' || c == '\n') return true;
    }
    return false;
}

} // namespace TreeMCP
