#include "token_optimizer.h"
#include <sstream>
#include <iomanip>

namespace TreeMCP {

std::string TokenOptimizer::format_drives(const std::vector<DriveInfo>& drives) {
    std::stringstream ss;
    ss << "DRIVES:";
    for (size_t i = 0; i < drives.size(); ++i) {
        const auto& d = drives[i];
        double total_gb = static_cast<double>(d.total_bytes) / (1ULL << 30);
        double used_gb = static_cast<double>(d.total_bytes - d.free_bytes) / (1ULL << 30);
        int pct = (d.total_bytes > 0) ? static_cast<int>((d.total_bytes - d.free_bytes) * 100 / d.total_bytes) : 0;

        char drive_ltr = (d.path.size() >= 2) ? static_cast<char>(d.path[0]) : '?';
        ss << " [" << drive_ltr << ": " << std::fixed << std::setprecision(1) << used_gb << "/"
           << total_gb << "GB (" << pct << "%) " << (d.is_ntfs ? "NTFS" : "FAT") << "]";
    }
    return ss.str();
}

std::string TokenOptimizer::format_hogs(const std::vector<FileTree::HogItem>& hogs, size_t limit) {
    if (hogs.empty()) return "No items found matching criteria.";

    std::stringstream ss;
    ss << "SIZE       TYPE  FILES  PATH\n";
    size_t count = 0;
    for (const auto& h : hogs) {
        if (count >= limit) break;
        ss << std::left << std::setw(10) << FileTree::format_size(h.size) << " "
           << std::setw(5) << (h.is_dir ? "DIR" : "FILE") << " "
           << std::setw(6) << h.file_count << " "
           << h.full_path << "\n";
        count++;
    }
    return ss.str();
}

std::string TokenOptimizer::format_tree(const FileTree& tree, uint32_t root_idx, int max_depth, uint64_t min_bytes) {
    if (root_idx == INVALID_INDEX) return "Tree is empty.";

    std::stringstream ss;
    const auto& root = tree.get_node(root_idx);
    ss << tree.get_name(root_idx) << " (" << FileTree::format_size(root.size) << ", "
       << root.file_count << " files)\n";

    auto print_level = [&](auto& self, uint32_t parent, int depth, const std::string& prefix) -> void {
        if (depth > max_depth) return;
        std::vector<uint32_t> children = tree.get_children(parent);
        
        uint64_t omitted_size = 0;
        uint32_t omitted_count = 0;

        for (size_t i = 0; i < children.size(); ++i) {
            uint32_t c = children[i];
            const auto& node = tree.get_node(c);
            if (node.size < min_bytes) {
                omitted_size += node.size;
                omitted_count++;
                continue;
            }

            bool is_last = (i + 1 == children.size() && omitted_count == 0);
            ss << prefix << (is_last ? "└── " : "├── ")
               << tree.get_name(c) << ": " << FileTree::format_size(node.size);
            if (root.size > 0) {
                int pct = static_cast<int>((node.size * 100) / root.size);
                ss << " (" << pct << "%)";
            }
            ss << "\n";

            if (node.is_directory() && depth < max_depth) {
                self(self, c, depth + 1, prefix + (is_last ? "    " : "│   "));
            }
        }

        if (omitted_count > 0) {
            ss << prefix << "└── [+" << omitted_count << " small items < "
               << FileTree::format_size(min_bytes) << ": " << FileTree::format_size(omitted_size) << "]\n";
        }
    };

    print_level(print_level, root_idx, 1, "");
    return ss.str();
}

std::string TokenOptimizer::format_extensions(const std::vector<ExtStat>& stats, uint64_t total_scanned_bytes) {
    if (stats.empty()) return "No extension statistics available.";

    std::stringstream ss;
    ss << "EXT         TOTAL     PCT   COUNT\n";
    for (const auto& es : stats) {
        int pct = (total_scanned_bytes > 0) ? static_cast<int>((es.total_size * 100) / total_scanned_bytes) : 0;
        ss << std::left << std::setw(10) << es.ext << " "
           << std::setw(9) << FileTree::format_size(es.total_size) << " "
           << std::setw(4) << pct << "% "
           << std::setw(7) << es.count << "\n";
    }
    return ss.str();
}

std::string TokenOptimizer::format_search(const std::vector<FileTree::SearchResult>& results) {
    if (results.empty()) return "No matching files or directories found.";

    std::stringstream ss;
    ss << "SIZE       TYPE  PATH\n";
    for (const auto& r : results) {
        ss << std::left << std::setw(10) << FileTree::format_size(r.size) << " "
           << std::setw(5) << (r.is_dir ? "DIR" : "FILE") << " "
           << r.full_path << "\n";
    }
    return ss.str();
}

size_t TokenOptimizer::estimate_tokens(const std::string& text) {
    if (text.empty()) return 0;
    // Standard approximation: ~4 characters per token
    return (text.size() + 3) / 4;
}

size_t TokenOptimizer::estimate_standard_json_tokens(size_t item_count) {
    // Typical verbose JSON representation is ~120 characters per item:
    // {"path":"...","size_bytes":...,"is_dir":...,"modified":"...","attribs":...}
    // 120 chars / 4 = ~30 tokens per item
    return item_count * 30;
}

} // namespace TreeMCP

