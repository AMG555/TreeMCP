#include "fast_scanner.h"
#include <chrono>
#include <iostream>

namespace TreeMCP {

FastScanner::FastScanner() {}

std::string FastScanner::utf16_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::wstring FastScanner::utf8_to_utf16(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::vector<DriveInfo> FastScanner::list_drives() {
    std::vector<DriveInfo> drives;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (mask & (1 << i)) {
            wchar_t letter = L'A' + i;
            std::wstring root_path = std::wstring(1, letter) + L":\\";
            UINT type = GetDriveTypeW(root_path.c_str());
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                DriveInfo di;
                di.path = root_path;
                wchar_t vol_name[MAX_PATH + 1] = {0};
                wchar_t fs_name[MAX_PATH + 1] = {0};
                DWORD serial = 0, max_len = 0, flags = 0;
                if (GetVolumeInformationW(root_path.c_str(), vol_name, MAX_PATH, &serial,
                                          &max_len, &flags, fs_name, MAX_PATH)) {
                    di.volume_name = vol_name;
                    di.file_system = fs_name;
                    di.is_ntfs = (_wcsicmp(fs_name, L"NTFS") == 0);
                }

                ULARGE_INTEGER free_bytes_avail, total_num_bytes, total_num_free_bytes;
                if (GetDiskFreeSpaceExW(root_path.c_str(), &free_bytes_avail, &total_num_bytes, &total_num_free_bytes)) {
                    di.total_bytes = total_num_bytes.QuadPart;
                    di.free_bytes = total_num_free_bytes.QuadPart;
                    di.available_bytes = free_bytes_avail.QuadPart;
                }
                drives.push_back(di);
            }
        }
    }
    return drives;
}

bool FastScanner::scan(const std::wstring& root_path, FileTree& tree, ScanStats& stats,
                       ProgressCallback progress_cb, std::atomic<bool>* cancel_flag) {
    tree.clear();
    stats = ScanStats();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::wstring clean_root = root_path;
    while (clean_root.size() > 3 && (clean_root.back() == L'\\' || clean_root.back() == L'/')) {
        clean_root.pop_back();
    }

    std::string root_name = utf16_to_utf8(clean_root);
    uint32_t root_idx = tree.add_node(root_name, 0, 0, INVALID_INDEX, true, FILE_ATTRIBUTE_DIRECTORY);
    tree.set_root_idx(root_idx);

    scan_recursive(clean_root, root_idx, tree, stats, progress_cb, cancel_flag);

    // Compute rollup sizes bottom-up
    tree.aggregate_sizes();
    tree.sort_children_by_size();

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    stats.total_files = tree.get_node(root_idx).file_count;
    stats.total_dirs = tree.get_node(root_idx).dir_count;
    stats.total_bytes = tree.get_node(root_idx).size;
    stats.total_allocated = tree.get_node(root_idx).allocated_size;

    return (cancel_flag && cancel_flag->load()) ? false : true;
}

void FastScanner::scan_recursive(const std::wstring& dir_path, uint32_t parent_idx, FileTree& tree,
                                ScanStats& stats, ProgressCallback progress_cb, std::atomic<bool>* cancel_flag) {
    if (cancel_flag && cancel_flag->load()) return;

    std::wstring search_pattern = dir_path;
    if (search_pattern.back() != L'\\') search_pattern += L'\\';
    search_pattern += L'*';

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(
        search_pattern.c_str(),
        FindExInfoBasic,
        &fd,
        FindExSearchNameMatch,
        NULL,
        FIND_FIRST_EX_LARGE_FETCH
    );

    if (hFind == INVALID_HANDLE_VALUE) return;

    struct PendingDir {
        std::wstring path;
        uint32_t node_idx;
    };
    std::vector<PendingDir> subdirs;

    do {
        // Skip . and ..
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
            continue;
        }

        bool is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        uint64_t file_size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        // Approximation of cluster allocation (4096-byte boundary)
        uint64_t alloc_size = is_directory ? 0 : ((file_size + 4095) / 4096) * 4096;

        std::string name_utf8 = utf16_to_utf8(fd.cFileName);
        uint32_t node_idx = tree.add_node(name_utf8, file_size, alloc_size, parent_idx, is_directory, fd.dwFileAttributes);

        if (is_directory) {
            // Avoid traversing reparse points (symlinks, junction loops)
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                std::wstring next_path = dir_path;
                if (next_path.back() != L'\\') next_path += L'\\';
                next_path += fd.cFileName;
                subdirs.push_back({next_path, node_idx});
            }
        }

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    // Recurse into subdirectories
    for (const auto& sd : subdirs) {
        if (cancel_flag && cancel_flag->load()) break;
        scan_recursive(sd.path, sd.node_idx, tree, stats, progress_cb, cancel_flag);
    }
}

} // namespace TreeMCP

