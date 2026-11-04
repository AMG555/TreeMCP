#pragma once

#include "../core/file_tree.h"
#include "../core/types.h"
#include <string>
#include <vector>

namespace TreeMCP {

class TokenOptimizer {
public:
    // Format drive summary in under ~25 tokens
    static std::string format_drives(const std::vector<DriveInfo>& drives);

    // Format top disk hogs in under ~80 tokens
    static std::string format_hogs(const std::vector<FileTree::HogItem>& hogs, size_t limit = 15);

    // Format indented directory tree with threshold rollup in under ~60 tokens
    static std::string format_tree(const FileTree& tree, uint32_t root_idx, int max_depth = 1, uint64_t min_bytes = 50 * 1024 * 1024);

    // Format extension breakdown in under ~40 tokens
    static std::string format_extensions(const std::vector<ExtStat>& stats, uint64_t total_scanned_bytes);

    // Format file search results in under ~50 tokens
    static std::string format_search(const std::vector<FileTree::SearchResult>& results);

    // Token estimation (standard ~4 characters per token heuristic for technical English)
    static size_t estimate_tokens(const std::string& text);
    
    // Compare tokens against standard verbose JSON
    static size_t estimate_standard_json_tokens(size_t item_count);
};

} // namespace TreeMCP

