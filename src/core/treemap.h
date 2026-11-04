#pragma once

#include "file_tree.h"
#include "types.h"
#include <vector>

namespace TreeMCP {

class TreemapEngine {
public:
    TreemapEngine() = default;

    // Generate squarified treemap tiles for a given subtree
    static std::vector<TreemapTile> compute_layout(
        const FileTree& tree,
        uint32_t root_idx,
        double x, double y, double width, double height,
        int max_depth = 2,
        uint64_t min_size_threshold = 1024 * 1024 // 1MB threshold for tile creation
    );

    // Hit-test mouse coordinate to find tile
    static const TreemapTile* hit_test(const std::vector<TreemapTile>& tiles, double px, double py);

private:
    struct Item {
        uint32_t node_idx;
        uint64_t size;
        double area;
    };

    static void squarify(
        const FileTree& tree,
        std::vector<Item>& items,
        std::vector<Item>& current_row,
        double x, double y, double w, double h,
        int current_depth, int max_depth,
        uint64_t min_size_threshold,
        std::vector<TreemapTile>& out_tiles
    );

    static double worst_aspect_ratio(const std::vector<Item>& row, double side_length, double total_row_area);
    static void layout_row(
        const FileTree& tree,
        const std::vector<Item>& row,
        double x, double y, double w, double h,
        bool vertical,
        int current_depth, int max_depth,
        uint64_t min_size_threshold,
        std::vector<TreemapTile>& out_tiles
    );
};

} // namespace TreeMCP

