#include "file_tree.h"
#include <sstream>
#include <iomanip>
#include <queue>
#include <stack>

namespace TreeMCP {

FileTree::FileTree() {
    clear();
}

void FileTree::clear() {
    nodes_.clear();
    string_pool_.clear();
    ext_pool_.clear();
    ext_to_id_.clear();
    root_idx_ = INVALID_INDEX;
    intern_ext(""); // 0 = no extension / directory
}

uint16_t FileTree::intern_ext(const std::string& ext) {
    auto it = ext_to_id_.find(ext);
    if (it != ext_to_id_.end()) {
        return it->second;
    }
    uint16_t id = static_cast<uint16_t>(ext_pool_.size());
    ext_pool_.push_back(ext);
    ext_to_id_[ext] = id;
    return id;
}

uint32_t FileTree::add_node(const std::string& name, uint64_t size, uint64_t allocated_size,
                            uint32_t parent_idx, bool is_dir, uint32_t attributes) {
    CompactNode node;
    node.name_offset = static_cast<uint32_t>(string_pool_.size());
    node.name_len = static_cast<uint32_t>(name.size());
    string_pool_.insert(string_pool_.end(), name.begin(), name.end());
    string_pool_.push_back('\0'); // safety null terminator

    node.size = size;
    node.allocated_size = allocated_size;
    node.parent_idx = parent_idx;
    node.first_child = INVALID_INDEX;
    node.next_sibling = INVALID_INDEX;
    node.file_count = is_dir ? 0 : 1;
    node.dir_count = is_dir ? 1 : 0;
    node.attributes = attributes;
    node.flags = is_dir ? 1 : 0;

    if (!is_dir) {
        size_t dot = name.find_last_of('.');
        if (dot != std::string::npos && dot + 1 < name.size()) {
            std::string ext = name.substr(dot);
            for (char& c : ext) c = static_cast<char>(tolower(c));
            node.ext_id = intern_ext(ext);
        } else {
            node.ext_id = 0;
        }
    } else {
        node.ext_id = 0;
    }

    uint32_t new_idx = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back(node);

    // Link to parent's child list (prepend for O(1), will sort later)
    if (parent_idx != INVALID_INDEX && parent_idx < nodes_.size()) {
        nodes_[new_idx].next_sibling = nodes_[parent_idx].first_child;
        nodes_[parent_idx].first_child = new_idx;
    }

    return new_idx;
}

std::string FileTree::get_name(uint32_t node_idx) const {
    if (node_idx >= nodes_.size()) return "";
    const auto& node = nodes_[node_idx];
    return std::string(&string_pool_[node.name_offset], node.name_len);
}

std::string FileTree::get_extension(uint32_t node_idx) const {
    if (node_idx >= nodes_.size()) return "";
    uint16_t id = nodes_[node_idx].ext_id;
    if (id < ext_pool_.size()) return ext_pool_[id];
    return "";
}

std::string FileTree::get_full_path(uint32_t node_idx) const {
    if (node_idx >= nodes_.size()) return "";
    std::vector<std::string> parts;
    uint32_t curr = node_idx;
    while (curr != INVALID_INDEX && curr < nodes_.size()) {
        parts.push_back(get_name(curr));
        curr = nodes_[curr].parent_idx;
    }
    std::reverse(parts.begin(), parts.end());
    std::string path;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0 && path.back() != '\\' && path.back() != '/') {
            path += "\\";
        }
        path += parts[i];
    }
    return path;
}

std::vector<uint32_t> FileTree::get_children(uint32_t parent_idx) const {
    std::vector<uint32_t> children;
    if (parent_idx >= nodes_.size()) return children;
    uint32_t curr = nodes_[parent_idx].first_child;
    while (curr != INVALID_INDEX && curr < nodes_.size()) {
        children.push_back(curr);
        curr = nodes_[curr].next_sibling;
    }
    return children;
}

void FileTree::aggregate_sizes() {
    // Process post-order (leaves to root) using topological order or reverse index if indexed bottom-up
    // Since nodes are added parent-before-child or arbitrary, we do post-order DFS
    if (root_idx_ == INVALID_INDEX || nodes_.empty()) return;

    std::vector<bool> visited(nodes_.size(), false);
    std::vector<uint32_t> post_order;
    post_order.reserve(nodes_.size());

    std::stack<uint32_t> st;
    st.push(root_idx_);

    while (!st.empty()) {
        uint32_t u = st.top();
        if (visited[u]) {
            st.pop();
            post_order.push_back(u);
            continue;
        }
        visited[u] = true;
        uint32_t child = nodes_[u].first_child;
        while (child != INVALID_INDEX && child < nodes_.size()) {
            st.push(child);
            child = nodes_[child].next_sibling;
        }
    }

    for (uint32_t idx : post_order) {
        auto& node = nodes_[idx];
        if (node.is_directory()) {
            uint64_t total_sz = 0;
            uint64_t total_alloc = 0;
            uint32_t f_count = 0;
            uint32_t d_count = 0;

            uint32_t child = node.first_child;
            while (child != INVALID_INDEX && child < nodes_.size()) {
                const auto& ch = nodes_[child];
                total_sz += ch.size;
                total_alloc += ch.allocated_size;
                f_count += ch.file_count;
                d_count += ch.dir_count;
                child = ch.next_sibling;
            }
            node.size = total_sz;
            node.allocated_size = total_alloc;
            node.file_count = f_count;
            node.dir_count = d_count + 1;
        }
    }
}

void FileTree::sort_children_by_size() {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (!nodes_[i].is_directory() || nodes_[i].first_child == INVALID_INDEX) continue;

        std::vector<uint32_t> ch;
        uint32_t curr = nodes_[i].first_child;
        while (curr != INVALID_INDEX && curr < nodes_.size()) {
            ch.push_back(curr);
            curr = nodes_[curr].next_sibling;
        }

        std::sort(ch.begin(), ch.end(), [this](uint32_t a, uint32_t b) {
            return nodes_[a].size > nodes_[b].size;
        });

        // Re-link sorted siblings
        nodes_[i].first_child = ch.empty() ? INVALID_INDEX : ch[0];
        for (size_t k = 0; k < ch.size(); ++k) {
            nodes_[ch[k]].next_sibling = (k + 1 < ch.size()) ? ch[k + 1] : INVALID_INDEX;
        }
    }
}

std::vector<ExtStat> FileTree::get_extension_stats(uint32_t from_idx, size_t limit) const {
    if (from_idx == INVALID_INDEX || from_idx >= nodes_.size()) return {};

    std::unordered_map<uint16_t, ExtStat> stats_map;
    std::stack<uint32_t> st;
    st.push(from_idx);

    while (!st.empty()) {
        uint32_t curr = st.top();
        st.pop();
        const auto& node = nodes_[curr];

        if (!node.is_directory()) {
            auto& es = stats_map[node.ext_id];
            es.count++;
            es.total_size += node.size;
        }

        uint32_t child = node.first_child;
        while (child != INVALID_INDEX && child < nodes_.size()) {
            st.push(child);
            child = nodes_[child].next_sibling;
        }
    }

    std::vector<ExtStat> results;
    results.reserve(stats_map.size());
    for (auto& pair : stats_map) {
        uint16_t id = pair.first;
        ExtStat stat = pair.second;
        stat.ext = (id < ext_pool_.size() && !ext_pool_[id].empty()) ? ext_pool_[id] : "[no extension]";
        stat.color = get_color_for_ext(stat.ext);
        results.push_back(stat);
    }

    std::sort(results.begin(), results.end(), [](const ExtStat& a, const ExtStat& b) {
        return a.total_size > b.total_size;
    });

    if (results.size() > limit) {
        results.resize(limit);
    }
    return results;
}

std::vector<FileTree::HogItem> FileTree::get_top_hogs(uint32_t from_idx, size_t limit, uint64_t min_bytes, const std::string& filter) const {
    if (from_idx == INVALID_INDEX || from_idx >= nodes_.size()) return {};

    std::vector<HogItem> hogs;
    std::stack<uint32_t> st;
    st.push(from_idx);

    bool inc_files = (filter == "all" || filter == "files");
    bool inc_dirs = (filter == "all" || filter == "dirs");

    while (!st.empty()) {
        uint32_t curr = st.top();
        st.pop();
        const auto& node = nodes_[curr];

        if (curr != from_idx && node.size >= min_bytes) {
            bool matches = false;
            if (node.is_directory() && inc_dirs) matches = true;
            if (!node.is_directory() && inc_files) matches = true;

            if (matches) {
                hogs.push_back({
                    curr,
                    get_full_path(curr),
                    node.size,
                    node.is_directory(),
                    node.file_count
                });
            }
        }

        uint32_t child = node.first_child;
        while (child != INVALID_INDEX && child < nodes_.size()) {
            st.push(child);
            child = nodes_[child].next_sibling;
        }
    }

    std::sort(hogs.begin(), hogs.end(), [](const HogItem& a, const HogItem& b) {
        return a.size > b.size;
    });

    if (hogs.size() > limit) {
        hogs.resize(limit);
    }
    return hogs;
}

std::vector<FileTree::SearchResult> FileTree::search(const std::string& pattern, uint64_t min_bytes, size_t limit) const {
    std::vector<SearchResult> results;
    if (root_idx_ == INVALID_INDEX) return results;

    std::string pat_lower = pattern;
    for (char& c : pat_lower) c = static_cast<char>(tolower(c));

    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        if (node.size < min_bytes) continue;

        std::string name = get_name(static_cast<uint32_t>(i));
        std::string name_lower = name;
        for (char& c : name_lower) c = static_cast<char>(tolower(c));

        if (pat_lower.empty() || name_lower.find(pat_lower) != std::string::npos) {
            results.push_back({
                static_cast<uint32_t>(i),
                get_full_path(static_cast<uint32_t>(i)),
                node.size,
                node.is_directory()
            });
            if (results.size() >= limit) break;
        }
    }

    return results;
}

std::string FileTree::format_size(uint64_t bytes) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= (1ULL << 40)) {
        ss << (static_cast<double>(bytes) / (1ULL << 40)) << " TB";
    } else if (bytes >= (1ULL << 30)) {
        ss << (static_cast<double>(bytes) / (1ULL << 30)) << " GB";
    } else if (bytes >= (1ULL << 20)) {
        ss << (static_cast<double>(bytes) / (1ULL << 20)) << " MB";
    } else if (bytes >= (1ULL << 10)) {
        ss << (static_cast<double>(bytes) / (1ULL << 10)) << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

std::string FileTree::get_color_for_ext(const std::string& ext) {
    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".wmv") return "#00d2d3"; // Cyan Video
    if (ext == ".zip" || ext == ".rar" || ext == ".7z" || ext == ".tar" || ext == ".gz") return "#ff9f43";  // Orange Archive
    if (ext == ".exe" || ext == ".dll" || ext == ".sys" || ext == ".msi" || ext == ".bin") return "#10ac84"; // Green Binary
    if (ext == ".iso" || ext == ".vhd" || ext == ".vhdx" || ext == ".img") return "#5f27cd";                 // Purple Disk Image
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".m4a") return "#ee5253";                 // Red Audio
    if (ext == ".jpg" || ext == ".png" || ext == ".gif" || ext == ".webp" || ext == ".bmp") return "#feca57"; // Yellow Image
    if (ext == ".pdf" || ext == ".doc" || ext == ".docx" || ext == ".xlsx" || ext == ".pptx") return "#54a0ff";// Blue Doc
    if (ext == ".cpp" || ext == ".h" || ext == ".js" || ext == ".ts" || ext == ".py" || ext == ".rs") return "#c8d6e5"; // Light Code
    return "#8395a7"; // Default Grey
}

} // namespace TreeMCP

