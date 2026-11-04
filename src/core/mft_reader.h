#pragma once

#include "file_tree.h"
#include <string>
#include <vector>

namespace TreeMCP {

class MftReader {
public:
    MftReader();
    ~MftReader();

    // Check if current process has administrative privileges
    static bool has_admin_privileges();

    // Enable privilege (e.g. SE_BACKUP_NAME)
    static bool enable_privilege(const wchar_t* priv_name);

    // Attempt direct high-speed MFT scan on NTFS volume
    bool scan_volume(const std::string& drive_path, FileTree& tree, ScanStats& stats);

private:
#ifdef _WIN32
    void* h_vol_ = (void*)-1;
#endif
};

} // namespace TreeMCP
