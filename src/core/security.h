#pragma once

#include <cstdint>
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

    // Active server runtime descriptor
    struct ActiveServerInfo {
        int port = 0;
        std::string token;
        uint32_t pid = 0;
        bool is_admin = false;
        bool is_valid = false;
    };

    // Save active server information to shared runtime locations
    static void save_active_server(int port, const std::string& token, bool is_admin);

    // Remove active server registration on shutdown
    static void clear_active_server();

    // Discover if an active or elevated TreeMCP server is running
    static ActiveServerInfo get_active_server();

    // Send HTTP POST to local TreeMCP server
    static std::string http_post_local(int port, const std::string& path, const std::string& token, const std::string& body, int timeout_ms = 3000);

    // Send HTTP GET to local TreeMCP server
    static std::string http_get_local(int port, const std::string& path, const std::string& token, int timeout_ms = 1500);
};

} // namespace TreeMCP
