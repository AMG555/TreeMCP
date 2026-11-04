#include "security.h"
#include <cstdint>
#include <wincrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

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
        // Fallback PRNG if crypto provider unavailable
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

    // Strip carriage return/newlines
    while (!host_lower.empty() && (host_lower.back() == '\r' || host_lower.back() == '\n' || host_lower.back() == ' ')) {
        host_lower.pop_back();
    }

    if (host_lower == "127.0.0.1" || host_lower == ("127.0.0.1" + port_str)) return true;
    if (host_lower == "localhost" || host_lower == ("localhost" + port_str)) return true;
    if (host_lower == "[::1]" || host_lower == ("[::1]" + port_str)) return true;

    return false;
}

std::wstring Security::canonicalize_path(const std::wstring& path) {
    if (path.empty()) return L"";
    wchar_t full_buf[MAX_PATH * 2] = {0};
    DWORD len = GetFullPathNameW(path.c_str(), sizeof(full_buf) / sizeof(wchar_t), full_buf, NULL);
    if (len == 0 || len >= sizeof(full_buf) / sizeof(wchar_t)) {
        return path;
    }
    return std::wstring(full_buf);
}

bool Security::is_path_safe_to_read(const std::wstring& path) {
    if (path.empty()) return false;

    // Check for DOS device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9)
    std::wstring p = path;
    for (wchar_t& c : p) c = towlower(c);

    // Reject raw device paths like \\.\nul or \\?\nul
    if (p.find(L"\\\\.\\") == 0 || p.find(L"\\\\?\\") == 0) {
        // Only allow volume format \\.\X: for direct MFT reader
        if (p.size() >= 6 && p.substr(0, 4) == L"\\\\.\\" && iswalpha(p[4]) && p[5] == L':') {
            return true;
        }
        return false;
    }

    // Check forbidden device keywords
    std::vector<std::wstring> bad_devices = {
        L"con", L"prn", L"aux", L"nul",
        L"com1", L"com2", L"com3", L"com4",
        L"lpt1", L"lpt2", L"lpt3", L"lpt4"
    };

    for (const auto& dev : bad_devices) {
        size_t pos = p.find(dev);
        if (pos != std::wstring::npos) {
            // Check if it's isolated by separators or end
            bool left_bound = (pos == 0 || p[pos - 1] == L'\\' || p[pos - 1] == L'/');
            bool right_bound = (pos + dev.size() == p.size() || p[pos + dev.size()] == L'\\' || p[pos + dev.size()] == L'/' || p[pos + dev.size()] == L'.');
            if (left_bound && right_bound) {
                return false;
            }
        }
    }

    return true;
}

bool Security::is_protected_system_path(const std::wstring& raw_path) {
    std::wstring path = canonicalize_path(raw_path);
    if (path.empty()) return true;

    // Normalize slashes
    for (wchar_t& c : path) {
        if (c == L'/') c = L'\\';
        c = towlower(c);
    }

    while (path.size() > 3 && path.back() == L'\\') {
        path.pop_back();
    }

    // 1. Never allow deleting root drives (e.g. C: or C:\)
    if (path.size() <= 3 && path.size() >= 2 && path[1] == L':') {
        return true;
    }

    // 2. Windows system paths
    wchar_t win_dir[MAX_PATH] = {0};
    if (GetWindowsDirectoryW(win_dir, MAX_PATH)) {
        std::wstring w_win = win_dir;
        for (wchar_t& c : w_win) { if (c == L'/') c = L'\\'; c = towlower(c); }
        if (path == w_win || path.find(w_win + L"\\") == 0) {
            return true;
        }
    }

    // 3. System drive root directories
    std::vector<std::wstring> protected_patterns = {
        L"\\windows",
        L"\\system32",
        L"\\syswow64",
        L"\\program files",
        L"\\program files (x86)",
        L"\\programdata\\microsoft",
        L"\\system volume information",
        L"\\$recycle.bin",
        L"\\recovery",
        L"\\boot",
        L"\\pagefile.sys",
        L"\\swapfile.sys",
        L"\\hiberfil.sys",
        L"\\bootmgr",
        L"\\ntldr"
    };

    for (const auto& pat : protected_patterns) {
        if (path.find(pat) != std::wstring::npos) {
            // Check if it's top-level or protected system folder
            if (path.size() >= 2 && path[1] == L':') {
                std::wstring relative_from_drive = path.substr(2);
                if (relative_from_drive == pat || relative_from_drive.find(pat + L"\\") == 0) {
                    return true;
                }
            }
        }
    }

    // 4. Protect user profile root directory (e.g. C:\Users\<Username> without subfolder)
    wchar_t user_prof[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"USERPROFILE", user_prof, MAX_PATH)) {
        std::wstring w_prof = user_prof;
        for (wchar_t& c : w_prof) { if (c == L'/') c = L'\\'; c = towlower(c); }
        if (path == w_prof) {
            // Do not delete entire user home!
            return true;
        }
    }

    return false;
}

Security::DeleteResult Security::safe_recycle_item(const std::wstring& raw_path) {
    DeleteResult res;
    res.success = false;
    res.was_protected = false;

    std::wstring path = canonicalize_path(raw_path);

    if (is_protected_system_path(path)) {
        res.was_protected = true;
        res.error_message = "Path is protected by system security policy and cannot be deleted.";
        return res;
    }

    // Windows SHFileOperation requires double null-terminated string
    std::vector<wchar_t> double_null_path(path.begin(), path.end());
    double_null_path.push_back(L'\0');
    double_null_path.push_back(L'\0');

    SHFILEOPSTRUCTW file_op{};
    file_op.wFunc = FO_DELETE;
    file_op.pFrom = double_null_path.data();
    file_op.pTo = NULL;
    // FOF_ALLOWUNDO: Sends file safely to Windows Recycle Bin!
    // FOF_NOCONFIRMATION | FOF_SILENT: Non-interactive
    file_op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

    int ret = SHFileOperationW(&file_op);
    if (ret == 0 && !file_op.fAnyOperationsAborted) {
        res.success = true;
    } else {
        res.error_message = "Windows Shell error code: " + std::to_string(ret);
    }

    return res;
}

bool Security::contains_dangerous_chars(const std::string& str) {
    for (char c : str) {
        if (c == '\0' || c == '\r' || c == '\n') return true;
    }
    return false;
}

} // namespace TreeMCP
