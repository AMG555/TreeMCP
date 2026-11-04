#pragma once

#include "file_tree.h"
#include <string>

namespace TreeMCP {

class MftReader {
public:
    MftReader();
    ~MftReader();

    // Check if process has administrative / root privileges
    static bool has_admin_privileges();

    // Attempt direct MFT scan on drive (Windows only)
    bool scan_volume(const std::string& drive_path, FileTree& tree, ScanStats& stats);

private:
#ifdef _WIN32
    void* h_vol_ = (void*)-1;
#endif
};

} // namespace TreeMCP
