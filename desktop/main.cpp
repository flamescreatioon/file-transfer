#include "protocol.hpp"
#include "web_assets.hpp"
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cstring>

#ifndef AIRBRIDGE_VERSION
#define AIRBRIDGE_VERSION "dev"
#endif

#ifdef _WIN32
    #include <windows.h>
    typedef int SockLenType;
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
    typedef socklen_t SockLenType;
#endif

namespace fs = std::filesystem;

// Structures
struct Peer {
    std::string name;
    std::string ip;
    int port;
    std::string os;
    std::chrono::steady_clock::time_point last_seen;
};

class ThreadPool {
public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::move(task));
        }
        condition.notify_one();
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker: workers) {
            if (worker.joinable()) worker.join();
        }
    }
    
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// Global variables
std::vector<Peer> peers;
std::mutex peers_mutex;
std::mutex console_mutex;
std::string my_name;
std::string my_ip;
bool running = true;
std::string last_received_clip = "";
std::vector<std::string> clipboard_history;
ThreadPool* thread_pool = nullptr;

#include <set>
std::set<std::string> active_transfers;
std::mutex active_transfers_mutex;

void add_active_transfer(const std::string& ip) {
    std::lock_guard<std::mutex> lock(active_transfers_mutex);
    active_transfers.insert(ip);
}

void remove_active_transfer(const std::string& ip) {
    std::lock_guard<std::mutex> lock(active_transfers_mutex);
    active_transfers.erase(ip);
}

bool is_transfer_active_for(const std::string& ip) {
    std::lock_guard<std::mutex> lock(active_transfers_mutex);
    return active_transfers.find(ip) != active_transfers.end();
}

void touch_peer(const std::string& ip) {
    std::lock_guard<std::mutex> lock(peers_mutex);
    for (auto& peer : peers) {
        if (peer.ip == ip) {
            peer.last_seen = std::chrono::steady_clock::now();
            break;
        }
    }
}

// System Clipboard Helpers
void copy_to_system_clipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hGlob) {
        CloseClipboard();
        return;
    }
    memcpy(GlobalLock(hGlob), text.c_str(), text.size() + 1);
    GlobalUnlock(hGlob);
    SetClipboardData(CF_TEXT, hGlob);
    CloseClipboard();
#else
    FILE* pipe = popen("xclip -selection clipboard", "w");
    if (pipe) {
        fwrite(text.c_str(), 1, text.size(), pipe);
        pclose(pipe);
    }
#endif
}

std::string get_local_ip() {
    SocketType s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET_VAL) return "127.0.0.1";
    
    sockaddr_in loopback;
    std::memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = inet_addr("8.8.8.8");
    loopback.sin_port = htons(80);
    
    if (connect(s, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == SOCKET_ERROR_VAL) {
        close_socket(s);
        return "127.0.0.1";
    }
    
    sockaddr_in name;
    SockLenType namelen = sizeof(name);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&name), &namelen) == SOCKET_ERROR_VAL) {
        close_socket(s);
        return "127.0.0.1";
    }
    
    char buffer[16];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
    close_socket(s);
    if (p) return std::string(p);
    return "127.0.0.1";
}

std::string get_device_name() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "Desktop-Peer";
}

// UDP Broadcast thread: sends ping packets
void udp_broadcast_thread() {
    SocketType s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET_VAL) return;

#ifdef _WIN32
    char broadcast_opt = '1';
#else
    int broadcast_opt = 1;
#endif
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(broadcast_opt));

    sockaddr_in broadcast_addr;
    std::memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(UDP_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    sockaddr_in multicast_addr;
    std::memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, "224.0.0.1", &multicast_addr.sin_addr);

#ifdef _WIN32
    std::string os_type = "Windows";
#else
    std::string os_type = "Linux";
#endif

    while (running) {
        std::stringstream ss;
        ss << "AIRB_DISCOVER:" << my_name << ":" << os_type << ":" << TCP_PORT;
        std::string payload = ss.str();
        
        sendto(s, payload.c_str(), static_cast<int>(payload.size()), 0, 
               reinterpret_cast<sockaddr*>(&broadcast_addr), sizeof(broadcast_addr));
        sendto(s, payload.c_str(), static_cast<int>(payload.size()), 0, 
               reinterpret_cast<sockaddr*>(&multicast_addr), sizeof(multicast_addr));
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    close_socket(s);
}

// UDP Listener thread: discovers peers
void udp_listen_thread() {
    SocketType s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET_VAL) return;

#ifdef _WIN32
    char reuse = '1';
#else
    int reuse = 1;
#endif
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in bind_addr;
    std::memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(UDP_PORT);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR_VAL) {
        close_socket(s);
        return;
    }

    char buffer[1024];
    sockaddr_in sender_addr;
    SockLenType sender_len = sizeof(sender_addr);

    while (running) {
        int bytes = recv(s, buffer, sizeof(buffer) - 1, 0); // Note: recvfrom is preferred for UDP, let's fix that
        // Wait, standard recv on UDP requires connection or we can use recvfrom
        // Let's use recvfrom to get remote IP correctly!
        int bytes_recv = recvfrom(s, buffer, sizeof(buffer) - 1, 0, 
                                  reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);
        if (bytes_recv <= 0) continue;
        buffer[bytes_recv] = '\0';

        std::string msg(buffer);
        if (msg.rfind("AIRB_DISCOVER:", 0) == 0) {
            std::stringstream ss(msg.substr(14));
            std::string name, os_str, port_str;
            if (std::getline(ss, name, ':') && std::getline(ss, os_str, ':') && std::getline(ss, port_str, ':')) {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sender_addr.sin_addr, ip_str, sizeof(ip_str));
                std::string ip(ip_str);
                
                if (ip == my_ip) continue;
                
                int port = std::stoi(port_str);
                
                std::lock_guard<std::mutex> lock(peers_mutex);
                bool found = false;
                for (auto& peer : peers) {
                    if (peer.ip == ip && peer.port == port) {
                        peer.name = name;
                        peer.os = os_str;
                        peer.last_seen = std::chrono::steady_clock::now();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    peers.push_back({name, ip, port, os_str, std::chrono::steady_clock::now()});
                }
            }
        }
    }
    close_socket(s);
}

// Thread that periodically prunes stale peers
void peer_pruning_thread() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(peers_mutex);
        peers.erase(std::remove_if(peers.begin(), peers.end(), 
            [now](const Peer& p) {
                if (is_transfer_active_for(p.ip)) {
                    return false;
                }
                return std::chrono::duration_cast<std::chrono::seconds>(now - p.last_seen).count() > 20;
            }), peers.end());
    }
}

// Custom String Parsing Helpers
std::string get_query_param(const std::string& path, const std::string& key) {
    size_t pos = path.find(key + "=");
    if (pos == std::string::npos) return "";
    size_t val_start = pos + key.size() + 1;
    size_t val_end = path.find("&", val_start);
    if (val_end == std::string::npos) {
        return path.substr(val_start);
    }
    return path.substr(val_start, val_end - val_start);
}

std::string url_decode(const std::string& src) {
    std::string ret;
    char ch;
    int ii;
    for (size_t i = 0; i < src.length(); i++) {
        if (src[i] == '%') {
            if (sscanf(src.substr(i + 1, 2).c_str(), "%x", &ii) == 1) {
                ch = static_cast<char>(ii);
                ret += ch;
                i += 2;
            }
        } else if (src[i] == '+') {
            ret += ' ';
        } else {
            ret += src[i];
        }
    }
    return ret;
}

std::string get_json_string_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    size_t colon = json.find(":", pos);
    if (colon == std::string::npos) return "";
    size_t quote_start = json.find("\"", colon);
    if (quote_start == std::string::npos) return "";
    size_t quote_end = json.find("\"", quote_start + 1);
    if (quote_end == std::string::npos) return "";
    return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

int get_json_int_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;
    size_t colon = json.find(":", pos);
    if (colon == std::string::npos) return 0;
    size_t digit_start = json.find_first_of("0123456789", colon);
    if (digit_start == std::string::npos) return 0;
    size_t digit_end = json.find_first_not_of("0123456789", digit_start);
    if (digit_end == std::string::npos) {
        return std::stoi(json.substr(digit_start));
    }
    return std::stoi(json.substr(digit_start, digit_end - digit_start));
}

std::string sanitize_filename(const std::string& filename) {
    std::string s = filename;
    size_t last_slash = s.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        s = s.substr(last_slash + 1);
    }
    const std::string invalid_chars = "<>:\"/\\|?*";
    for (char& c : s) {
        if (invalid_chars.find(c) != std::string::npos || c < 32) {
            c = '_';
        }
    }
    if (s.empty()) s = "unnamed_file";
    return s;
}

std::string get_unique_filename(const std::string& directory, const std::string& filename) {
    fs::path dir(directory);
    fs::path file_path = dir / filename;
    if (!fs::exists(file_path)) return filename;
    
    std::string stem = file_path.stem().string();
    std::string ext = file_path.extension().string();
    int counter = 1;
    while (true) {
        std::string candidate = stem + " (" + std::to_string(counter) + ")" + ext;
        if (!fs::exists(dir / candidate)) {
            return candidate;
        }
        counter++;
    }
}

std::string get_header_value(const std::string& headers, const std::string& key) {
    size_t pos = headers.find(key + ":");
    if (pos == std::string::npos) return "";
    size_t val_start = pos + key.size() + 1;
    size_t val_end = headers.find("\r\n", val_start);
    if (val_end == std::string::npos) {
        return headers.substr(val_start);
    }
    std::string val = headers.substr(val_start, val_end - val_start);
    // trim leading/trailing spaces
    size_t first = val.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = val.find_last_not_of(" \t");
    return val.substr(first, (last - first + 1));
}

std::string get_files_json() {
    std::stringstream ss;
    ss << "[";
    bool first = true;
    if (fs::exists("received_files")) {
        for (const auto& entry : fs::directory_iterator("received_files")) {
            if (entry.is_regular_file()) {
                if (!first) ss << ",";
                first = false;
                ss << "{\"name\":\"" << entry.path().filename().string() 
                   << "\",\"size\":" << entry.file_size() << "}";
            }
        }
    }
    ss << "]";
    return ss.str();
}

// HTTP Headers Reader Helper
bool read_http_headers(SocketType s, std::string& req_headers, std::string& body_start) {
    char buf[2048];
    std::string data = req_headers;
    size_t header_end = std::string::npos;
    
    while (header_end == std::string::npos) {
        int bytes = recv(s, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) return false;
        buf[bytes] = '\0';
        data.append(buf, bytes);
        header_end = data.find("\r\n\r\n");
        if (data.size() > 10240) return false; // Safety limit
    }
    
    req_headers = data.substr(0, header_end);
    body_start = data.substr(header_end + 4);
    return true;
}

// Client request: Send clipboard text to a peer via HTTP POST
bool send_clipboard_to_peer(const std::string& peer_ip, int peer_port, const std::string& text) {
    SocketType s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET_VAL) return false;

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip.c_str(), &addr.sin_addr);

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_VAL) {
        close_socket(s);
        return false;
    }

    set_socket_timeouts(s, 15); // 15 seconds read/write timeout

    std::stringstream http_req;
    http_req << "POST /api/clipboard HTTP/1.1\r\n"
             << "Host: " << peer_ip << ":" << peer_port << "\r\n"
             << "Content-Type: text/plain\r\n"
             << "Content-Length: " << text.size() << "\r\n"
             << "X-Device-Name: " << my_name << "\r\n";
#ifdef _WIN32
    http_req << "X-Device-OS: Windows\r\n";
#else
    http_req << "X-Device-OS: Linux\r\n";
#endif
    http_req << "Connection: close\r\n\r\n"
             << text;

    std::string req_str = http_req.str();
    bool success = send_all(s, req_str.c_str(), req_str.size());
    close_socket(s);
    return success;
}

// HTTP Server: processes routes
void handle_http_request(SocketType client_socket, const std::string& first_line, const std::string& headers, const std::string& initial_body, std::string client_ip) {
    std::stringstream ss(first_line);
    std::string method, full_path, version;
    ss >> method >> full_path >> version;

    std::string path = full_path;
    size_t q_pos = path.find('?');
    if (q_pos != std::string::npos) {
        path = path.substr(0, q_pos);
    }

    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "[HTTP] " << method << " " << path << " (from " << client_ip << ")\n";
    }

    if (method == "GET" && path == "/") {
        // Serve embedded glassmorphic html dashboard
        std::stringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html; charset=utf-8\r\n"
             << "Content-Length: " << INDEX_HTML.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << INDEX_HTML;
        send_all(client_socket, resp.str().c_str(), resp.str().size());
    } 
    else if (method == "GET" && path == "/api/status") {
        // Build response JSON
#ifdef _WIN32
        std::string host_os = "Windows";
#else
        std::string host_os = "Linux";
#endif
        std::stringstream json;
        json << "{\n"
             << "  \"my_name\": \"" << my_name << "\",\n"
             << "  \"my_ip\": \"" << my_ip << "\",\n"
             << "  \"my_port\": " << TCP_PORT << ",\n"
             << "  \"my_os\": \"" << host_os << "\",\n"
             << "  \"last_clip\": \"" << last_received_clip << "\",\n";
        
        json << "  \"peers\": [\n";
        {
            std::lock_guard<std::mutex> lock(peers_mutex);
            for (size_t i = 0; i < peers.size(); ++i) {
                json << "    {\"name\":\"" << peers[i].name << "\", \"ip\":\"" << peers[i].ip 
                     << "\", \"port\":" << peers[i].port << ", \"os\":\"" << peers[i].os << "\"}";
                if (i + 1 < peers.size()) json << ",";
                json << "\n";
            }
        }
        json << "  ],\n"
             << "  \"files\": " << get_files_json() << "\n"
             << "}";

        std::string json_str = json.str();
        std::stringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << json_str.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n\r\n"
             << json_str;
        send_all(client_socket, resp.str().c_str(), resp.str().size());
    } 
    else if (method == "POST" && (path == "/api/send_clip" || path == "/api/clipboard")) {
        // Read JSON body or raw text
        std::string body = initial_body;
        size_t content_len = 0;
        
        std::string cl_str = get_header_value(headers, "Content-Length");
        if (!cl_str.empty()) {
            content_len = std::stoul(cl_str);
        }

        while (body.size() < content_len) {
            char buf[512];
            int bytes = recv(client_socket, buf, sizeof(buf), 0);
            if (bytes <= 0) break;
            body.append(buf, bytes);
        }

        std::string text = "";
        std::string ct = get_header_value(headers, "Content-Type");

        if (ct.find("application/json") != std::string::npos) {
            // Browser sent clip JSON
            std::string peer_ip = get_json_string_field(body, "ip");
            int peer_port = get_json_int_field(body, "port");
            text = get_json_string_field(body, "text");
            bool success = send_clipboard_to_peer(peer_ip, peer_port, text);

            std::string response_payload = success ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}";
            std::stringstream resp;
            resp << "HTTP/1.1 " << (success ? "200 OK" : "500 Internal Error") << "\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Content-Length: " << response_payload.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << response_payload;
            send_all(client_socket, resp.str().c_str(), resp.str().size());
        } else {
            // Peer sent raw text clip
            text = body;
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "[HTTP Clipboard] Synced Clipboard: \"" << text << "\"\n";
                last_received_clip = text;
                clipboard_history.push_back(text);
                if (clipboard_history.size() > 10) {
                    clipboard_history.erase(clipboard_history.begin());
                }
            }
            copy_to_system_clipboard(text);

            std::string response_payload = "{\"status\":\"ok\"}";
            std::stringstream resp;
            resp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Content-Length: " << response_payload.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << response_payload;
            send_all(client_socket, resp.str().c_str(), resp.str().size());
        }
    }
    else if (method == "POST" && path == "/api/send_file") {
        // Proxy file upload directly to peer TCP connection!
        std::string peer_ip = url_decode(get_query_param(full_path, "ip"));
        int peer_port = std::stoi(get_query_param(full_path, "port"));
        std::string filename = url_decode(get_query_param(full_path, "filename"));
        uint64_t file_size = std::stoull(get_query_param(full_path, "size"));

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "[HTTP Proxy] Uploading: " << filename << " (" << file_size << " bytes) to " << peer_ip << "\n";
        }

        SocketType dest_socket = socket(AF_INET, SOCK_STREAM, 0);
        bool proxy_success = false;

        if (dest_socket != INVALID_SOCKET_VAL) {
            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(peer_port);
            inet_pton(AF_INET, peer_ip.c_str(), &addr.sin_addr);

            if (connect(dest_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR_VAL) {
                set_socket_timeouts(dest_socket, 15);
                set_socket_timeouts(client_socket, 15);
                add_active_transfer(peer_ip);
                // Send standard HTTP headers
                std::stringstream http_req;
                http_req << "POST /api/upload HTTP/1.1\r\n"
                         << "Host: " << peer_ip << ":" << peer_port << "\r\n"
                         << "X-File-Name: " << filename << "\r\n"
                         << "X-File-Size: " << file_size << "\r\n"
                         << "X-Device-Name: " << my_name << "\r\n";
#ifdef _WIN32
                http_req << "X-Device-OS: Windows\r\n";
#else
                http_req << "X-Device-OS: Linux\r\n";
#endif
                http_req << "Content-Length: " << file_size << "\r\n"
                         << "Connection: close\r\n\r\n";

                std::string headers_str = http_req.str();
                if (send_all(dest_socket, headers_str.c_str(), headers_str.size())) {
                    proxy_success = true;
                    uint64_t total_piped = 0;
                    
                    if (!initial_body.empty()) {
                        send_all(dest_socket, initial_body.data(), initial_body.size());
                        total_piped += initial_body.size();
                    }

                    std::vector<char> buffer(262144);
                    while (total_piped < file_size) {
                        size_t to_read = std::min(static_cast<uint64_t>(buffer.size()), file_size - total_piped);
                        int bytes = recv(client_socket, buffer.data(), static_cast<int>(to_read), 0);
                        if (bytes <= 0) {
                            proxy_success = false;
                            break;
                        }
                        if (!send_all(dest_socket, buffer.data(), bytes)) {
                            proxy_success = false;
                            break;
                        }
                        total_piped += bytes;
                        touch_peer(peer_ip); // Keep alive from TCP traffic
                    }

                    if (proxy_success) {
                        // Wait for remote peer HTTP response (200 OK)
                        char resp_buf[1024];
                        int r_bytes = recv(dest_socket, resp_buf, sizeof(resp_buf) - 1, 0);
                        if (r_bytes > 0) {
                            resp_buf[r_bytes] = '\0';
                            std::string peer_resp(resp_buf);
                            if (peer_resp.find("HTTP/1.1 200") == std::string::npos) {
                                proxy_success = false;
                            }
                        } else {
                            proxy_success = false;
                        }
                    }
                }
            }
            close_socket(dest_socket);
            remove_active_transfer(peer_ip);
        }

        std::string reply = proxy_success ? "{\"status\":\"ok\"}" : "{\"status\":\"failed\"}";
        std::stringstream resp;
        resp << "HTTP/1.1 " << (proxy_success ? "200 OK" : "500 Internal Error") << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << reply.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << reply;
        send_all(client_socket, resp.str().c_str(), resp.str().size());
    }
    else if (method == "POST" && path == "/api/upload") {
        // Direct local file upload from a peer
        std::string filename = get_header_value(headers, "X-File-Name");
        std::string size_str = get_header_value(headers, "X-File-Size");
        if (size_str.empty()) size_str = get_header_value(headers, "Content-Length");
        
        if (filename.empty()) {
            filename = "Uploaded_File_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        }

        // 1. Sanitize filename to prevent directory traversal
        filename = sanitize_filename(filename);

        uint64_t file_size = size_str.empty() ? 0 : std::stoull(size_str);

        // 2. Pre-flight storage space validation (returns 507 if full)
        fs::create_directories("received_files");
        try {
            auto space = fs::space("received_files");
            if (space.available < file_size) {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "[HTTP Upload] Rejected upload due to insufficient space. Needed: " 
                          << file_size << " bytes, Available: " << space.available << "\n";
                
                std::string reply = "Insufficient Storage Space";
                std::stringstream resp;
                resp << "HTTP/1.1 507 Insufficient Storage\r\n"
                     << "Content-Type: text/plain\r\n"
                     << "Content-Length: " << reply.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << reply;
                send_all(client_socket, resp.str().c_str(), resp.str().size());
                return;
            }
        } catch(...) {}

        // 3. Resolve filename conflicts (automatic duplicate numbering)
        std::string unique_filename = get_unique_filename("received_files", filename);

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "[HTTP Upload] Receiving file: " << unique_filename << " (" << (file_size / (1024.0 * 1024.0)) << " MB). Saving...\n";
        }

        add_active_transfer(client_ip);
        std::ofstream outfile("received_files/" + unique_filename, std::ios::binary);
        bool success = false;

        if (outfile.is_open()) {
            uint64_t total_received = 0;
            
            if (!initial_body.empty()) {
                outfile.write(initial_body.data(), initial_body.size());
                total_received += initial_body.size();
            }

            std::vector<char> buffer(262144);
            while (total_received < file_size) {
                size_t to_recv = std::min(static_cast<uint64_t>(buffer.size()), file_size - total_received);
                int bytes = recv(client_socket, buffer.data(), static_cast<int>(to_recv), 0);
                if (bytes <= 0) break;
                outfile.write(buffer.data(), bytes);
                total_received += bytes;
                touch_peer(client_ip); // Keep alive from TCP traffic
            }
            outfile.close();
            
            if (total_received == file_size && file_size > 0) {
                success = true;
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "[HTTP Upload] Received file " << unique_filename << " to received_files/\n";
            } else {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "[HTTP Upload] Failed receiving file " << unique_filename << " (mismatch or interrupted)\n";
                // 4. Cleanup partial temporary file
                fs::remove("received_files/" + unique_filename);
            }
        }
        remove_active_transfer(client_ip);

        std::string reply = success ? "{\"status\":\"ok\"}" : "{\"status\":\"failed\"}";
        std::stringstream resp;
        resp << "HTTP/1.1 " << (success ? "200 OK" : "500 Internal Error") << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << reply.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << reply;
        send_all(client_socket, resp.str().c_str(), resp.str().size());
    }
    else if (method == "GET" && path == "/api/download") {
        // Serve download request from received_files/
        std::string filename = url_decode(get_query_param(full_path, "file"));
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }

        std::string filepath = "received_files/" + filename;
        std::ifstream file(filepath, std::ios::binary);

        if (file.is_open()) {
            uint64_t file_size = fs::file_size(filepath);
            std::stringstream resp;
            resp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: application/octet-stream\r\n"
                 << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n"
                 << "Content-Length: " << file_size << "\r\n"
                 << "Connection: close\r\n\r\n";
            send_all(client_socket, resp.str().c_str(), resp.str().size());

            std::vector<char> buffer(262144);
            while (file) {
                file.read(buffer.data(), buffer.size());
                std::streamsize bytes = file.gcount();
                if (bytes <= 0) break;
                if (!send_all(client_socket, buffer.data(), bytes)) break;
            }
            file.close();
        } else {
            std::string err_msg = "File Not Found";
            std::stringstream resp;
            resp << "HTTP/1.1 404 Not Found\r\n"
                 << "Content-Length: " << err_msg.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << err_msg;
            send_all(client_socket, resp.str().c_str(), resp.str().size());
        }
    } 
    else {
        std::string err_msg = "Not Found";
        std::stringstream resp;
        resp << "HTTP/1.1 404 Not Found\r\n"
             << "Content-Length: " << err_msg.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << err_msg;
        send_all(client_socket, resp.str().c_str(), resp.str().size());
    }
}

// TCP Connection Handler: Standard HTTP Server (unifies protocol)
void handle_client_connection(SocketType client_socket, std::string client_ip) {
    // We read lead bytes. Since it's HTTP, we can check if it starts with standard HTTP methods
    std::string req_headers = "";
    std::string body_start = "";
    if (read_http_headers(client_socket, req_headers, body_start)) {
        size_t first_line_end = req_headers.find("\r\n");
        if (first_line_end != std::string::npos) {
            std::string first_line = req_headers.substr(0, first_line_end);
            
            // Parse pairing headers if present for manual connection reverse registration
            std::string device_name = "";
            std::string device_os = "";
            size_t name_pos = req_headers.find("X-Device-Name:");
            if (name_pos != std::string::npos) {
                size_t end_pos = req_headers.find("\r\n", name_pos);
                if (end_pos != std::string::npos) {
                    device_name = req_headers.substr(name_pos + 14, end_pos - (name_pos + 14));
                    device_name.erase(0, device_name.find_first_not_of(" \t"));
                }
            }
            size_t os_pos = req_headers.find("X-Device-OS:");
            if (os_pos != std::string::npos) {
                size_t end_pos = req_headers.find("\r\n", os_pos);
                if (end_pos != std::string::npos) {
                    device_os = req_headers.substr(os_pos + 12, end_pos - (os_pos + 12));
                    device_os.erase(0, device_os.find_first_not_of(" \t"));
                }
            }

            if (!device_name.empty() && !device_os.empty()) {
                std::lock_guard<std::mutex> lock(peers_mutex);
                bool found = false;
                for (auto& peer : peers) {
                    if (peer.ip == client_ip && peer.port == TCP_PORT) {
                        peer.name = device_name;
                        peer.os = device_os;
                        peer.last_seen = std::chrono::steady_clock::now();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    peers.push_back({device_name, client_ip, TCP_PORT, device_os, std::chrono::steady_clock::now()});
                }
            }

            handle_http_request(client_socket, first_line, req_headers, body_start, client_ip);
        }
    }
    close_socket(client_socket);
}

// TCP Server thread: listens for incoming HTTP connections
void tcp_listen_thread() {
    SocketType s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET_VAL) return;

#ifdef _WIN32
    char reuse = '1';
#else
    int reuse = 1;
#endif
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in bind_addr;
    std::memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(TCP_PORT);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR_VAL) {
        close_socket(s);
        return;
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR_VAL) {
        close_socket(s);
        return;
    }

    while (running) {
        sockaddr_in client_addr;
        SockLenType client_len = sizeof(client_addr);
        SocketType client_socket = accept(s, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_socket == INVALID_SOCKET_VAL) continue;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::string client_ip(ip_str);

        set_socket_timeouts(client_socket, 15);

        if (thread_pool) {
            thread_pool->enqueue([client_socket, client_ip] {
                handle_client_connection(client_socket, client_ip);
            });
        } else {
            std::thread t(handle_client_connection, client_socket, client_ip);
            t.detach();
        }
    }
    close_socket(s);
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "AirBridge " << AIRBRIDGE_VERSION << "\n";
        return 0;
    }

    init_sockets();
    thread_pool = new ThreadPool(8);
    
    my_name = get_device_name();
    my_ip = get_local_ip();

    std::cout << "==================================================\n";
    std::cout << "     AIRBRIDGE LOCAL HTTP SHARING (GUI)           \n";
    std::cout << "==================================================\n";
    std::cout << " Host Name:   " << my_name << "\n";
    std::cout << " Version:     " << AIRBRIDGE_VERSION << "\n";
    std::cout << " Local IP:    " << my_ip << "\n";
    std::cout << " HTTP Port:   " << TCP_PORT << "\n";
    std::cout << "==================================================\n\n";

    // Start background network threads
    std::thread t_broad(udp_broadcast_thread);
    std::thread t_listen(udp_listen_thread);
    std::thread t_prune(peer_pruning_thread);
    std::thread t_server(tcp_listen_thread);

    t_broad.detach();
    t_listen.detach();
    t_prune.detach();
    t_server.detach();

    // Auto open the web browser page dashboard
    std::string my_url = "http://localhost:" + std::to_string(TCP_PORT);
    std::cout << "[Server] Dashboard running at: " << my_url << "\n";
    std::cout << "[Server] Opening default web browser...\n";
    open_browser(my_url);

    std::cout << "\nKeep this window open to run the services.\n";
    std::cout << "Press ENTER or CTRL+C inside this console to exit.\n\n";
    
    std::string dummy;
    std::getline(std::cin, dummy);

    std::cout << "Shutting down AirBridge services...\n";
    running = false;
    if (thread_pool) {
        delete thread_pool;
        thread_pool = nullptr;
    }
    cleanup_sockets();
    return 0;
}
