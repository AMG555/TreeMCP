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
    uint64_t vcn;
    uint64_t lcn;
    uint64_t cluster_count;
    bool is_sparse;
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

static void apply_record_fixups(uint8_t* rec, size_t record_size) {
    if (record_size < 1024) return;
    uint16_t usa_offset = *reinterpret_cast<uint16_t*>(rec + 4);
    uint16_t usa_count = *reinterpret_cast<uint16_t*>(rec + 6);
    if (usa_count < 2 || usa_offset + (usa_count * 2) > record_size) return;

    uint16_t* usa = reinterpret_cast<uint16_t*>(rec + usa_offset);
    uint16_t usn = usa[0];

    for (size_t s = 1; s < usa_count && (s * 512) <= record_size; ++s) {
        uint16_t* sector_end = reinterpret_cast<uint16_t*>(rec + (s * 512) - 2);
        if (*sector_end == usn) {
            *sector_end = usa[s];
        }
    }
}

// Decode NTFS variable-length signed data run
static bool parse_data_runs(const uint8_t* run_ptr, size_t max_bytes, uint64_t start_vcn, std::vector<MftExtent>& extents) {
    size_t offset = 0;
    int64_t prev_lcn = 0;
    uint64_t curr_vcn = start_vcn;

    while (offset < max_bytes && run_ptr[offset] != 0) {
        uint8_t b = run_ptr[offset++];
        uint8_t len_bytes = b & 0x0F;
        uint8_t offset_bytes = (b >> 4) & 0x0F;

        if (len_bytes == 0 || offset + len_bytes + offset_bytes > max_bytes) break;

        uint64_t run_len = 0;
        for (int i = 0; i < len_bytes; ++i) {
            run_len |= (static_cast<uint64_t>(run_ptr[offset++]) << (i * 8));
        }

        if (offset_bytes == 0) {
            extents.push_back({curr_vcn, 0, run_len, true});
        } else {
            int64_t lcn_offset = 0;
            for (int i = 0; i < offset_bytes; ++i) {
                lcn_offset |= (static_cast<int64_t>(run_ptr[offset++]) << (i * 8));
            }
            // Sign extend
            if (lcn_offset & (1ULL << (offset_bytes * 8 - 1))) {
                lcn_offset |= ~((1ULL << (offset_bytes * 8)) - 1);
            }
            prev_lcn += lcn_offset;
            extents.push_back({curr_vcn, static_cast<uint64_t>(prev_lcn), run_len, false});
        }
        curr_vcn += run_len;
    }
    return !extents.empty();
}

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

    apply_record_fixups(rec0.data(), record_size);

    std::vector<MftExtent> extents;
    std::vector<uint64_t> mft_extension_records;

    auto parse_record_data_runs = [&](const uint8_t* rec_buf, size_t rec_len, bool check_attr_list) {
        if (rec_len < 48 || std::memcmp(rec_buf, "FILE", 4) != 0) return;
        uint16_t a_off = *reinterpret_cast<const uint16_t*>(rec_buf + 20);

        while (a_off + 8 <= rec_len) {
            uint32_t atype = *reinterpret_cast<const uint32_t*>(rec_buf + a_off);
            uint32_t alen = *reinterpret_cast<const uint32_t*>(rec_buf + a_off + 4);
            if (atype == 0xFFFFFFFF || alen == 0 || a_off + alen > rec_len) break;

            if (atype == 0x80) { // $DATA attribute
                uint8_t non_res = rec_buf[a_off + 8];
                if (non_res == 1 && a_off + 34 <= rec_len) {
                    uint64_t start_vcn = *reinterpret_cast<const uint64_t*>(rec_buf + a_off + 16);
                    uint16_t run_offset = *reinterpret_cast<const uint16_t*>(rec_buf + a_off + 32);
                    if (a_off + run_offset < rec_len) {
                        parse_data_runs(rec_buf + a_off + run_offset, rec_len - (a_off + run_offset), start_vcn, extents);
                    }
                }
            } else if (atype == 0x20 && check_attr_list) { // $ATTRIBUTE_LIST attribute
                uint8_t non_res = rec_buf[a_off + 8];
                if (non_res == 0 && a_off + 24 <= rec_len) { // Resident attribute list
                    uint32_t val_len = *reinterpret_cast<const uint32_t*>(rec_buf + a_off + 16);
                    uint16_t val_offset = *reinterpret_cast<const uint16_t*>(rec_buf + a_off + 20);
                    if (a_off + val_offset + val_len <= rec_len) {
                        const uint8_t* al_ptr = rec_buf + a_off + val_offset;
                        size_t al_pos = 0;
                        while (al_pos + 26 <= val_len) {
                            uint32_t entry_type = *reinterpret_cast<const uint32_t*>(al_ptr + al_pos);
                            uint16_t entry_len = *reinterpret_cast<const uint16_t*>(al_ptr + al_pos + 4);
                            if (entry_len == 0 || al_pos + entry_len > val_len) break;

                            if (entry_type == 0x80) { // $DATA entry
                                uint64_t mft_ref = *reinterpret_cast<const uint64_t*>(al_ptr + al_pos + 24) & 0x0000FFFFFFFFFFFFULL;
                                if (mft_ref != 0 && std::find(mft_extension_records.begin(), mft_extension_records.end(), mft_ref) == mft_extension_records.end()) {
                                    mft_extension_records.push_back(mft_ref);
                                }
                            }
                            al_pos += entry_len;
                        }
                    }
                }
            }
            a_off += alen;
        }
    };

    parse_record_data_runs(rec0.data(), record_size, true);

    // If Record 0 has extension records via $ATTRIBUTE_LIST, read them from Extent 0
    if (!extents.empty() && !mft_extension_records.empty()) {
        uint64_t ext0_offset = extents[0].lcn * cluster_size;
        std::vector<uint8_t> ext_rec(record_size);

        for (uint64_t ext_frn : mft_extension_records) {
            LARGE_INTEGER pos;
            pos.QuadPart = ext0_offset + (ext_frn * record_size);
            if (SetFilePointerEx(h, pos, NULL, FILE_BEGIN)) {
                DWORD rd = 0;
                if (ReadFile(h, ext_rec.data(), record_size, &rd, NULL) && rd == record_size) {
                    apply_record_fixups(ext_rec.data(), record_size);
                    parse_record_data_runs(ext_rec.data(), record_size, false);
                }
            }
        }
    }

    if (extents.empty()) {
        uint64_t mft_clusters = (ntfs_data.MftValidDataLength.QuadPart + cluster_size - 1) / cluster_size;
        extents.push_back({0, mft_lcn, mft_clusters, false});
    }

    std::sort(extents.begin(), extents.end(), [](const MftExtent& a, const MftExtent& b) {
        return a.vcn < b.vcn;
    });

    const size_t CHUNK_SIZE = 2 * 1024 * 1024; // 2 MB aligned buffer
    std::vector<uint8_t> chunk_buf(CHUNK_SIZE);
    std::vector<ParsedMftItem> items;
    items.reserve(1500000);

    uint32_t recs_per_cluster = cluster_size / record_size;

    for (const auto& ext : extents) {
        if (ext.is_sparse) continue;
        uint64_t ext_bytes = ext.cluster_count * cluster_size;
        uint64_t ext_offset = ext.lcn * cluster_size;
        uint64_t base_rec_idx = ext.vcn * recs_per_cluster;

        LARGE_INTEGER pos;
        pos.QuadPart = ext_offset;
        if (!SetFilePointerEx(h, pos, NULL, FILE_BEGIN)) continue;

        uint64_t bytes_left = ext_bytes;
        uint64_t bytes_read_in_extent = 0;

        while (bytes_left > 0) {
            DWORD to_read = static_cast<DWORD>(std::min<uint64_t>(bytes_left, CHUNK_SIZE));
            DWORD read_now = 0;
            if (!ReadFile(h, chunk_buf.data(), to_read, &read_now, NULL) || read_now == 0) break;

            size_t recs_in_chunk = read_now / record_size;
            uint64_t chunk_start_rec = base_rec_idx + (bytes_read_in_extent / record_size);

            for (size_t r = 0; r < recs_in_chunk; ++r) {
                uint8_t* rec = &chunk_buf[r * record_size];
                if (std::memcmp(rec, "FILE", 4) != 0) continue;

                uint16_t flags = *reinterpret_cast<uint16_t*>(rec + 22);
                if ((flags & 0x01) == 0) continue; // Deleted record

                uint64_t base_mft_ref = *reinterpret_cast<uint64_t*>(rec + 32) & 0x0000FFFFFFFFFFFFULL;
                if (base_mft_ref != 0) {
                    continue; // Skip secondary extension record to avoid detached duplicate
                }

                bool is_dir = (flags & 0x02) != 0;

                apply_record_fixups(rec, record_size);

                uint32_t header_rec_num = *reinterpret_cast<uint32_t*>(rec + 0x2C);
                uint64_t frn = (header_rec_num != 0) ? header_rec_num : (chunk_start_rec + r);

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
                                    // Prefer Win32 (1) / Win32+DOS (3) / POSIX (0) over pure DOS 8.3 (2)
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

                if (!best_name.empty() && frn != 5) { // Skip record 5 (root itself)
                    items.push_back({
                        frn,
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
            bytes_read_in_extent += read_now;
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
    frn_to_node.reserve(items.size());
    frn_to_node[5] = root_node_idx; // Root folder FRN is 5

    // Pass 1: add all directory nodes
    for (const auto& it : items) {
        if (it.is_dir) {
            uint32_t n_idx = tree.add_node_w(
                it.name.c_str(), 0, 0, INVALID_INDEX, true, it.attributes
            );
            frn_to_node[it.frn] = n_idx;
        }
    }

    // Pass 2: link all directory nodes to their parents
    for (const auto& it : items) {
        if (it.is_dir) {
            auto self_it = frn_to_node.find(it.frn);
            if (self_it == frn_to_node.end()) continue;
            uint32_t self_idx = self_it->second;
            if (self_idx == INVALID_INDEX || self_idx == root_node_idx) continue;

            uint32_t p_idx = root_node_idx;
            auto p_it = frn_to_node.find(it.parent_frn);
            if (p_it != frn_to_node.end()) {
                p_idx = p_it->second;
            }

            auto& self_node = tree.get_node_mut(self_idx);
            self_node.parent_idx = p_idx;

            auto& parent_node = tree.get_node_mut(p_idx);
            self_node.next_sibling = parent_node.first_child;
            parent_node.first_child = self_idx;
        }
    }

    // Pass 3: add all leaf files
    for (const auto& it : items) {
        if (!it.is_dir) {
            uint32_t p_idx = root_node_idx;
            auto p_it = frn_to_node.find(it.parent_frn);
            if (p_it != frn_to_node.end()) {
                p_idx = p_it->second;
            }

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
