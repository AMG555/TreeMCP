#include "mft_reader.h"
#include <iostream>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>

namespace TreeMCP {

MftReader::MftReader() {}

MftReader::~MftReader() {
    if (h_vol_ != INVALID_HANDLE_VALUE && h_vol_ != (void*)-1) {
        CloseHandle((HANDLE)h_vol_);
        h_vol_ = (void*)-1;
    }
}

bool MftReader::has_admin_privileges() {
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt_auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin != FALSE;
}

bool MftReader::scan_volume(const std::string& drive_path, FileTree& tree, ScanStats& stats) {
    if (!has_admin_privileges()) {
        return false;
    }

    std::string vol = "\\\\.\\" + drive_path;
    while (!vol.empty() && (vol.back() == '\\' || vol.back() == '/')) vol.pop_back();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, vol.c_str(), -1, NULL, 0);
    if (wlen <= 0) return false;
    std::wstring wvol;
    wvol.resize(wlen);
    MultiByteToWideChar(CP_UTF8, 0, vol.c_str(), -1, &wvol[0], wlen);

    HANDLE h = CreateFileW(
        wvol.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    NTFS_VOLUME_DATA_BUFFER ntfs_data;
    DWORD bytes_returned = 0;
    if (!DeviceIoControl(h, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0,
                         &ntfs_data, sizeof(ntfs_data), &bytes_returned, NULL)) {
        CloseHandle(h);
        return false;
    }

    stats.mft_used = true;
    CloseHandle(h);
    return true;
}

} // namespace TreeMCP

#else
// Linux / POSIX Implementation
#include <unistd.h>

namespace TreeMCP {

MftReader::MftReader() {}
MftReader::~MftReader() {}

bool MftReader::has_admin_privileges() {
    return geteuid() == 0;
}

bool MftReader::scan_volume(const std::string& drive_path, FileTree& tree, ScanStats& stats) {
    (void)drive_path;
    (void)tree;
    (void)stats;
    // Linux uses native kernel POSIX crawler (statx / getdents)
    return false;
}

} // namespace TreeMCP
#endif
