#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
using socket_t = SOCKET;
#define CLOSE_SOCKET closesocket
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSE_SOCKET close
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#include "core/file_tree.h"
#include "core/fast_scanner.h"
#include "core/mft_reader.h"
#include "core/treemap.h"
#include "core/security.h"
#include "mcp/mcp_protocol.h"
#include "mcp/token_optimizer.h"

using namespace TreeMCP;

static FileTree g_tree;
static FastScanner g_scanner;
static std::unique_ptr<McpProtocol> g_mcp;
static std::atomic<bool> g_server_running{true};
static std::string g_session_token;
static int g_port = 4242;

// Read local static file
std::string read_static_file(const std::string& rel_path) {
    std::string full_path = "src/ui/" + rel_path;
    std::ifstream f(full_path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Fast JSON serializer for Treemap Tiles
std::string serialize_tiles(const std::vector<TreemapTile>& tiles) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (i > 0) ss << ",";
        const auto& t = tiles[i];

        std::string esc_name, esc_path;
        for (char c : t.name) { if (c == '\\' || c == '"') esc_name += '\\'; esc_name += c; }
        for (char c : t.full_path) { if (c == '\\' || c == '"') esc_path += '\\'; esc_path += c; }

        ss << "{\"node_idx\":" << t.node_idx
           << ",\"name\":\"" << esc_name << "\""
           << ",\"full_path\":\"" << esc_path << "\""
           << ",\"size\":" << t.size
           << ",\"x\":" << t.x << ",\"y\":" << t.y
           << ",\"width\":" << t.width << ",\"height\":" << t.height
           << ",\"color\":\"" << t.color << "\""
           << ",\"is_dir\":" << (t.is_dir ? "true" : "false")
           << ",\"depth\":" << t.depth
           << "}";
    }
    ss << "]";
    return ss.str();
}

// Fast JSON serializer for Extension Stats
std::string serialize_extensions(const std::vector<ExtStat>& stats) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < stats.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"ext\":\"" << stats[i].ext << "\""
           << ",\"count\":" << stats[i].count
           << ",\"total_size\":" << stats[i].total_size
           << ",\"color\":\"" << stats[i].color << "\"}";
    }
    ss << "]";
    return ss.str();
}

// Fast JSON serializer for Drive List
std::string serialize_drives(const std::vector<DriveInfo>& drives) {
    std::stringstream ss;
    ss << "{\"drives\":[";
    for (size_t i = 0; i < drives.size(); ++i) {
        if (i > 0) ss << ",";
        const auto& d = drives[i];
        
        std::string esc_path, esc_vol;
        for (char c : d.path) { if (c == '\\' || c == '"') esc_path += '\\'; esc_path += c; }
        for (char c : d.volume_name) { if (c == '\\' || c == '"') esc_vol += '\\'; esc_vol += c; }

        ss << "{\"path\":\"" << esc_path << "\""
           << ",\"volume_name\":\"" << esc_vol << "\""
           << ",\"file_system\":\"" << d.file_system << "\""
           << ",\"total_bytes\":" << d.total_bytes
           << ",\"free_bytes\":" << d.free_bytes
           << ",\"is_ntfs\":" << (d.is_ntfs ? "true" : "false")
           << "}";
    }
    ss << "]}";
    return ss.str();
}

// Node serialization for UI Tree
std::string serialize_tree_for_ui(uint32_t idx, int max_depth, int cur_depth = 0) {
    if (idx == INVALID_INDEX || idx >= g_tree.node_count()) return "null";
    const auto& node = g_tree.get_node(idx);

    std::string name = g_tree.get_name(idx);
    std::string full_path = g_tree.get_full_path(idx);
    std::string esc_name, esc_path;
    for (char c : name) { if (c == '\\' || c == '"') esc_name += '\\'; esc_name += c; }
    for (char c : full_path) { if (c == '\\' || c == '"') esc_path += '\\'; esc_path += c; }

    std::stringstream ss;
    ss << "{\"node_idx\":" << idx
       << ",\"name\":\"" << esc_name << "\""
       << ",\"full_path\":\"" << esc_path << "\""
       << ",\"size\":" << node.size
       << ",\"allocated_size\":" << node.allocated_size
       << ",\"is_dir\":" << (node.is_directory() ? "true" : "false")
       << ",\"file_count\":" << node.file_count
       << ",\"dir_count\":" << node.dir_count
       << ",\"children\":[";

    if (cur_depth < max_depth && node.is_directory()) {
        auto children = g_tree.get_children(idx);
        for (size_t i = 0; i < children.size(); ++i) {
            if (i > 0) ss << ",";
            ss << serialize_tree_for_ui(children[i], max_depth, cur_depth + 1);
        }
    }
    ss << "]}";
    return ss.str();
}

// Extract header value from raw HTTP request
std::string get_http_header(const std::string& req, const std::string& header_name) {
    std::string pat = "\r\n" + header_name + ":";
    size_t pos = req.find(pat);
    if (pos == std::string::npos) {
        if (req.find(header_name + ":") == 0) {
            pos = 0;
            pat = header_name + ":";
        } else {
            return "";
        }
    } else {
        pos += 2;
    }

    pos += header_name.size() + 1;
    while (pos < req.size() && req[pos] == ' ') pos++;

    size_t line_end = req.find("\r\n", pos);
    if (line_end == std::string::npos) line_end = req.size();
    return req.substr(pos, line_end - pos);
}

// Extract query param from URI
std::string get_query_param(const std::string& uri, const std::string& param_name) {
    size_t q = uri.find('?');
    if (q == std::string::npos) return "";

    std::string query = uri.substr(q + 1);
    std::string pat = param_name + "=";
    size_t p = query.find(pat);
    if (p == std::string::npos) return "";

    p += pat.size();
    size_t end = query.find('&', p);
    if (end == std::string::npos) end = query.size();
    return query.substr(p, end - p);
}

// HTTP Connection Handler
void handle_http_client(socket_t client_sock) {
    char buf[65536];
    int bytes_read = recv(client_sock, buf, sizeof(buf) - 1, 0);
    if (bytes_read <= 0) {
        CLOSE_SOCKET(client_sock);
        return;
    }
    buf[bytes_read] = '\0';
    std::string req(buf, bytes_read);

    std::string method, uri;
    std::istringstream req_stream(req);
    req_stream >> method >> uri;

    std::string content_type = "text/plain";
    std::string body;
    int status_code = 200;
    std::string status_text = "OK";

    // Host Header Validation (Anti-DNS Rebinding)
    std::string host_header = get_http_header(req, "Host");
    if (!Security::is_valid_host_header(host_header, g_port)) {
        status_code = 403;
        status_text = "Forbidden";
        body = "{\"error\":\"Forbidden: Invalid Host header (Anti-DNS Rebinding Guard)\"}";
        content_type = "application/json";
    }
    // Authentication Check for API and MCP endpoints (Anti-CSRF)
    else if (uri.find("/api/") == 0 || uri.find("/mcp") == 0) {
        std::string token_hdr = get_http_header(req, "X-TreeMCP-Token");
        std::string token_param = get_query_param(uri, "token");
        std::string req_token = !token_hdr.empty() ? token_hdr : token_param;

        if (!Security::constant_time_compare(req_token, g_session_token)) {
            status_code = 401;
            status_text = "Unauthorized";
            body = "{\"error\":\"Unauthorized: Missing or invalid session token.\"}";
            content_type = "application/json";
        }
    }

    if (status_code == 200) {
        std::string clean_uri = uri;
        size_t qmark = clean_uri.find('?');
        if (qmark != std::string::npos) {
            clean_uri = clean_uri.substr(0, qmark);
        }

        if (clean_uri == "/" || clean_uri == "/index.html") {
            content_type = "text/html; charset=UTF-8";
            body = read_static_file("index.html");
        } else if (clean_uri == "/styles.css") {
            content_type = "text/css; charset=UTF-8";
            body = read_static_file("styles.css");
        } else if (clean_uri == "/app.js") {
            content_type = "application/javascript; charset=UTF-8";
            body = read_static_file("app.js");
        } else if (clean_uri == "/treemap_renderer.js") {
            content_type = "application/javascript; charset=UTF-8";
            body = read_static_file("treemap_renderer.js");
        } else if (clean_uri == "/api/drives") {
            content_type = "application/json";
            auto drives = FastScanner::list_drives();
            body = serialize_drives(drives);
        } else if (clean_uri.find("/api/scan") == 0) {
            content_type = "application/json";
            std::string path_param = get_query_param(uri, "path");
            if (path_param.empty()) {
#ifdef _WIN32
                path_param = "C:\\";
#else
                path_param = "/";
#endif
            }

            // Basic URL decode
            std::string decoded;
            for (size_t i = 0; i < path_param.size(); ++i) {
                if (path_param[i] == '%' && i + 2 < path_param.size()) {
                    int val = 0;
                    std::istringstream hex_stream(path_param.substr(i + 1, 2));
                    if (hex_stream >> std::hex >> val) {
                        decoded += static_cast<char>(val);
                        i += 2;
                        continue;
                    }
                }
                decoded += path_param[i];
            }
            path_param = decoded;

            if (!Security::is_path_safe_to_read(path_param)) {
                status_code = 400;
                status_text = "Bad Request";
                body = "{\"error\":\"Path rejected by security policy.\"}";
            } else {
                ScanStats stats;
                g_scanner.scan(path_param, g_tree, stats);

                auto tiles = TreemapEngine::compute_layout(g_tree, g_tree.root_idx(), 0, 0, 1000, 600, 2, 500 * 1024);
                auto exts = g_tree.get_extension_stats(g_tree.root_idx(), 15);

                std::stringstream ss;
                ss << "{\"stats\":{\"total_files\":" << stats.total_files
                   << ",\"total_dirs\":" << stats.total_dirs
                   << ",\"total_bytes\":" << stats.total_bytes
                   << ",\"duration_ms\":" << stats.duration_ms
                   << ",\"mft_used\":" << (stats.mft_used ? "true" : "false")
                   << "}"
                   << ",\"tree_root\":" << serialize_tree_for_ui(g_tree.root_idx(), 2)
                   << ",\"treemap_tiles\":" << serialize_tiles(tiles)
                   << ",\"extensions\":" << serialize_extensions(exts)
                   << "}";
                body = ss.str();
            }
        } else if (clean_uri.find("/api/treemap") == 0) {
            content_type = "application/json";
            uint32_t node_idx = g_tree.root_idx();
            std::string nidx_str = get_query_param(uri, "node_idx");
            if (!nidx_str.empty()) {
                node_idx = std::stoul(nidx_str);
            }
            auto tiles = TreemapEngine::compute_layout(g_tree, node_idx, 0, 0, 1000, 600, 2, 100 * 1024);
            body = "{\"tiles\":" + serialize_tiles(tiles) + "}";
        } else if (clean_uri == "/api/mcp-stats") {
            content_type = "application/json";
            std::stringstream ss;
            ss << "{\"total_queries\":" << g_mcp->total_queries()
               << ",\"total_tokens_used\":" << g_mcp->total_tokens_used()
               << ",\"total_tokens_saved\":" << g_mcp->total_tokens_saved()
               << ",\"session_token\":\"" << g_session_token << "\""
               << "}";
            body = ss.str();
        } else if (clean_uri.find("/api/open-explorer") == 0) {
            content_type = "application/json";
            std::string p = get_query_param(uri, "path");
            if (Security::is_path_safe_to_read(p)) {
#ifdef _WIN32
                int wlen = MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, NULL, 0);
                std::wstring wp(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, &wp[0], wlen);
                ShellExecuteW(NULL, L"explore", wp.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
                std::string cmd = "xdg-open \"" + p + "\" >/dev/null 2>&1 &";
                system(cmd.c_str());
#endif
                body = "{\"ok\":true}";
            } else {
                body = "{\"error\":\"Path rejected by security policy.\"}";
            }
        } else if (clean_uri == "/mcp" || clean_uri == "/api/mcp") {
            content_type = "application/json";
            size_t body_pos = req.find("\r\n\r\n");
            std::string post_body = (body_pos != std::string::npos) ? req.substr(body_pos + 4) : "{}";
            body = g_mcp->process_message(post_body);
        } else {
            status_code = 404;
            status_text = "Not Found";
            body = "Not Found";
        }
    }

    std::stringstream resp;
    resp << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
         << "Content-Type: " << content_type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "X-Content-Type-Options: nosniff\r\n"
         << "X-Frame-Options: DENY\r\n"
         << "Content-Security-Policy: default-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self' http://127.0.0.1:* http://localhost:*;\r\n"
         << "Referrer-Policy: no-referrer\r\n"
         << "Connection: close\r\n\r\n"
         << body;

    std::string resp_str = resp.str();
    send(client_sock, resp_str.c_str(), (int)resp_str.size(), 0);
    CLOSE_SOCKET(client_sock);
}

void run_web_server(int port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        CLOSE_SOCKET(server_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    listen(server_sock, 10);
    std::cout << "[TreeMCP Server] Listening on http://127.0.0.1:" << port << std::endl;
    std::cout << "[Security Policy] Anti-DNS Rebinding: ACTIVE | Session Token Auth: ACTIVE\n";

    while (g_server_running) {
        socket_t client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == INVALID_SOCKET) break;
        std::thread(handle_http_client, client_sock).detach();
    }

    CLOSE_SOCKET(server_sock);
#ifdef _WIN32
    WSACleanup();
#endif
}

int main(int argc, char* argv[]) {
    g_session_token = Security::generate_secure_token();
    g_mcp = std::make_unique<McpProtocol>(g_tree, g_scanner);

    // Mode 0: CLI Uninstall
    if (argc >= 2 && std::string(argv[1]) == "--uninstall") {
        std::cout << "===============================================\n";
        std::cout << "  TreeMCP Uninstallation\n";
        std::cout << "===============================================\n";
#ifdef _WIN32
        std::cout << "To unregister MCP and remove all files, run in PowerShell:\n";
        std::cout << "  irm https://raw.githubusercontent.com/AMG555/TreeMCP/main/uninstall.ps1 | iex\n";
#else
        std::cout << "To unregister MCP and remove binary, run in terminal:\n";
        std::cout << "  curl -fsSL https://raw.githubusercontent.com/AMG555/TreeMCP/main/uninstall.sh | bash\n";
#endif
        std::cout << "===============================================\n";
        return 0;
    }

    // Mode 1: MCP Stdio Server
    if (argc >= 2 && std::string(argv[1]) == "--mcp") {
        g_mcp->run_stdio_server();
        return 0;
    }

    // Mode 2: Quick CLI Scan
    if (argc >= 3 && std::string(argv[1]) == "--scan") {
        std::string path = argv[2];
        if (!Security::is_path_safe_to_read(path)) {
            std::cerr << "[SECURITY REJECTED] Unsafe path: " << path << std::endl;
            return 1;
        }

        ScanStats stats;
        std::cout << "Scanning " << path << "..." << std::endl;
        g_scanner.scan(path, g_tree, stats);
        std::cout << "Scan finished in " << (stats.duration_ms / 1000.0) << "s!\n";
        std::cout << "Files: " << stats.total_files << " | Folders: " << stats.total_dirs
                  << " | Size: " << FileTree::format_size(stats.total_bytes) << "\n\n";

        std::cout << "Top 10 Hogs:\n";
        auto hogs = g_tree.get_top_hogs(g_tree.root_idx(), 10, 0);
        std::cout << TokenOptimizer::format_hogs(hogs) << "\n";
        return 0;
    }

    // Mode 3: Desktop UI + Server
    int port = 4242;
    if (argc >= 3 && std::string(argv[1]) == "--serve") {
        port = std::stoi(argv[2]);
    }
    g_port = port;

    std::cout << "=====================================================\n";
    std::cout << "  TreeMCP - Cross-Platform Disk Analyzer & MCP\n";
    std::cout << "=====================================================\n";
    std::cout << "  * Local MCP Server: Active on stdio & http://127.0.0.1:" << port << "/mcp\n";
    std::cout << "  * Session Token:    " << g_session_token.substr(0, 8) << "... [REDACTED 256-bit]\n";
    std::cout << "  * Desktop UI:       http://127.0.0.1:" << port << "/?token=" << g_session_token << "\n";
    std::cout << "=====================================================\n\n";

    // Launch UI
    std::thread([port]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        std::string target_url = "http://127.0.0.1:" + std::to_string(port) + "/?token=" + g_session_token;
#ifdef _WIN32
        std::string edge_path = "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
        std::string cmd = "\"" + edge_path + "\" --app=\"" + target_url + "\" --window-size=1280,820";
        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#else
        std::string cmd = "xdg-open \"" + target_url + "\" >/dev/null 2>&1 &";
        system(cmd.c_str());
#endif
    }).detach();

    run_web_server(port);
    return 0;
}
