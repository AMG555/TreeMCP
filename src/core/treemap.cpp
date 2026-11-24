#include "treemap.h"
#include <cmath>
#include <algorithm>

namespace TreeMCP {

std::vector<TreemapTile> TreemapEngine::compute_layout(
    const FileTree& tree,
    uint32_t root_idx,
    double x, double y, double width, double height,
    int max_depth,
    uint64_t min_size_threshold
) {
    std::vector<TreemapTile> tiles;
    if (root_idx == INVALID_INDEX || width <= 4.0 || height <= 4.0) return tiles;

    const auto& root_node = tree.get_node(root_idx);
    if (root_node.size == 0) return tiles;

    std::vector<uint32_t> children = tree.get_children(root_idx);
    if (children.empty()) {
        // Single root tile
        TreemapTile t;
        t.node_idx = root_idx;
        t.name = tree.get_name(root_idx);
        t.full_path = tree.get_full_path(root_idx);
        t.size = root_node.size;
        t.x = x; t.y = y; t.width = width; t.height = height;
        t.extension = root_node.is_directory() ? "" : tree.get_extension(root_idx);
        t.color = root_node.is_directory() ? "#546e7a" : FileTree::get_color_for_ext(t.extension);
        t.is_dir = root_node.is_directory();
        t.depth = 0;
        tiles.push_back(t);
        return tiles;
    }

    std::vector<Item> items;
    items.reserve(children.size() + 1);

    uint64_t small_files_size = 0;
    uint32_t small_files_count = 0;

    for (uint32_t c : children) {
        const auto& cnode = tree.get_node(c);
        if (cnode.size == 0) continue;

        if (cnode.is_directory()) {
            if (cnode.size >= min_size_threshold) {
                Item it;
                it.node_idx = c;
                it.size = cnode.size;
                it.area = 0.0;
                it.is_aggregate = false;
                items.push_back(it);
            } else {
                small_files_size += cnode.size;
                small_files_count++;
            }
        } else {
            if (cnode.size >= min_size_threshold) {
                Item it;
                it.node_idx = c;
                it.size = cnode.size;
                it.area = 0.0;
                it.is_aggregate = false;
                items.push_back(it);
            } else {
                small_files_size += cnode.size;
                small_files_count++;
            }
        }
    }

    // Aggregate small loose items into a single composite tile [N Files in Folder]
    if (small_files_size > 0) {
        Item agg;
        agg.node_idx = root_idx;
        agg.size = small_files_size;
        agg.area = 0.0;
        agg.is_aggregate = true;
        agg.small_count = small_files_count;
        items.push_back(agg);
    }

    if (items.empty()) return tiles;

    // Sort descending by size for optimal squarified aspect ratios
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.size > b.size;
    });

    // Cap items per level to prevent subpixel rendering thrashing
    if (items.size() > 1000) {
        items.resize(1000);
    }

    uint64_t items_total_size = 0;
    for (const auto& it : items) {
        items_total_size += it.size;
    }
    if (items_total_size == 0) return tiles;

    double total_area = width * height;
    for (auto& it : items) {
        it.area = (static_cast<double>(it.size) / items_total_size) * total_area;
    }

    std::vector<Item> row;
    squarify(tree, items, row, x, y, width, height, 1, max_depth, min_size_threshold, tiles);
    return tiles;
}

double TreemapEngine::worst_aspect_ratio(const std::vector<Item>& row, double side_length, double total_row_area) {
    if (row.empty() || side_length <= 0.0001 || total_row_area <= 0.0001) return 1e9;
    double row_thickness = total_row_area / side_length;
    double max_aspect = 0.0;

    for (const auto& item : row) {
        double item_len = item.area / row_thickness;
        double aspect = std::max(row_thickness / item_len, item_len / row_thickness);
        if (aspect > max_aspect) max_aspect = aspect;
    }
    return max_aspect;
}

void TreemapEngine::layout_row(
    const FileTree& tree,
    const std::vector<Item>& row,
    double x, double y, double w, double h,
    bool vertical,
    int current_depth, int max_depth,
    uint64_t min_size_threshold,
    std::vector<TreemapTile>& out_tiles
) {
    double total_row_area = 0.0;
    for (const auto& it : row) total_row_area += it.area;
    if (total_row_area <= 0.0) return;

    double side = vertical ? h : w;
    double thickness = total_row_area / side;

    double curr_pos = 0.0;
    for (const auto& it : row) {
        double item_len = (it.area / total_row_area) * side;
        double item_x = vertical ? x : (x + curr_pos);
        double item_y = vertical ? (y + curr_pos) : y;
        double item_w = vertical ? thickness : item_len;
        double item_h = vertical ? item_len : thickness;

        TreemapTile tile;
        tile.x = item_x;
        tile.y = item_y;
        tile.width = item_w;
        tile.height = item_h;
        tile.depth = current_depth;

        if (it.is_aggregate) {
            std::string parent_name = (it.node_idx != INVALID_INDEX) ? tree.get_name(it.node_idx) : "Folder";
            tile.node_idx = it.node_idx;
            tile.name = "[" + std::to_string(it.small_count) + " Files in " + parent_name + "]";
            tile.full_path = (it.node_idx != INVALID_INDEX) ? tree.get_full_path(it.node_idx) : "";
            tile.size = it.size;
            tile.is_dir = false;
            tile.extension = "[no extension]";
            tile.color = "#546e7a"; // Composite items slate
            out_tiles.push_back(tile);
        } else {
            const auto& node = tree.get_node(it.node_idx);
            tile.node_idx = it.node_idx;
            tile.name = tree.get_name(it.node_idx);
            tile.full_path = tree.get_full_path(it.node_idx);
            tile.size = node.size;
            tile.is_dir = node.is_directory();
            tile.extension = tile.is_dir ? "" : tree.get_extension(it.node_idx);
            tile.color = tile.is_dir ? "#2c3e50" : FileTree::get_color_for_ext(tile.extension);

            bool has_sub = false;
            // Recurse into subdirectories only if box is large enough to display meaningful children
            if (tile.is_dir && current_depth < max_depth && item_w >= 14.0 && item_h >= 14.0) {
                auto sub_tiles = compute_layout(
                    tree, it.node_idx,
                    item_x, item_y,
                    item_w, item_h,
                    max_depth, min_size_threshold
                );
                if (!sub_tiles.empty()) {
                    has_sub = true;
                    out_tiles.insert(out_tiles.end(), sub_tiles.begin(), sub_tiles.end());
                }
            }

            // If a directory had no sub-tiles (e.g. depth limit or tiny children),
            // treat it as a leaf tile so it gets a cushion and no area is blank!
            if (tile.is_dir && !has_sub) {
                tile.is_dir = false;
                tile.color = "#546e7a";
            }

            out_tiles.push_back(tile);
        }

        curr_pos += item_len;
    }
}

void TreemapEngine::squarify(
    const FileTree& tree,
    std::vector<Item>& items,
    std::vector<Item>& current_row,
    double x, double y, double w, double h,
    int current_depth, int max_depth,
    uint64_t min_size_threshold,
    std::vector<TreemapTile>& out_tiles
) {
    if (items.empty()) {
        if (!current_row.empty()) {
            bool vertical = (w >= h);
            layout_row(tree, current_row, x, y, w, h, vertical, current_depth, max_depth, min_size_threshold, out_tiles);
        }
        return;
    }

    Item next_item = items.front();
    bool vertical = (w >= h);
    double side = vertical ? h : w;

    double curr_row_area = 0.0;
    for (const auto& it : current_row) curr_row_area += it.area;

    double ar_current = worst_aspect_ratio(current_row, side, curr_row_area);

    std::vector<Item> candidate_row = current_row;
    candidate_row.push_back(next_item);
    double candidate_row_area = curr_row_area + next_item.area;
    double ar_candidate = worst_aspect_ratio(candidate_row, side, candidate_row_area);

    if (current_row.empty() || ar_candidate <= ar_current) {
        items.erase(items.begin());
        current_row.push_back(next_item);
        squarify(tree, items, current_row, x, y, w, h, current_depth, max_depth, min_size_threshold, out_tiles);
    } else {
        // Lay out current row and advance bounding box
        layout_row(tree, current_row, x, y, w, h, vertical, current_depth, max_depth, min_size_threshold, out_tiles);

        double thickness = curr_row_area / side;
        double new_x = vertical ? (x + thickness) : x;
        double new_y = vertical ? y : (y + thickness);
        double new_w = vertical ? (w - thickness) : w;
        double new_h = vertical ? h : (h - thickness);

        current_row.clear();
        squarify(tree, items, current_row, new_x, new_y, new_w, new_h, current_depth, max_depth, min_size_threshold, out_tiles);
    }
}

const TreemapTile* TreemapEngine::hit_test(const std::vector<TreemapTile>& tiles, double px, double py) {
    const TreemapTile* best = nullptr;
    int max_depth = -1;
    for (const auto& tile : tiles) {
        if (px >= tile.x && px <= (tile.x + tile.width) &&
            py >= tile.y && py <= (tile.y + tile.height)) {
            if (tile.depth >= max_depth) {
                max_depth = tile.depth;
                best = &tile;
            }
        }
    }
    return best;
}

} // namespace TreeMCP
