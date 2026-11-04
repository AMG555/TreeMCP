#pragma once

#include "types.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace TreeMCP {

class FileTree {
public:
    FileTree();
    ~FileTree() = default;

    void clear();

    // Node management
    uint32_t add_node(const std::string& name, uint64_t size, uint64_t allocated_size,
                      uint32_t parent_idx, bool is_dir, uint32_t attributes);

#ifdef _WIN32
    // High-speed Win32 direct wide-character ingestion (zero heap allocation)
    uint32_t add_node_w(const wchar_t* wname, uint64_t size, uint64_t allocated_size,
                        uint32_t parent_idx, bool is_dir, uint32_t attributes);
#endif

    std::string get_name(uint32_t node_idx) const;
    std::string get_full_path(uint32_t node_idx) const;
    std::string get_extension(uint32_t node_idx) const;

    const CompactNode& get_node(uint32_t idx) const { return nodes_[idx]; }
    CompactNode& get_node_mut(uint32_t idx) { return nodes_[idx]; }
    size_t node_count() const { return nodes_.size(); }
    uint32_t root_idx() const { return root_idx_; }
    void set_root_idx(uint32_t idx) { root_idx_ = idx; }

    // Aggregations
    void aggregate_sizes();
    void sort_children_by_size();

    // Queries
    std::vector<uint32_t> get_children(uint32_t parent_idx) const;
    std::vector<ExtStat> get_extension_stats(uint32_t from_idx, size_t limit = 20) const;
    
    struct HogItem {
        uint32_t node_idx;
        std::string full_path;
        uint64_t size;
        bool is_dir;
        uint32_t file_count;
    };
    std::vector<HogItem> get_top_hogs(uint32_t from_idx, size_t limit = 20, uint64_t min_bytes = 0, const std::string& filter = "all") const;

    struct SearchResult {
        uint32_t node_idx;
        std::string full_path;
        uint64_t size;
        bool is_dir;
    };
    std::vector<SearchResult> search(const std::string& pattern, uint64_t min_bytes = 0, size_t limit = 50) const;

    struct DuplicateCandidate {
        std::string name;
        std::string extension;
        uint64_t size;
        uint64_t wasted_bytes;
        std::vector<std::string> paths;
    };
    std::vector<DuplicateCandidate> get_duplicate_candidates(uint64_t min_bytes = 1048576, size_t limit = 50) const;

    // Helpers
    static std::string format_size(uint64_t bytes);
    static std::string get_color_for_ext(const std::string& ext);

private:
    std::vector<CompactNode> nodes_;
    std::vector<char> string_pool_;
    uint32_t root_idx_ = INVALID_INDEX;

    // Extension interning
    std::vector<std::string> ext_pool_;
    std::unordered_map<std::string, uint16_t> ext_to_id_;
    uint16_t intern_ext(const std::string& ext);
    uint16_t intern_ext_fast(const char* ext_data, size_t ext_len);
};

} // namespace TreeMCP

