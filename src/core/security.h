#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace TreeMCP {

class Security {
public:
    // Generate cryptographically secure random hexadecimal token (256-bit / 32 bytes)
    static std::string generate_secure_token();

    // Constant-time token comparison to avoid timing attacks
    static bool constant_time_compare(const std::string& a, const std::string& b);

    // Validate Host header to prevent DNS rebinding attacks
    static bool is_valid_host_header(const std::string& host_header, int expected_port);

    // Validate that path is well-formed, non-traversal, and not pointing to Windows devices
    static bool is_path_safe_to_read(const std::wstring& path);

    // Check if path is a protected Windows operating system directory or volume root
    static bool is_protected_system_path(const std::wstring& path);

    // Delete item safely to Windows Recycle Bin (FOF_ALLOWUNDO) instead of permanent deletion
    struct DeleteResult {
        bool success;
        bool was_protected;
        std::string error_message;
    };
    static DeleteResult safe_recycle_item(const std::wstring& path);

    // Canonicalize path to absolute form without dots or relative segments
    static std::wstring canonicalize_path(const std::wstring& path);

    // Check if string contains dangerous control characters or null bytes
    static bool contains_dangerous_chars(const std::string& str);
};

} // namespace TreeMCP
