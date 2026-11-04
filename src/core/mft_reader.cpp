#include "mft_reader.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>

namespace TreeMCP {

struct MftExtent {
    uint64_t lcn;
    uint64_t cluster_count;
};

struct ParsedMftItem {
    uint64_t frn;
    uint64_t parent_frn;
    uint64_t size;
    uint64_t alloc_size;
    bool is_dir;
    uint32_t attributes;
    std::wstring name;
};

MftReader::MftReader() {}

MftReader::~MftReader() {
    if (h_vol_ != INVALID_HANDLE_VALUE && h_vol_ != (void*)-1) {
        CloseHandle((HANDLE)h_vol_);
        h_vol_ = (void*)-1;
    }
}

bool MftReader::enable_privilege(const wchar_t* priv_name) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(NULL, priv_name, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(hToken);
    return (ok && err == ERROR_SUCCESS);
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

// Decode NTFS variable-length signed data run
static bool parse_data_runs(const uint8_t* run_ptr, size_t max_bytes, std::vector<MftExtent>& extents) {
    size_t offset = 0;
    int64_t prev_lcn = 0;

    while (offset < max_bytes && run_ptr[offset] != 0) {
        uint8_t b = run_ptr[offset++];
        uint8_t len_bytes = b & 0x0F;
        uint8_t offset_bytes = (b >> 4) & 0x0F;

        if (len_bytes == 0 || offset + len_bytes + offset_bytes > max_bytes) break;

        uint64_t run_len = 0;
        for (int i = 0; i < len_bytes; ++i) {
            run_len |= (static_cast<uint64_t>(run_ptr[offset++]) << (i * 8));
        }

        int64_t lcn_offset = 0;
        if (offset_bytes > 0) {
            for (int i = 0; i < offset_bytes; ++i) {
                lcn_offset |= (static_cast<int64_t>(run_ptr[offset++]) << (i * 8));
            }
            // Sign extend
            if (lcn_offset & (1ULL << (offset_bytes * 8 - 1))) {
                lcn_offset |= ~((1ULL << (offset_bytes * 8)) - 1);
            }
            prev_lcn += lcn_offset;
            extents.push_back({static_cast<uint64_t>(prev_lcn), run_len});
        }
    }
    return !extents.empty();
}

bool MftReader::scan_volume(const std::string& drive_path, FileTree& tree, ScanStats& stats) {
    if (!has_admin_privileges()) {
        return false;
    }

    enable_privilege(L"SeBackupPrivilege");
    enable_privilege(L"SeRestorePrivilege");

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

    NTFS_VOLUME_DATA_BUFFER ntfs_data{};
    DWORD bytes_returned = 0;
    if (!DeviceIoControl(h, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0,
                         &ntfs_data, sizeof(ntfs_data), &bytes_returned, NULL)) {
        CloseHandle(h);
        return false;
    }

    uint32_t cluster_size = ntfs_data.BytesPerCluster;
    uint32_t record_size = ntfs_data.BytesPerFileRecordSegment;
    if (record_size == 0 || cluster_size == 0) {
        CloseHandle(h);
        return false;
    }

    uint64_t mft_lcn = ntfs_data.MftStartLcn.QuadPart;

    // Read Record 0 to get all data runs of $MFT
    LARGE_INTEGER mft_pos;
    mft_pos.QuadPart = mft_lcn * cluster_size;
    if (!SetFilePointerEx(h, mft_pos, NULL, FILE_BEGIN)) {
        CloseHandle(h);
        return false;
    }

    std::vector<uint8_t> rec0(record_size);
    DWORD bytes_read = 0;
    if (!ReadFile(h, rec0.data(), record_size, &bytes_read, NULL) || bytes_read < record_size) {
        CloseHandle(h);
        return false;
    }

    if (std::memcmp(rec0.data(), "FILE", 4) != 0) {
        CloseHandle(h);
        return false;
    }

    // Parse attributes in Record 0 to find $DATA (0x80)
    std::vector<MftExtent> extents;
    uint16_t attr_offset = *reinterpret_cast<uint16_t*>(&rec0[20]);

    while (attr_offset + 8 <= record_size) {
        uint32_t attr_type = *reinterpret_cast<uint32_t*>(&rec0[attr_offset]);
        uint32_t attr_len = *reinterpret_cast<uint32_t*>(&rec0[attr_offset + 4]);
        if (attr_type == 0xFFFFFFFF || attr_len == 0 || attr_offset + attr_len > record_size) break;

        if (attr_type == 0x80) { // $DATA
            uint8_t non_res = rec0[attr_offset + 8];
            if (non_res == 1) {
                uint16_t run_offset = *reinterpret_cast<uint16_t*>(&rec0[attr_offset + 32]);
                if (attr_offset + run_offset < record_size) {
                    parse_data_runs(&rec0[attr_offset + run_offset], record_size - (attr_offset + run_offset), extents);
                }
            }
            break;
        }
        attr_offset += attr_len;
    }

    if (extents.empty()) {
        // Fallback: Use single extent from volume data
        uint64_t mft_clusters = (ntfs_data.MftValidDataLength.QuadPart + cluster_size - 1) / cluster_size;
        extents.push_back({mft_lcn, mft_clusters});
    }

    // Sequential MFT Chunk Streamer (reads in 2MB blocks)
    const size_t CHUNK_SIZE = 2 * 1024 * 1024; // 2 MB aligned buffer
    std::vector<uint8_t> chunk_buf(CHUNK_SIZE);
    std::vector<ParsedMftItem> items;
    items.reserve(524288);

    uint64_t current_rec_idx = 0;

    for (const auto& ext : extents) {
        uint64_t ext_bytes = ext.cluster_count * cluster_size;
        uint64_t ext_offset = ext.lcn * cluster_size;

        LARGE_INTEGER pos;
        pos.QuadPart = ext_offset;
        if (!SetFilePointerEx(h, pos, NULL, FILE_BEGIN)) continue;

        uint64_t bytes_left = ext_bytes;
        while (bytes_left > 0) {
            DWORD to_read = static_cast<DWORD>(std::min<uint64_t>(bytes_left, CHUNK_SIZE));
            DWORD read_now = 0;
            if (!ReadFile(h, chunk_buf.data(), to_read, &read_now, NULL) || read_now == 0) break;

            size_t recs_in_chunk = read_now / record_size;
            for (size_t r = 0; r < recs_in_chunk; ++r, ++current_rec_idx) {
                uint8_t* rec = &chunk_buf[r * record_size];
                if (std::memcmp(rec, "FILE", 4) != 0) continue;

                uint16_t flags = *reinterpret_cast<uint16_t*>(rec + 22);
                if ((flags & 0x01) == 0) continue; // Deleted record

                bool is_dir = (flags & 0x02) != 0;

                // Apply update sequence fixups
                uint16_t usa_offset = *reinterpret_cast<uint16_t*>(rec + 4);
                uint16_t usa_count = *reinterpret_cast<uint16_t*>(rec + 6);
                if (usa_offset + (usa_count * 2) <= record_size && usa_count >= 3) {
                    uint16_t* usa = reinterpret_cast<uint16_t*>(rec + usa_offset);
                    uint16_t* sec1 = reinterpret_cast<uint16_t*>(rec + 510);
                    uint16_t* sec2 = reinterpret_cast<uint16_t*>(rec + 1022);
                    if (*sec1 == usa[0]) *sec1 = usa[1];
                    if (*sec2 == usa[0]) *sec2 = usa[2];
                }

                // Parse Attributes
                uint16_t a_off = *reinterpret_cast<uint16_t*>(rec + 20);
                uint64_t real_size = 0;
                uint64_t alloc_size = 0;
                uint64_t parent_frn = 0;
                uint32_t file_attrs = 0;
                std::wstring best_name;
                uint8_t best_ns = 255;

                while (a_off + 8 <= record_size) {
                    uint32_t atype = *reinterpret_cast<uint32_t*>(rec + a_off);
                    uint32_t alen = *reinterpret_cast<uint32_t*>(rec + a_off + 4);
                    if (atype == 0xFFFFFFFF || alen == 0 || a_off + alen > record_size) break;

                    if (atype == 0x30) { // $FILE_NAME
                        uint8_t non_res = rec[a_off + 8];
                        if (non_res == 0) {
                            uint16_t c_off = *reinterpret_cast<uint16_t*>(rec + a_off + 20);
                            if (a_off + c_off + 66 <= record_size) {
                                uint8_t* fnd = rec + a_off + c_off;
                                uint64_t pfrn = *reinterpret_cast<uint64_t*>(fnd) & 0x0000FFFFFFFFFFFFULL;
                                uint8_t nlen = fnd[64];
                                uint8_t ns = fnd[65];
                                uint32_t fa = *reinterpret_cast<uint32_t*>(fnd + 56);

                                if (a_off + c_off + 66 + (nlen * 2) <= record_size) {
                                    // Prefer Win32 / POSIX names over DOS 8.3 names
                                    if (best_name.empty() || ns == 1 || ns == 3 || (best_ns == 2 && ns != 2)) {
                                        best_name.assign(reinterpret_cast<wchar_t*>(fnd + 66), nlen);
                                        parent_frn = pfrn;
                                        best_ns = ns;
                                        file_attrs = fa;
                                    }
                                }
                            }
                        }
                    } else if (atype == 0x80) { // $DATA
                        uint8_t name_len = rec[a_off + 9];
                        if (name_len == 0) { // Default unnamed data stream
                            uint8_t non_res = rec[a_off + 8];
                            if (non_res == 1 && a_off + 56 <= record_size) {
                                alloc_size = *reinterpret_cast<uint64_t*>(rec + a_off + 40);
                                real_size = *reinterpret_cast<uint64_t*>(rec + a_off + 48);
                            } else if (non_res == 0 && a_off + 24 <= record_size) {
                                real_size = *reinterpret_cast<uint32_t*>(rec + a_off + 16);
                                alloc_size = (real_size + 511) & ~511;
                            }
                        }
                    }
                    a_off += alen;
                }

                if (!best_name.empty() && current_rec_idx != 5) { // Skip record 5 (root itself)
                    items.push_back({
                        current_rec_idx,
                        parent_frn,
                        real_size,
                        alloc_size,
                        is_dir,
                        file_attrs,
                        std::move(best_name)
                    });
                }
            }

            bytes_left -= read_now;
        }
    }

    CloseHandle(h);
    h_vol_ = (void*)-1;

    if (items.empty()) {
        return false;
    }

    // Build the FileTree hierarchy from flat parsed MFT items
    tree.clear();
    std::string root_label = drive_path;
    while (!root_label.empty() && (root_label.back() == '\\' || root_label.back() == '/')) root_label.pop_back();
    root_label += "\\";

    uint32_t root_node_idx = tree.add_node(root_label, 0, 0, INVALID_INDEX, true, FILE_ATTRIBUTE_DIRECTORY);
    tree.set_root_idx(root_node_idx);

    std::unordered_map<uint64_t, uint32_t> frn_to_node;
    frn_to_node.reserve(items.size() * 2);
    frn_to_node[5] = root_node_idx; // Root folder FRN is 5

    // First pass: add directory nodes
    for (const auto& it : items) {
        if (it.is_dir) {
            uint32_t n_idx = tree.add_node_w(
                it.name.c_str(), 0, 0, INVALID_INDEX, true, it.attributes
            );
            frn_to_node[it.frn] = n_idx;
        }
    }

    // Second pass: link directories to parents and add files
    for (const auto& it : items) {
        uint32_t p_idx = root_node_idx;
        auto p_it = frn_to_node.find(it.parent_frn);
        if (p_it != frn_to_node.end()) {
            p_idx = p_it->second;
        }

        if (it.is_dir) {
            uint32_t self_idx = frn_to_node[it.frn];
            if (self_idx != INVALID_INDEX && self_idx != root_node_idx) {
                // Link directory child
                auto& self_node = tree.get_node_mut(self_idx);
                self_node.parent_idx = p_idx;

                auto& parent_node = tree.get_node_mut(p_idx);
                self_node.next_sibling = parent_node.first_child;
                parent_node.first_child = self_idx;
            }
        } else {
            // Leaf file
            tree.add_node_w(
                it.name.c_str(), it.size, it.alloc_size, p_idx, false, it.attributes
            );
        }
    }

    tree.aggregate_sizes();
    tree.sort_children_by_size();

    stats.mft_used = true;
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

bool MftReader::enable_privilege(const wchar_t* priv_name) {
    (void)priv_name;
    return false;
}

bool MftReader::scan_volume(const std::string& drive_path, FileTree& tree, ScanStats& stats) {
    (void)drive_path;
    (void)tree;
    (void)stats;
    return false;
}

} // namespace TreeMCP
#endif
