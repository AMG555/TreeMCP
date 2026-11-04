#pragma once

#include "file_tree.h"
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace TreeMCP {

class FastScanner {
public:
    FastScanner();
    ~FastScanner() = default;

    static std::vector<DriveInfo> list_drives();

    // Progress callback: (current_path, files_count, dirs_count, bytes_count)
    using ProgressCallback = std::function<void(const std::string&, uint64_t, uint64_t, uint64_t)>;

    bool scan(const std::wstring& root_path, FileTree& tree, ScanStats& stats,
              ProgressCallback progress_cb = nullptr, std::atomic<bool>* cancel_flag = nullptr);

private:
    void scan_recursive(const std::wstring& dir_path, uint32_t parent_idx, FileTree& tree,
                        ScanStats& stats, ProgressCallback progress_cb, std::atomic<bool>* cancel_flag);

    static std::string utf16_to_utf8(const std::wstring& wstr);
    static std::wstring utf8_to_utf16(const std::string& str);
};

} // namespace TreeMCP

