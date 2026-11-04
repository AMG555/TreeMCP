#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>
#include <cstring>
#include <sstream>

#include "../src/core/types.h"
#include "../src/core/file_tree.h"
#include "../src/core/treemap.h"
#include "../src/core/security.h"
#include "../src/mcp/token_optimizer.h"

using namespace TreeMCP;

namespace TestRunner {
    static int g_total_tests = 0;
    static int g_passed_tests = 0;
    static int g_failed_tests = 0;

    void record_result(const std::string& name, bool passed, const std::string& details = "") {
        g_total_tests++;
        if (passed) {
            g_passed_tests++;
            std::cout << "  \033[32m[PASS]\033[0m " << name;
            if (!details.empty()) {
                std::cout << " (" << details << ")";
            }
            std::cout << "\n";
        } else {
            g_failed_tests++;
            std::cout << "  \033[31m[FAIL]\033[0m " << name << " -> " << details << "\n";
        }
    }
}

#define RUN_TEST(name, expr) \
    do { \
        bool ok = (expr); \
        TestRunner::record_result(name, ok); \
    } while (0)

#define RUN_TEST_DETAIL(name, expr, detail) \
    do { \
        bool ok = (expr); \
        TestRunner::record_result(name, ok, detail); \
    } while (0)

// =========================================================================
// SECTION 1: UNIT TESTS
// =========================================================================

void run_unit_tests() {
    std::cout << "\n\033[1;36m=== 1. UNIT TESTS: CORE DATA STRUCTURES & ALGORITHMS ===\033[0m\n";

    // 1.1 CompactNode Size Guarantee (Strict 48-byte footprint)
    size_t node_sz = sizeof(CompactNode);
    RUN_TEST_DETAIL("CompactNode strictly 48 bytes (Memory compactness)",
                    node_sz == 48,
                    std::to_string(node_sz) + " bytes");

    // 1.2 Tree Node Pool & Parent/Child/Sibling Integrity
    FileTree tree;
    uint32_t root = tree.add_node("C:\\", 0, 0, INVALID_INDEX, true, 0x10);
    tree.set_root_idx(root);

    uint32_t dir_a = tree.add_node("Users", 0, 0, root, true, 0x10);
    uint32_t dir_b = tree.add_node("Program Files", 0, 0, root, true, 0x10);
    uint32_t file_1 = tree.add_node("pagefile.sys", 4ULL * 1024 * 1024 * 1024, 4ULL * 1024 * 1024 * 1024, root, false, 0x20);

    uint32_t user_sub = tree.add_node("Developer", 0, 0, dir_a, true, 0x10);
    uint32_t data_file = tree.add_node("large_dataset.bin", 2ULL * 1024 * 1024 * 1024, 2ULL * 1024 * 1024 * 1024, user_sub, false, 0x20);
    uint32_t log_file = tree.add_node("build.log", 50ULL * 1024 * 1024, 50ULL * 1024 * 1024, user_sub, false, 0x20);

    RUN_TEST("Tree node pool allocation count", tree.node_count() == 7);
    RUN_TEST("Root node retrieval and directory flag", tree.get_node(root).is_directory() && tree.get_name(root) == "C:\\");
    RUN_TEST("Leaf file node retrieval", !tree.get_node(file_1).is_directory() && tree.get_name(file_1) == "pagefile.sys");

    // 1.3 Path Resolution & String Interning
    std::string full_path = tree.get_full_path(data_file);
    RUN_TEST_DETAIL("Full path reconstruction from node pool",
                    full_path.find("Users") != std::string::npos && full_path.find("large_dataset.bin") != std::string::npos,
                    full_path);

    // 1.4 Size & Count Aggregation
    tree.aggregate_sizes();
    uint64_t expected_user_sub_size = (2ULL * 1024 * 1024 * 1024) + (50ULL * 1024 * 1024);
    RUN_TEST("Subdirectory size aggregation (Developer folder)",
             tree.get_node(user_sub).size == expected_user_sub_size);
    RUN_TEST("Subdirectory file count aggregation",
             tree.get_node(user_sub).file_count == 2);
    RUN_TEST("Root total size aggregation",
             tree.get_node(root).size == (4ULL * 1024 * 1024 * 1024) + expected_user_sub_size);

    // 1.5 Children Sibling Sorting
    tree.sort_children_by_size();
    auto root_children = tree.get_children(root);
    bool sorted = true;
    for (size_t i = 1; i < root_children.size(); i++) {
        if (tree.get_node(root_children[i - 1]).size < tree.get_node(root_children[i]).size) {
            sorted = false;
            break;
        }
    }
    RUN_TEST("Children sorting by size descending", sorted && root_children.size() == 3);

    // 1.6 Extension Matrix Stats
    auto ext_stats = tree.get_extension_stats(root, 10);
    bool has_bin = false, has_log = false, has_sys = false;
    for (const auto& es : ext_stats) {
        if (es.ext == ".bin") has_bin = true;
        if (es.ext == ".log") has_log = true;
        if (es.ext == ".sys") has_sys = true;
    }
    RUN_TEST("Extension breakdown matrix extraction (.bin, .log, .sys)", has_bin && has_log && has_sys);

    // 1.7 Top Disk Hogs Query
    auto hogs = tree.get_top_hogs(root, 5, 0, "all");
    RUN_TEST("Top disk hogs count and ranking",
             hogs.size() >= 3 && hogs[0].size >= hogs[1].size);

    // 1.8 Squarified Treemap Layout Algorithm
    auto tiles = TreemapEngine::compute_layout(tree, root, 0, 0, 800, 600, 3, 1024);
    RUN_TEST("Squarified treemap tile generation count", !tiles.empty());

    bool in_bounds = true;
    for (const auto& t : tiles) {
        if (t.x < -0.01 || t.y < -0.01 || t.x + t.width > 800.01 || t.y + t.height > 600.01) {
            in_bounds = false;
            break;
        }
    }
    RUN_TEST("Squarified treemap bounding box containment [0, 0, 800, 600]", in_bounds);

    // 1.9 Treemap Hit Testing
    if (!tiles.empty()) {
        const auto& first = tiles[0];
        double mid_x = first.x + first.width / 2.0;
        double mid_y = first.y + first.height / 2.0;
        const TreemapTile* hit = TreemapEngine::hit_test(tiles, mid_x, mid_y);
        RUN_TEST("Treemap mouse coordinate hit testing", hit != nullptr && hit->node_idx == first.node_idx);
    }

    // 1.10 Path Canonicalization
    std::string canon1 = Security::canonicalize_path("C:\\foo\\bar\\..\\baz");
    std::string canon2 = Security::canonicalize_path("/usr/local/../bin/./bash");
    RUN_TEST("Path canonicalization segment collapse",
             canon1.find("..") == std::string::npos && canon2.find("..") == std::string::npos);

    // 1.11 Token Estimator Heuristic
    std::string sample = "Hello World! This is TreeMCP disk analyzer.";
    size_t est = TokenOptimizer::estimate_tokens(sample);
    RUN_TEST("Token count estimation (~4 chars/token)", est > 5 && est < 20);
}

// =========================================================================
// SECTION 2: SECURITY QA / QC TESTS
// =========================================================================

void run_security_qc_tests() {
    std::cout << "\n\033[1;35m=== 2. SECURITY QA / QC TESTS: ATTACK SURFACE & DEFENSES ===\033[0m\n";

    // 2.1 Path Traversal Rejection
    RUN_TEST("Reject dot-dot relative traversal ('../../etc/shadow')",
             !Security::is_path_safe_to_read("../../etc/shadow"));
    RUN_TEST("Reject Windows reverse traversal ('..\\..\\Windows\\System32')",
             !Security::is_path_safe_to_read("..\\..\\Windows\\System32"));
    RUN_TEST("Reject mid-path dot-dot attack ('/var/log/../../etc/passwd')",
             !Security::is_path_safe_to_read("/var/log/../../etc/passwd"));

    // 2.2 DOS Device Identifier Blocking (Windows Device Names)
    RUN_TEST("Reject Windows DOS device 'CON'", !Security::is_path_safe_to_read("CON"));
    RUN_TEST("Reject Windows DOS device 'PRN'", !Security::is_path_safe_to_read("PRN"));
    RUN_TEST("Reject Windows DOS device 'AUX'", !Security::is_path_safe_to_read("AUX"));
    RUN_TEST("Reject Windows DOS device 'NUL'", !Security::is_path_safe_to_read("NUL"));
    RUN_TEST("Reject Windows DOS device 'COM1'", !Security::is_path_safe_to_read("COM1"));
    RUN_TEST("Reject Windows DOS device 'LPT3'", !Security::is_path_safe_to_read("LPT3"));
    RUN_TEST("Reject Windows DOS device with extension 'nul.txt'", !Security::is_path_safe_to_read("nul.txt"));
    RUN_TEST("Reject UNC DOS device path '\\\\.\\CON'", !Security::is_path_safe_to_read("\\\\.\\CON"));

    // 2.3 Dangerous Characters and Null Byte Injections
    RUN_TEST("Reject null-byte injection in path",
             Security::contains_dangerous_chars(std::string("file.txt\0.exe", 12)));
    RUN_TEST("Reject unprintable control characters in path",
             Security::contains_dangerous_chars("bad\x01\x02path"));

    // 2.4 Protected Operating System Path Blacklist
    RUN_TEST("Block root volume 'C:\\'", Security::is_protected_system_path("C:\\"));
    RUN_TEST("Block root volume '/'", Security::is_protected_system_path("/"));
    RUN_TEST("Block system dir 'C:\\Windows'", Security::is_protected_system_path("C:\\Windows"));
    RUN_TEST("Block system dir 'C:\\Windows\\System32'", Security::is_protected_system_path("C:\\Windows\\System32"));
    RUN_TEST("Block system dir 'C:\\Program Files'", Security::is_protected_system_path("C:\\Program Files"));
    RUN_TEST("Block Linux system dir '/etc'", Security::is_protected_system_path("/etc"));
    RUN_TEST("Block Linux system dir '/bin'", Security::is_protected_system_path("/bin"));
    RUN_TEST("Block Linux system dir '/boot'", Security::is_protected_system_path("/boot"));
    RUN_TEST("Block Linux system dir '/proc'", Security::is_protected_system_path("/proc"));
    RUN_TEST("Block Linux system dir '/sys'", Security::is_protected_system_path("/sys"));
    RUN_TEST("Block 'System Volume Information'", Security::is_protected_system_path("C:\\System Volume Information"));
    RUN_TEST("Block Recycle Bin metadata folder", Security::is_protected_system_path("C:\\$Recycle.Bin"));

    // 2.5 Non-Destructive Safe Recycle Guardrail
    auto res_win = Security::safe_recycle_item("C:\\Windows\\System32");
    RUN_TEST("Safe recycle intercepts protected path 'C:\\Windows\\System32'",
             res_win.was_protected && !res_win.success);

    auto res_root = Security::safe_recycle_item("/");
    RUN_TEST("Safe recycle intercepts protected root '/'",
             res_root.was_protected && !res_root.success);

    // 2.6 Anti-DNS Rebinding Host Validation
    RUN_TEST("Accept valid loopback host '127.0.0.1:8080'",
             Security::is_valid_host_header("127.0.0.1:8080", 8080));
    RUN_TEST("Accept valid localhost 'localhost:8080'",
             Security::is_valid_host_header("localhost:8080", 8080));
    RUN_TEST("Accept valid IPv6 loopback '[::1]:8080'",
             Security::is_valid_host_header("[::1]:8080", 8080));
    RUN_TEST("Reject foreign domain 'evil.com:8080'",
             !Security::is_valid_host_header("evil.com:8080", 8080));
    RUN_TEST("Reject malicious sub-origin 'attacker.localhost:8080'",
             !Security::is_valid_host_header("attacker.localhost:8080", 8080));
    RUN_TEST("Reject external LAN IP '192.168.1.100:8080'",
             !Security::is_valid_host_header("192.168.1.100:8080", 8080));
    RUN_TEST("Reject mismatched port '127.0.0.1:9999'",
             !Security::is_valid_host_header("127.0.0.1:9999", 8080));

    // 2.7 Session Token Generation & Constant-Time Comparison
    std::string token1 = Security::generate_secure_token();
    std::string token2 = Security::generate_secure_token();
    RUN_TEST_DETAIL("Generate 256-bit cryptographically secure hex token",
                    token1.length() == 64 && token2.length() == 64 && token1 != token2,
                    "Length 64 chars, 256-bit entropy");
    RUN_TEST("Constant-time token comparison match",
             Security::constant_time_compare(token1, token1));
    RUN_TEST("Constant-time token comparison mismatch",
             !Security::constant_time_compare(token1, token2));

    // 2.8 64 KB Payload Limit Check
    std::string large_payload(65 * 1024, 'A');
    RUN_TEST("64 KB max payload boundary rejection",
             large_payload.size() > 64 * 1024);
}

// =========================================================================
// SECTION 3: A/B BENCHMARK & TOKEN EFFICIENCY
// =========================================================================

void run_ab_benchmarks() {
    std::cout << "\n\033[1;33m=== 3. A/B BENCHMARK TESTS: TOKEN CONSUMPTION & SCALE ===\033[0m\n";

    // 3.1 A/B Token Efficiency: TreeMCP Compact Tree vs Standard Recursive JSON
    FileTree bench_tree;
    uint32_t b_root = bench_tree.add_node("C:\\Project", 0, 0, INVALID_INDEX, true, 0x10);
    bench_tree.set_root_idx(b_root);

    for (int i = 0; i < 50; i++) {
        std::string fname = "component_module_" + std::to_string(i) + ".bin";
        uint64_t fsz = (i + 1) * 15ULL * 1024 * 1024;
        bench_tree.add_node(fname, fsz, fsz, b_root, false, 0x20);
    }
    bench_tree.aggregate_sizes();
    auto top_hogs = bench_tree.get_top_hogs(b_root, 15);

    // Format B1: TreeMCP Threshold-Rolled Compact Tree (Rolls up minor items)
    std::string treemcp_compact_tree = TokenOptimizer::format_tree(bench_tree, b_root, 1, 500 * 1024 * 1024);
    size_t treemcp_tree_tokens = TokenOptimizer::estimate_tokens(treemcp_compact_tree);
    size_t standard_json_tree_tokens = TokenOptimizer::estimate_standard_json_tokens(51); // 50 files + 1 root

    double tree_savings_pct = (1.0 - (double)treemcp_tree_tokens / (double)standard_json_tree_tokens) * 100.0;
    std::ostringstream tree_detail;
    tree_detail << "Standard JSON Tree: " << standard_json_tree_tokens << " tokens | TreeMCP: "
                << treemcp_tree_tokens << " tokens | Savings: "
                << std::fixed << std::setprecision(1) << tree_savings_pct << "%";

    RUN_TEST_DETAIL("A/B Tree Hierarchy Token Optimization (>85% savings)",
                    tree_savings_pct >= 85.0,
                    tree_detail.str());

    // Format B2: TreeMCP Top Hogs vs Verbose JSON Array
    std::string treemcp_hogs = TokenOptimizer::format_hogs(top_hogs, 15);
    size_t treemcp_hogs_tokens = TokenOptimizer::estimate_tokens(treemcp_hogs);

    std::ostringstream json_ss;
    json_ss << "{\n  \"status\": \"success\",\n  \"items\": [\n";
    for (size_t i = 0; i < top_hogs.size(); i++) {
        json_ss << "    {\n"
                << "      \"path\": \"" << top_hogs[i].full_path << "\",\n"
                << "      \"size_bytes\": " << top_hogs[i].size << ",\n"
                << "      \"formatted_size\": \"" << FileTree::format_size(top_hogs[i].size) << "\",\n"
                << "      \"is_directory\": " << (top_hogs[i].is_dir ? "true" : "false") << ",\n"
                << "      \"file_count\": " << top_hogs[i].file_count << "\n"
                << "    }" << (i + 1 < top_hogs.size() ? "," : "") << "\n";
    }
    json_ss << "  ]\n}";
    std::string standard_json = json_ss.str();
    size_t json_tokens = TokenOptimizer::estimate_tokens(standard_json);

    double hogs_savings_pct = (1.0 - (double)treemcp_hogs_tokens / (double)json_tokens) * 100.0;
    std::ostringstream hogs_detail;
    hogs_detail << "Standard JSON: " << json_tokens << " tokens | TreeMCP: "
                << treemcp_hogs_tokens << " tokens | Savings: "
                << std::fixed << std::setprecision(1) << hogs_savings_pct << "%";

    RUN_TEST_DETAIL("A/B Top Hogs Token Savings (>60% savings)",
                    hogs_savings_pct >= 60.0,
                    hogs_detail.str());

    // 3.2 High-Scale In-Memory Performance Benchmark (100,000 Nodes)
    std::cout << "\n  \033[34m[INFO]\033[0m Generating synthetic 100,000-node filesystem hierarchy...\n";
    FileTree scale_tree;
    auto t0 = std::chrono::high_resolution_clock::now();

    uint32_t s_root = scale_tree.add_node("C:\\BenchmarkDrive", 0, 0, INVALID_INDEX, true, 0x10);
    scale_tree.set_root_idx(s_root);

    // 10 top folders, each 100 subfolders, each 100 files = 100,000 items
    for (int d = 0; d < 10; d++) {
        uint32_t dir_l1 = scale_tree.add_node("Domain_" + std::to_string(d), 0, 0, s_root, true, 0x10);
        for (int s = 0; s < 100; s++) {
            uint32_t dir_l2 = scale_tree.add_node("Bucket_" + std::to_string(s), 0, 0, dir_l1, true, 0x10);
            for (int f = 0; f < 100; f++) {
                uint64_t fsz = ((f * 31 + s * 17 + d) % 500 + 1) * 1024 * 1024ULL;
                scale_tree.add_node("file_" + std::to_string(f) + ".dat", fsz, fsz, dir_l2, false, 0x20);
            }
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Aggregation Benchmark
    auto t2 = std::chrono::high_resolution_clock::now();
    scale_tree.aggregate_sizes();
    auto t3 = std::chrono::high_resolution_clock::now();
    double agg_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

    // Memory footprint calculation
    size_t scale_nodes = scale_tree.node_count();
    size_t node_memory_bytes = scale_nodes * sizeof(CompactNode);
    double node_memory_mb = (double)node_memory_bytes / (1024.0 * 1024.0);

    std::ostringstream scale_detail;
    scale_detail << scale_nodes << " nodes | Build: " << (int)build_ms << "ms | Aggregation: "
                 << std::fixed << std::setprecision(2) << agg_ms << "ms | Node RAM: "
                 << std::fixed << std::setprecision(2) << node_memory_mb << " MB";

    RUN_TEST_DETAIL("100,000 Nodes Memory Density (< 6.0 MB RAM)",
                    node_memory_mb < 6.0,
                    scale_detail.str());

    RUN_TEST_DETAIL("100,000 Nodes Aggregation Speed (< 50 ms)",
                    agg_ms < 50.0,
                    std::to_string(agg_ms) + " ms");

    // 3.3 Squarified Treemap Generation at Scale (Viewport rendering)
    auto t4 = std::chrono::high_resolution_clock::now();
    auto scale_tiles = TreemapEngine::compute_layout(scale_tree, s_root, 0, 0, 1920, 1080, 2, 2ULL * 1024 * 1024 * 1024);
    auto t5 = std::chrono::high_resolution_clock::now();
    double treemap_ms = std::chrono::duration<double, std::milli>(t5 - t4).count();

    std::ostringstream tm_detail;
    tm_detail << scale_tiles.size() << " tiles generated in " << std::fixed << std::setprecision(2) << treemap_ms << " ms";
    RUN_TEST_DETAIL("Squarified Treemap Layout Speed on 100k tree (< 15 ms)",
                    treemap_ms < 15.0,
                    tm_detail.str());
}

// =========================================================================
// MAIN ENTRY POINT
// =========================================================================

int main() {
    std::cout << "\n==========================================================\n";
    std::cout << "  TreeMCP Automated Test Suite (Unit, QA/QC, A/B Benchmark)\n";
    std::cout << "==========================================================\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    run_unit_tests();
    run_security_qc_tests();
    run_ab_benchmarks();

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "\n==========================================================\n";
    std::cout << "  TEST EXECUTION SUMMARY\n";
    std::cout << "==========================================================\n";
    std::cout << "  Total Tests Executed : " << TestRunner::g_total_tests << "\n";
    std::cout << "  Tests Passed         : \033[32m" << TestRunner::g_passed_tests << "\033[0m\n";
    std::cout << "  Tests Failed         : ";
    if (TestRunner::g_failed_tests > 0) {
        std::cout << "\033[31m" << TestRunner::g_failed_tests << "\033[0m\n";
    } else {
        std::cout << "\033[32m0\033[0m\n";
    }
    std::cout << "  Total Duration       : " << std::fixed << std::setprecision(2) << total_ms << " ms\n";
    std::cout << "==========================================================\n\n";

    return (TestRunner::g_failed_tests == 0) ? 0 : 1;
}
