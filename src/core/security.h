#pragma once

#include <string>
#include <vector>

namespace TreeMCP {

class Security {
public:
    // Generate cryptographically secure random hexadecimal token (256-bit / 32 bytes)
    static std::string generate_secure_token();

    // Constant-time token comparison to avoid timing attacks
    static bool constant_time_compare(const std::string& a, const std::string& b);

    // Validate Host header to prevent DNS rebinding attacks
    static bool is_valid_host_header(const std::string& host_header, int expected_port);

    // Validate that path is well-formed, non-traversal, and not pointing to system devices
    static bool is_path_safe_to_read(const std::string& path);

    // Check if path is a protected operating system directory or volume root
    static bool is_protected_system_path(const std::string& path);

    // Delete item safely (Windows Recycle Bin / Linux FreeDesktop Trash)
    struct DeleteResult {
        bool success;
        bool was_protected;
        std::string error_message;
    };
    static DeleteResult safe_recycle_item(const std::string& path, bool force_system_override = false);

    // Canonicalize path to absolute form without dots or relative segments
    static std::string canonicalize_path(const std::string& path);

    // Check if string contains dangerous control characters or null bytes
    static bool contains_dangerous_chars(const std::string& str);
};

} // namespace TreeMCP
