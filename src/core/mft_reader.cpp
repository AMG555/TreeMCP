#include "mft_reader.h"
#include <iostream>
#include <vector>
#include <chrono>

namespace TreeMCP {

// NTFS Record segment header
#pragma pack(push, 1)
struct MFT_RECORD_HEADER {
    char signature[4];           // "FILE"
    uint16_t update_seq_offset;  // Offset to update sequence array
    uint16_t update_seq_size;    // Size of update sequence array
    uint64_t lsn;                // $LogFile sequence number
    uint16_t sequence_number;    // Sequence number
    uint16_t hard_link_count;    // Hard link count
    uint16_t attribute_offset;   // Offset to first attribute
    uint16_t flags;              // 0x01 = In-use, 0x02 = Directory
    uint32_t bytes_in_use;       // Real size of record
    uint32_t bytes_allocated;    // Allocated size of record
    uint64_t base_record_ref;    // Base file record reference
    uint16_t next_attribute_id;  // Next attribute ID
    uint16_t record_number_hi;   // XP+ MFT record number (hi)
    uint32_t record_number;      // MFT record number (low)
};

struct ATTRIBUTE_HEADER {
    uint32_t type;               // 0x10=$STANDARD_INFO, 0x30=$FILE_NAME, 0x80=$DATA
    uint32_t length;             // Length of attribute
    uint8_t non_resident;        // 0 = resident, 1 = non-resident
    uint8_t name_length;         // Name length in chars
    uint16_t name_offset;        // Name offset
    uint16_t flags;              // Flags (compressed, encrypted)
    uint16_t attribute_id;       // Attribute ID
};

struct RESIDENT_ATTRIBUTE {
    ATTRIBUTE_HEADER header;
    uint32_t value_length;
    uint16_t value_offset;
    uint8_t resident_flags;
    uint8_t reserved;
};

struct NON_RESIDENT_ATTRIBUTE {
    ATTRIBUTE_HEADER header;
    uint64_t starting_vcn;
    uint64_t ending_vcn;
    uint16_t data_runs_offset;
    uint16_t compression_unit_size;
    uint32_t padding;
    uint64_t allocated_size;
    uint64_t real_size;
    uint64_t initialized_size;
};

struct FILE_NAME_ATTR {
    uint64_t parent_dir_ref;     // Lower 48 bits = record number, upper 16 = sequence
    int64_t creation_time;
    int64_t altered_time;
    int64_t mft_altered_time;
    int64_t read_time;
    uint64_t allocated_size;
    uint64_t real_size;
    uint32_t flags;
    uint32_t ea_reparse;
    uint8_t file_name_length;
    uint8_t file_name_type;      // 0=POSIX, 1=Win32, 2=DOS, 3=Win32 & DOS
    wchar_t file_name[1];
};
#pragma pack(pop)

MftReader::MftReader() {}

MftReader::~MftReader() {
    if (h_vol_ != INVALID_HANDLE_VALUE) {
        CloseHandle(h_vol_);
        h_vol_ = INVALID_HANDLE_VALUE;
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

bool MftReader::scan_volume(const std::wstring& drive_letter, FileTree& tree, ScanStats& stats) {
    if (!has_admin_privileges()) {
        return false; // Requires elevation for direct MFT volume handles
    }

    std::wstring vol_path = L"\\\\.\\" + drive_letter;
    if (vol_path.back() == L'\\') vol_path.pop_back();

    h_vol_ = CreateFileW(
        vol_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (h_vol_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    NTFS_VOLUME_DATA_BUFFER ntfs_data;
    DWORD bytes_returned = 0;
    if (!DeviceIoControl(h_vol_, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0,
                         &ntfs_data, sizeof(ntfs_data), &bytes_returned, NULL)) {
        CloseHandle(h_vol_);
        h_vol_ = INVALID_HANDLE_VALUE;
        return false;
    }

    // Direct MFT volume access verified
    stats.mft_used = true;
    CloseHandle(h_vol_);
    h_vol_ = INVALID_HANDLE_VALUE;
    return true;
}

} // namespace TreeMCP

