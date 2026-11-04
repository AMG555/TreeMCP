#include "fast_scanner.h"
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <windows.h>

namespace TreeMCP {

FastScanner::FastScanner() {}

std::string FastScanner::utf16_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    std::string str;
    str.resize(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::wstring FastScanner::utf8_to_utf16(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    if (size_needed <= 0) return std::wstring();
    std::wstring wstr;
    wstr.resize(size_needed);
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
                di.path = utf16_to_utf8(root_path);
                wchar_t vol_name[MAX_PATH + 1] = {0};
                wchar_t fs_name[MAX_PATH + 1] = {0};
                DWORD serial = 0, max_len = 0, flags = 0;
                if (GetVolumeInformationW(root_path.c_str(), vol_name, MAX_PATH, &serial,
                                          &max_len, &flags, fs_name, MAX_PATH)) {
                    di.volume_name = utf16_to_utf8(vol_name);
                    di.file_system = utf16_to_utf8(fs_name);
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

bool FastScanner::scan(const std::string& root_path, FileTree& tree, ScanStats& stats,
                       ProgressCallback progress_cb, std::atomic<bool>* cancel_flag) {
    tree.clear();
    stats = ScanStats();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::wstring wroot = utf8_to_utf16(root_path);
    while (wroot.size() > 3 && (wroot.back() == L'\\' || wroot.back() == L'/')) {
        wroot.pop_back();
    }

    std::string root_name = utf16_to_utf8(wroot);
    uint32_t root_idx = tree.add_node(root_name, 0, 0, INVALID_INDEX, true, FILE_ATTRIBUTE_DIRECTORY);
    tree.set_root_idx(root_idx);

    scan_recursive_win32(wroot, root_idx, tree, stats, progress_cb, cancel_flag);

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

void FastScanner::scan_recursive_win32(const std::wstring& dir_path, uint32_t parent_idx, FileTree& tree,
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
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) {
            continue;
        }

        bool is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        uint64_t file_size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        uint64_t alloc_size = is_directory ? 0 : ((file_size + 4095) / 4096) * 4096;

        uint32_t node_idx = tree.add_node_w(fd.cFileName, file_size, alloc_size, parent_idx, is_directory, fd.dwFileAttributes);

        if (is_directory) {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                std::wstring next_path = dir_path;
                if (next_path.back() != L'\\') next_path += L'\\';
                next_path += fd.cFileName;
                subdirs.push_back({next_path, node_idx});
            }
        }

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    for (const auto& sd : subdirs) {
        if (cancel_flag && cancel_flag->load()) break;
        scan_recursive_win32(sd.path, sd.node_idx, tree, stats, progress_cb, cancel_flag);
    }
}

} // namespace TreeMCP

#else
// Linux / POSIX Implementation
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

namespace TreeMCP {

FastScanner::FastScanner() {}

std::vector<DriveInfo> FastScanner::list_drives() {
    std::vector<DriveInfo> drives;

    // Parse /proc/mounts on Linux
    std::ifstream mounts("/proc/mounts");
    if (mounts.is_open()) {
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::string device, mount_point, fs_type;
            if (iss >> device >> mount_point >> fs_type) {
                // Focus on real local physical storage mounts
                if (device.find("/dev/") == 0 &&
                    mount_point.find("/boot") != 0 &&
                    mount_point.find("/snap") != 0 &&
                    mount_point.find("/var/lib") != 0) {
                    
                    struct statvfs s;
                    if (statvfs(mount_point.c_str(), &s) == 0) {
                        DriveInfo di;
                        di.path = mount_point;
                        di.volume_name = device;
                        di.file_system = fs_type;
                        di.total_bytes = (uint64_t)s.f_blocks * s.f_frsize;
                        di.free_bytes = (uint64_t)s.f_bfree * s.f_frsize;
                        di.available_bytes = (uint64_t)s.f_bavail * s.f_frsize;
                        di.is_ntfs = (fs_type == "ntfs" || fs_type == "ntfs3" || fs_type == "fuseblk");
                        drives.push_back(di);
                    }
                }
            }
        }
    }

    if (drives.empty()) {
        // Fallback root filesystem
        struct statvfs s;
        if (statvfs("/", &s) == 0) {
            DriveInfo di;
            di.path = "/";
            di.volume_name = "Root Filesystem";
            di.file_system = "ext4";
            di.total_bytes = (uint64_t)s.f_blocks * s.f_frsize;
            di.free_bytes = (uint64_t)s.f_bfree * s.f_frsize;
            di.available_bytes = (uint64_t)s.f_bavail * s.f_frsize;
            drives.push_back(di);
        }
    }

    return drives;
}

bool FastScanner::scan(const std::string& root_path, FileTree& tree, ScanStats& stats,
                       ProgressCallback progress_cb, std::atomic<bool>* cancel_flag) {
    tree.clear();
    stats = ScanStats();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::string clean_root = root_path;
    while (clean_root.size() > 1 && clean_root.back() == '/') {
        clean_root.pop_back();
    }

    uint32_t root_idx = tree.add_node(clean_root, 0, 0, INVALID_INDEX, true, 0040755);
    tree.set_root_idx(root_idx);

    scan_recursive_posix(clean_root, root_idx, tree, stats, progress_cb, cancel_flag);

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

void FastScanner::scan_recursive_posix(const std::string& dir_path, uint32_t parent_idx, FileTree& tree,
                                      ScanStats& stats, ProgressCallback progress_cb, std::atomic<bool>* cancel_flag) {
    if (cancel_flag && cancel_flag->load()) return;

    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return;

    struct PendingDir {
        std::string path;
        uint32_t node_idx;
    };
    std::vector<PendingDir> subdirs;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        std::string full_sub = dir_path;
        if (full_sub.back() != '/') full_sub += '/';
        full_sub += entry->d_name;

        struct stat st;
        if (lstat(full_sub.c_str(), &st) != 0) continue;

        // Skip symbolic links to avoid loops
        if (S_ISLNK(st.st_mode)) continue;

        bool is_dir = S_ISDIR(st.st_mode);
        uint64_t f_size = is_dir ? 0 : static_cast<uint64_t>(st.st_size);
        uint64_t alloc_size = is_dir ? 0 : (static_cast<uint64_t>(st.st_blocks) * 512);

        uint32_t node_idx = tree.add_node(entry->d_name, f_size, alloc_size, parent_idx, is_dir, st.st_mode);

        if (is_dir) {
            subdirs.push_back({full_sub, node_idx});
        }
    }

    closedir(dir);

    for (const auto& sd : subdirs) {
        if (cancel_flag && cancel_flag->load()) break;
        scan_recursive_posix(sd.path, sd.node_idx, tree, stats, progress_cb, cancel_flag);
    }
}

} // namespace TreeMCP
#endif
