#include "file_tree.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
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
    // Reserve upfront to eliminate reallocations on millions of items
    nodes_.reserve(524288);
    string_pool_.reserve(16777216);
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

uint16_t FileTree::intern_ext_fast(const char* ext_data, size_t ext_len) {
    if (ext_len == 0) return 0;
    std::string ext(ext_data, ext_len);
    auto it = ext_to_id_.find(ext);
    if (it != ext_to_id_.end()) {
        return it->second;
    }
    uint16_t id = static_cast<uint16_t>(ext_pool_.size());
    ext_pool_.push_back(ext);
    ext_to_id_[ext] = id;
    return id;
}

#ifdef _WIN32
uint32_t FileTree::add_node_w(const wchar_t* wname, uint64_t size, uint64_t allocated_size,
                              uint32_t parent_idx, bool is_dir, uint32_t attributes) {
    CompactNode node;
    node.name_offset = static_cast<uint32_t>(string_pool_.size());

    // Ultra-fast ASCII path: avoid WideCharToMultiByte kernel calls
    const wchar_t* p = wname;
    bool is_ascii = true;
    while (*p) {
        if (*p >= 128) { is_ascii = false; break; }
        p++;
    }

    if (is_ascii) {
        size_t len = p - wname;
        node.name_len = static_cast<uint16_t>(len);
        for (size_t i = 0; i < len; ++i) {
            string_pool_.push_back(static_cast<char>(wname[i]));
        }
        string_pool_.push_back('\0');
    } else {
        int sz = WideCharToMultiByte(CP_UTF8, 0, wname, -1, NULL, 0, NULL, NULL);
        if (sz > 0) {
            size_t old_sz = string_pool_.size();
            string_pool_.resize(old_sz + sz);
            WideCharToMultiByte(CP_UTF8, 0, wname, -1, &string_pool_[old_sz], sz, NULL, NULL);
            node.name_len = static_cast<uint16_t>(sz - 1);
        } else {
            node.name_len = 0;
            string_pool_.push_back('\0');
        }
    }

    node.size = size;
    node.allocated_size = allocated_size;
    node.parent_idx = parent_idx;
    node.first_child = INVALID_INDEX;
    node.next_sibling = INVALID_INDEX;
    node.file_count = is_dir ? 0 : 1;
    node.dir_count = is_dir ? 1 : 0;
    node.attributes = static_cast<uint16_t>(attributes);
    node.flags = is_dir ? 1 : 0;

    if (!is_dir) {
        const wchar_t* dot = nullptr;
        const wchar_t* scan = wname;
        while (*scan) {
            if (*scan == L'.') dot = scan;
            scan++;
        }
        if (dot && dot + 1 < scan && (scan - dot) < 16) {
            char ext_buf[16];
            size_t ext_len = scan - dot;
            for (size_t i = 0; i < ext_len; ++i) {
                wchar_t wc = dot[i];
                ext_buf[i] = (wc < 128) ? static_cast<char>(tolower(static_cast<int>(wc))) : '?';
            }
            node.ext_id = intern_ext_fast(ext_buf, ext_len);
        } else {
            node.ext_id = 0;
        }
    } else {
        node.ext_id = 0;
    }

    uint32_t new_idx = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back(node);

    if (parent_idx != INVALID_INDEX && parent_idx < nodes_.size()) {
        nodes_[new_idx].next_sibling = nodes_[parent_idx].first_child;
        nodes_[parent_idx].first_child = new_idx;
    }

    return new_idx;
}
#endif

uint32_t FileTree::add_node(const std::string& name, uint64_t size, uint64_t allocated_size,
                            uint32_t parent_idx, bool is_dir, uint32_t attributes) {
    CompactNode node;
    node.name_offset = static_cast<uint32_t>(string_pool_.size());
    node.name_len = static_cast<uint16_t>(name.size());
    string_pool_.insert(string_pool_.end(), name.begin(), name.end());
    string_pool_.push_back('\0'); // safety null terminator

    node.size = size;
    node.allocated_size = allocated_size;
    node.parent_idx = parent_idx;
    node.first_child = INVALID_INDEX;
    node.next_sibling = INVALID_INDEX;
    node.file_count = is_dir ? 0 : 1;
    node.dir_count = is_dir ? 1 : 0;
    node.attributes = static_cast<uint16_t>(attributes);
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
    if (root_idx_ == INVALID_INDEX || nodes_.empty()) return;

    // Linear bottom-up single-pass aggregation (parents always precede children in index order)
    for (size_t i = nodes_.size(); i > 0; --i) {
        size_t idx = i - 1;
        uint32_t p = nodes_[idx].parent_idx;
        if (p != INVALID_INDEX && p < nodes_.size()) {
            nodes_[p].size += nodes_[idx].size;
            nodes_[p].allocated_size += nodes_[idx].allocated_size;
            nodes_[p].file_count += nodes_[idx].file_count;
            if (nodes_[idx].is_directory()) {
                nodes_[p].dir_count += (nodes_[idx].dir_count + 1);
            }
        }
    }
}

void FileTree::sort_children_by_size() {
    std::vector<uint32_t> ch;
    ch.reserve(1024);

    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (!nodes_[i].is_directory() || nodes_[i].first_child == INVALID_INDEX) continue;

        ch.clear();
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

std::vector<FileTree::DuplicateCandidate> FileTree::get_duplicate_candidates(uint64_t min_bytes, size_t limit) const {
    std::vector<DuplicateCandidate> duplicates;
    if (root_idx_ == INVALID_INDEX || nodes_.empty()) return duplicates;

    struct Group {
        std::string name;
        std::string ext;
        uint64_t size;
        std::vector<std::string> paths;
    };
    std::unordered_map<std::string, Group> groups;
    groups.reserve(65536);

    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        if (!node.is_directory() && node.size >= min_bytes) {
            std::string name = get_name(static_cast<uint32_t>(i));
            std::string key = std::to_string(node.size) + "_" + name;
            auto it = groups.find(key);
            if (it == groups.end()) {
                groups[key] = {name, get_extension(static_cast<uint32_t>(i)), node.size, {get_full_path(static_cast<uint32_t>(i))}};
            } else {
                it->second.paths.push_back(get_full_path(static_cast<uint32_t>(i)));
            }
        }
    }

    for (auto& pair : groups) {
        if (pair.second.paths.size() >= 2) {
            uint64_t wasted = (pair.second.paths.size() - 1) * pair.second.size;
            duplicates.push_back({
                std::move(pair.second.name),
                std::move(pair.second.ext),
                pair.second.size,
                wasted,
                std::move(pair.second.paths)
            });
        }
    }

    std::sort(duplicates.begin(), duplicates.end(), [](const DuplicateCandidate& a, const DuplicateCandidate& b) {
        return a.wasted_bytes > b.wasted_bytes;
    });

    if (duplicates.size() > limit) {
        duplicates.resize(limit);
    }
    return duplicates;
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
    // WizTree Canonical Extension Color Palette
    if (ext == ".dll") return "#8dc63f"; // Lime green (.dll)
    if (ext == "[no extension]" || ext.empty()) return "#4a90e2"; // Soft slate blue
    if (ext == ".exe") return "#0099ff"; // Electric azure (.exe)
    if (ext == ".sys") return "#00a896"; // Sea green (.sys)
    if (ext == ".msi") return "#02c39a"; // Mint teal (.msi)
    if (ext == ".bin") return "#f39c12"; // Amber orange (.bin)
    if (ext == ".mkv" || ext == ".mp4" || ext == ".avi" || ext == ".mov" || ext == ".wmv") return "#ff5722"; // Bright Orange-Red (Video)
    if (ext == ".db" || ext == ".sqlite" || ext == ".mdf" || ext == ".ldf") return "#27ae60"; // Forest emerald (.db)
    if (ext == ".lib" || ext == ".a" || ext == ".obj" || ext == ".o") return "#9b51e0"; // Royal purple (.lib)
    if (ext == ".js" || ext == ".ts" || ext == ".jsx" || ext == ".tsx") return "#f1c40f"; // Vivid yellow (.js)
    if (ext == ".cat") return "#e74c3c"; // Crimson red (.cat)
    if (ext == ".pak" || ext == ".zip" || ext == ".rar" || ext == ".7z" || ext == ".tar" || ext == ".gz") return "#ff9800"; // Deep Tangerine (Archives)
    if (ext == ".json" || ext == ".xml" || ext == ".yaml" || ext == ".yml") return "#e91e63"; // Hot magenta (.json/data)
    if (ext == ".so" || ext == ".dylib") return "#00b4d8"; // Cyan (.so)
    if (ext == ".iso" || ext == ".vhd" || ext == ".vhdx" || ext == ".img" ||
        ext == ".vmdk" || ext == ".vdi" || ext == ".vmem" || ext == ".ova" || ext == ".ovf" || ext == ".qcow2") return "#9c27b0"; // Vivid Violet (Disk images & VMs)
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".aac" || ext == ".ogg") return "#e84393"; // Pink rose (Audio)
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".gif" || ext == ".bmp") return "#fbc531"; // Sunny gold (Images)
    if (ext == ".pdf") return "#d63031"; // Acrobat red (.pdf)
    if (ext == ".docx" || ext == ".doc" || ext == ".xlsx" || ext == ".pptx" || ext == ".txt") return "#2980b9"; // Steel Blue (Documents)
    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".rs" || ext == ".py" || ext == ".go") return "#2ed573"; // Bright lime green (Code)
    if (ext == ".crypt14" || ext == ".crypt15" || ext == ".enc" || ext == ".bak") return "#a55eea"; // Lavender purple

    // Deterministic golden-ratio HSV generator for any arbitrary extension
    uint32_t hash = 2166136261u;
    for (char c : ext) {
        hash = (hash ^ static_cast<uint8_t>(tolower(c))) * 16777619u;
    }
    // Fixed high-contrast palettes (32 distinct, vibrant hues)
    static const char* const DYNAMIC_PALETTE[] = {
        "#ff4757", "#2ed573", "#1e90ff", "#ffa502", "#a55eea",
        "#3742fa", "#70a1ff", "#7bed9f", "#eccc68", "#ff7f50",
        "#26de81", "#4b7bec", "#fed330", "#a55eea", "#fc5c65",
        "#fd9644", "#45aaf2", "#2bcbba", "#a55eea", "#778ca3",
        "#eb3b5a", "#fa8231", "#20bf6b", "#0fb9b1", "#3867d6",
        "#8854d0", "#d1d8e0", "#e17055", "#00cec9", "#6c5ce7",
        "#fdcb6e", "#00b894"
    };
    return DYNAMIC_PALETTE[hash % 32];
}

} // namespace TreeMCP

