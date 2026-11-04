#pragma once

#include "file_tree.h"
#include <windows.h>
#include <winioctl.h>
#include <string>

namespace TreeMCP {

class MftReader {
public:
    MftReader();
    ~MftReader();

    // Check if process has administrative privileges to open raw volumes
    static bool has_admin_privileges();

    // Attempt direct MFT scan on drive (e.g. L"C:")
    bool scan_volume(const std::wstring& drive_letter, FileTree& tree, ScanStats& stats);

private:
    HANDLE h_vol_ = INVALID_HANDLE_VALUE;
};

} // namespace TreeMCP

