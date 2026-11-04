#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace TreeMCP {

constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;

// 48-byte memory-compact node structure for millions of items in minimal RAM
struct CompactNode {
    uint32_t name_offset;     // Offset in contiguous string pool
    uint32_t name_len;        // Length of UTF-8 filename
    uint64_t size;            // Uncompressed file size in bytes
    uint64_t allocated_size;  // On-disk cluster allocated size
    uint32_t parent_idx;      // Parent node index (INVALID_INDEX for root)
    uint32_t first_child;     // First child index (INVALID_INDEX if leaf/empty)
    uint32_t next_sibling;    // Next sibling index (INVALID_INDEX if last)
    uint32_t file_count;      // Subtree total file count
    uint32_t dir_count;       // Subtree total directory count
    uint32_t attributes;      // Win32 FILE_ATTRIBUTE_* flags
    uint16_t ext_id;          // Interned extension identifier
    uint16_t flags;           // Internal flags (e.g. 1 = is_directory, 2 = is_reparse)

    bool is_directory() const {
        return (flags & 1) != 0;
    }
};

struct DriveInfo {
    std::wstring path;
    std::wstring volume_name;
    std::wstring file_system;
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t available_bytes = 0;
    bool is_ntfs = false;
};

struct ScanStats {
    uint64_t total_files = 0;
    uint64_t total_dirs = 0;
    uint64_t total_bytes = 0;
    uint64_t total_allocated = 0;
    double duration_ms = 0.0;
    bool mft_used = false;
};

struct ExtStat {
    std::string ext;
    uint64_t count = 0;
    uint64_t total_size = 0;
    std::string color;
};

struct TreemapTile {
    uint32_t node_idx;
    std::string name;
    std::string full_path;
    uint64_t size;
    double x;
    double y;
    double width;
    double height;
    std::string color;
    bool is_dir;
    int depth;
};

} // namespace TreeMCP

