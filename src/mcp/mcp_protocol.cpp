#include "mcp_protocol.h"
#include "../core/security.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace TreeMCP {

McpProtocol::McpProtocol(FileTree& tree, FastScanner& scanner)
    : tree_(tree), scanner_(scanner) {}

std::string McpProtocol::escape_json_string(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string McpProtocol::parse_json_field(const std::string& json, const std::string& field_name) {
    std::string pattern = "\"" + field_name + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos += pattern.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        pos++; // skip quote
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                val += json[pos + 1];
                pos += 2;
            } else {
                val += json[pos];
                pos++;
            }
        }
        return val;
    } else {
        // Number or boolean or null
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']' && json[end] != ' ' && json[end] != '\r' && json[end] != '\n') {
            end++;
        }
        return json.substr(pos, end - pos);
    }
}

std::string McpProtocol::make_json_rpc_result(const std::string& id, const std::string& result_content) {
    std::string id_field = id.empty() ? "null" : (id[0] == '"' ? id : ("\"" + id + "\""));
    return "{\"jsonrpc\":\"2.0\",\"id\":" + id_field + ",\"result\":" + result_content + "}\n";
}

std::string McpProtocol::make_json_rpc_error(const std::string& id, int code, const std::string& message) {
    std::string id_field = id.empty() ? "null" : (id[0] == '"' ? id : ("\"" + id + "\""));
    return "{\"jsonrpc\":\"2.0\",\"id\":" + id_field + ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":\"" + escape_json_string(message) + "\"}}\n";
}

std::string McpProtocol::handle_initialize(const std::string& id) {
    return make_json_rpc_result(id,
        "{\"protocolVersion\":\"2024-11-05\","
        "\"capabilities\":{\"tools\":{},\"resources\":{}},"
        "\"serverInfo\":{\"name\":\"TreeMCP\",\"version\":\"1.1.0\"}}"
    );
}

std::string McpProtocol::handle_tools_list(const std::string& id) {
    std::string tools_json =
        "{\"tools\":["
        "{"
            "\"name\":\"treemcp_drive_summary\","
            "\"description\":\"Get instant summary of all local drives, capacity, free space, and file system with minimum tokens (~25 tokens).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}"
        "},"
        "{"
            "\"name\":\"treemcp_scan\","
            "\"description\":\"Scan and index a directory or drive for instant tree and treemap analysis (safe paths only).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Path to scan (e.g. C:\\\\ or D:\\\\Projects)\"}},\"required\":[\"path\"]}"
        "},"
        "{"
            "\"name\":\"treemcp_top_hogs\","
            "\"description\":\"Find the largest files or folders consuming disk space, formatted in ultra-compact tokens (~80 tokens).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
                "\"limit\":{\"type\":\"integer\",\"description\":\"Max items to return (default 15)\"},"
                "\"min_mb\":{\"type\":\"integer\",\"description\":\"Minimum size in MB (default 50)\"},"
                "\"filter\":{\"type\":\"string\",\"enum\":[\"all\",\"files\",\"dirs\"],\"description\":\"Filter by item type\"}"
            "}}"
        "},"
        "{"
            "\"name\":\"treemcp_dir_tree\","
            "\"description\":\"Get compact, depth-limited directory tree with percentage rollup and size threshold (~60 tokens).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
                "\"max_depth\":{\"type\":\"integer\",\"description\":\"Max folder depth (default 1)\"},"
                "\"min_mb\":{\"type\":\"integer\",\"description\":\"Minimum size in MB (default 50)\"}"
            "}}"
        "},"
        "{"
            "\"name\":\"treemcp_extension_stats\","
            "\"description\":\"Get breakdown of disk space consumed by file extension (.mp4, .zip, .dll, etc.) (~40 tokens).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
                "\"limit\":{\"type\":\"integer\",\"description\":\"Top N extensions (default 10)\"}"
            "}}"
        "},"
        "{"
            "\"name\":\"treemcp_search\","
            "\"description\":\"Instant search for files matching substring/extension with size filtering (~50 tokens).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
                "\"pattern\":{\"type\":\"string\",\"description\":\"Search substring (e.g. node_modules, .iso, cache)\"},"
                "\"min_mb\":{\"type\":\"integer\",\"description\":\"Minimum size in MB\"},"
                "\"limit\":{\"type\":\"integer\",\"description\":\"Max results (default 20)\"}"
            "},\"required\":[\"pattern\"]}"
        "},"
        "{"
            "\"name\":\"treemcp_clean\","
            "\"description\":\"Safely preview or delete a space-consuming directory or file. Uses Windows Recycle Bin and blocks protected system paths.\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
                "\"path\":{\"type\":\"string\",\"description\":\"Path to clean/delete\"},"
                "\"dry_run\":{\"type\":\"boolean\",\"description\":\"If true, only previews space to be reclaimed without deleting (default true)\"}"
            "},\"required\":[\"path\"]}"
        "}"
        "]}";

    return make_json_rpc_result(id, tools_json);
}

std::string McpProtocol::handle_resources_list(const std::string& id) {
    return make_json_rpc_result(id, "{\"resources\":[]}");
}

std::string McpProtocol::handle_tools_call(const std::string& id, const std::string& tool_name, const std::string& args_json) {
    total_queries_++;
    std::string text_result;
    size_t standard_tokens = 0;

    if (tool_name == "treemcp_drive_summary") {
        auto drives = FastScanner::list_drives();
        text_result = TokenOptimizer::format_drives(drives);
        standard_tokens = TokenOptimizer::estimate_standard_json_tokens(drives.size() * 3);
    }
    else if (tool_name == "treemcp_scan") {
        std::string path_str = parse_json_field(args_json, "path");
        if (path_str.empty()) {
#ifdef _WIN32
            path_str = "C:\\";
#else
            path_str = "/";
#endif
        }

        // Security check
        if (!Security::is_path_safe_to_read(path_str)) {
            return make_json_rpc_error(id, -32602, "Security check failed: Path contains invalid or reserved device names.");
        }

        ScanStats stats;
        bool ok = scanner_.scan(path_str, tree_, stats);
        if (ok) {
            std::stringstream ss;
            ss << "SCAN COMPLETE: " << path_str << " in " << std::fixed << std::setprecision(2)
               << (stats.duration_ms / 1000.0) << "s. Indexed " << stats.total_files << " files, "
               << stats.total_dirs << " folders (" << FileTree::format_size(stats.total_bytes) << ").";
            text_result = ss.str();
            standard_tokens = TokenOptimizer::estimate_standard_json_tokens(stats.total_files);
        } else {
            text_result = "Scan failed or was cancelled.";
            standard_tokens = 20;
        }
    }
    else if (tool_name == "treemcp_top_hogs") {
        std::string limit_str = parse_json_field(args_json, "limit");
        std::string min_mb_str = parse_json_field(args_json, "min_mb");
        std::string filter_str = parse_json_field(args_json, "filter");

        size_t limit = limit_str.empty() ? 15 : std::stoull(limit_str);
        if (limit > 100) limit = 100; // Cap limit to prevent memory/token blowup

        uint64_t min_bytes = min_mb_str.empty() ? 0 : (std::stoull(min_mb_str) * 1024 * 1024);
        if (filter_str.empty()) filter_str = "all";

        auto hogs = tree_.get_top_hogs(tree_.root_idx(), limit, min_bytes, filter_str);
        text_result = TokenOptimizer::format_hogs(hogs, limit);
        standard_tokens = TokenOptimizer::estimate_standard_json_tokens(hogs.size() * 5);
    }
    else if (tool_name == "treemcp_dir_tree") {
        std::string depth_str = parse_json_field(args_json, "max_depth");
        std::string min_mb_str = parse_json_field(args_json, "min_mb");

        int max_depth = depth_str.empty() ? 1 : std::stoi(depth_str);
        if (max_depth > 5) max_depth = 5; // Guard maximum tree depth

        uint64_t min_bytes = min_mb_str.empty() ? 0 : (std::stoull(min_mb_str) * 1024 * 1024);

        text_result = TokenOptimizer::format_tree(tree_, tree_.root_idx(), max_depth, min_bytes);
        standard_tokens = TokenOptimizer::estimate_standard_json_tokens(30);
    }
    else if (tool_name == "treemcp_extension_stats") {
        std::string limit_str = parse_json_field(args_json, "limit");
        size_t limit = limit_str.empty() ? 10 : std::stoull(limit_str);
        if (limit > 50) limit = 50;

        auto stats = tree_.get_extension_stats(tree_.root_idx(), limit);
        uint64_t total_scanned = (tree_.root_idx() != INVALID_INDEX) ? tree_.get_node(tree_.root_idx()).size : 0;
        text_result = TokenOptimizer::format_extensions(stats, total_scanned);
        standard_tokens = TokenOptimizer::estimate_standard_json_tokens(stats.size() * 3);
    }
    else if (tool_name == "treemcp_search") {
        std::string pattern = parse_json_field(args_json, "pattern");
        std::string min_mb_str = parse_json_field(args_json, "min_mb");
        std::string limit_str = parse_json_field(args_json, "limit");

        uint64_t min_bytes = min_mb_str.empty() ? 0 : (std::stoull(min_mb_str) * 1024 * 1024);
        size_t limit = limit_str.empty() ? 20 : std::stoull(limit_str);
        if (limit > 100) limit = 100;

        auto results = tree_.search(pattern, min_bytes, limit);
        text_result = TokenOptimizer::format_search(results);
        standard_tokens = TokenOptimizer::estimate_standard_json_tokens(results.size() * 4);
    }
    else if (tool_name == "treemcp_clean") {
        std::string path_str = parse_json_field(args_json, "path");
        std::string dry_run_str = parse_json_field(args_json, "dry_run");
        bool dry_run = (dry_run_str == "true" || dry_run_str.empty());

        if (path_str.empty()) {
            return make_json_rpc_error(id, -32602, "Path is required for treemcp_clean.");
        }

        // 1. Critical security check: Protected system directory check
        if (Security::is_protected_system_path(path_str)) {
            text_result = "[BLOCKED BY SECURITY POLICY] Path '" + path_str + "' is a protected system directory or root filesystem. Deletion denied.";
            standard_tokens = 30;
        } else if (dry_run) {
            text_result = "[DRY RUN SAFE PREVIEW] Would move to Recycle Bin / Trash: " + path_str + ". Pass dry_run=false to execute.";
            standard_tokens = 30;
        } else {
            // Safe deletion via Windows Recycle Bin / Linux Trash (non-destructive)
            auto del_res = Security::safe_recycle_item(path_str);
            if (del_res.success) {
                text_result = "[SUCCESS] Safely moved to Recycle Bin / Trash: " + path_str;
            } else {
                text_result = "[FAILED] Could not recycle item: " + del_res.error_message;
            }
            standard_tokens = 30;
        }
    }
    else {
        return make_json_rpc_error(id, -32601, "Tool not found: " + tool_name);
    }

    size_t actual_tokens = TokenOptimizer::estimate_tokens(text_result);
    total_tokens_used_ += actual_tokens;
    if (standard_tokens > actual_tokens) {
        total_tokens_saved_ += (standard_tokens - actual_tokens);
    }

    std::string response_payload =
        "{\"content\":[{\"type\":\"text\",\"text\":\"" + escape_json_string(text_result) + "\"}]}";

    return make_json_rpc_result(id, response_payload);
}

std::string McpProtocol::process_message(const std::string& raw_json) {
    if (raw_json.empty()) return "";

    // Security check: Payload size cap (64 KB) to prevent memory exhaustion DoS
    if (raw_json.size() > 65536) {
        return make_json_rpc_error("", -32600, "Payload exceeds 64 KB security limit.");
    }

    std::string id = parse_json_field(raw_json, "id");
    std::string method = parse_json_field(raw_json, "method");

    if (method == "initialize") {
        return handle_initialize(id);
    } else if (method == "notifications/initialized") {
        return ""; // Notifications receive no response
    } else if (method == "ping") {
        return make_json_rpc_result(id, "{}");
    } else if (method == "tools/list") {
        return handle_tools_list(id);
    } else if (method == "resources/list") {
        return handle_resources_list(id);
    } else if (method == "tools/call") {
        std::string tool_name = parse_json_field(raw_json, "name");
        size_t args_pos = raw_json.find("\"arguments\"");
        std::string args_json = "{}";
        if (args_pos != std::string::npos) {
            args_json = raw_json.substr(args_pos);
        }
        return handle_tools_call(id, tool_name, args_json);
    }

    return make_json_rpc_error(id, -32601, "Method not supported: " + method);
}

void McpProtocol::run_stdio_server() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::string resp = process_message(line);
        if (!resp.empty()) {
            std::cout << resp << std::flush;
        }
    }
}

} // namespace TreeMCP
