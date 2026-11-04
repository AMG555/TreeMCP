#pragma once

#include "../core/file_tree.h"
#include "../core/fast_scanner.h"
#include "token_optimizer.h"
#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace TreeMCP {

class McpProtocol {
public:
    McpProtocol(FileTree& tree, FastScanner& scanner);
    ~McpProtocol() = default;

    // Process a single JSON-RPC message and return JSON response
    std::string process_message(const std::string& raw_json);

    // Run stdio loop (blocks until EOF on stdin)
    void run_stdio_server();

    // Query counters
    uint64_t total_queries() const { return total_queries_; }
    uint64_t total_tokens_used() const { return total_tokens_used_; }
    uint64_t total_tokens_saved() const { return total_tokens_saved_; }

private:
    FileTree& tree_;
    FastScanner& scanner_;

    std::atomic<uint64_t> total_queries_{0};
    std::atomic<uint64_t> total_tokens_used_{0};
    std::atomic<uint64_t> total_tokens_saved_{0};

    // Tool implementations
    std::string handle_tools_list(const std::string& id);
    std::string handle_tools_call(const std::string& id, const std::string& tool_name, const std::string& args_json);
    std::string handle_resources_list(const std::string& id);
    std::string handle_initialize(const std::string& id);

    // Helpers
    static std::string make_json_rpc_result(const std::string& id, const std::string& result_content);
    static std::string make_json_rpc_error(const std::string& id, int code, const std::string& message);
    static std::string escape_json_string(const std::string& input);
    static std::string parse_json_field(const std::string& json, const std::string& field_name);
};

} // namespace TreeMCP

