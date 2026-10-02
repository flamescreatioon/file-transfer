#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <shellapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "shell32.lib")
    typedef SOCKET SocketType;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    inline void close_socket(SocketType s) { closesocket(s); }
    inline int get_last_socket_error() { return WSAGetLastError(); }
    inline void init_sockets() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    inline void cleanup_sockets() { WSACleanup(); }
    inline void set_socket_timeouts(SocketType s, int seconds) {
        DWORD timeout = seconds * 1000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    }
    inline void open_browser(const std::string& url) {
        std::string app_arg = "--app=" + url;
        HINSTANCE res = ShellExecuteA(nullptr, "open", "msedge.exe", app_arg.c_str(), nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(res) <= 32) {
            ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <cstdlib>
    typedef int SocketType;
    #define INVALID_SOCKET_VAL -1
    #define SOCKET_ERROR_VAL -1
    inline void close_socket(SocketType s) { close(s); }
    inline int get_last_socket_error() { return errno; }
    inline void init_sockets() {}
    inline void cleanup_sockets() {}
    inline void set_socket_timeouts(SocketType s, int seconds) {
        struct timeval tv;
        tv.tv_sec = seconds;
        tv.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
    inline void open_browser(const std::string& url) {
        std::string command = "if command -v google-chrome >/dev/null 2>&1; then google-chrome --app=" + url + " & "
                              "elif command -v chromium-browser >/dev/null 2>&1; then chromium-browser --app=" + url + " & "
                              "elif command -v chromium >/dev/null 2>&1; then chromium --app=" + url + " & "
                              "else xdg-open " + url + " & fi > /dev/null 2>&1";
        std::system(command.c_str());
    }
#endif

// Port configurations
constexpr int UDP_PORT = 53535;
constexpr int TCP_PORT = 53536;

// Packet Types
constexpr uint8_t PKT_FILE_REQ   = 0x01;
constexpr uint8_t PKT_CLIPBOARD  = 0x02;
constexpr uint8_t PKT_FILE_ACC   = 0x03;
constexpr uint8_t PKT_FILE_REJ   = 0x04;

// 64-bit Network Byte Order Helpers
inline uint64_t hton64(uint64_t val) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((val & 0x00000000000000FFULL) << 56) |
            ((val & 0x000000000000FF00ULL) << 40) |
            ((val & 0x0000000000FF0000ULL) << 24) |
            ((val & 0x00000000FF000000ULL) << 8)  |
            ((val & 0x000000FF00000000ULL) >> 8)  |
            ((val & 0x0000FF0000000000ULL) >> 24) |
            ((val & 0x00FF000000000000ULL) >> 40) |
            ((val & 0xFF00000000000000ULL) >> 56));
    #else
    return val;
    #endif
}

inline uint64_t ntoh64(uint64_t val) {
    return hton64(val);
}

// Helpers to read exactly N bytes from socket
inline bool recv_all(SocketType s, char* buffer, size_t size) {
    size_t total_received = 0;
    while (total_received < size) {
        int bytes = recv(s, buffer + total_received, static_cast<int>(size - total_received), 0);
        if (bytes <= 0) {
            return false;
        }
        total_received += bytes;
    }
    return true;
}

// Helpers to send exactly N bytes to socket
inline bool send_all(SocketType s, const char* buffer, size_t size) {
    size_t total_sent = 0;
    while (total_sent < size) {
        int bytes = send(s, buffer + total_sent, static_cast<int>(size - total_sent), 0);
        if (bytes <= 0) {
            return false;
        }
        total_sent += bytes;
    }
    return true;
}

#endif // PROTOCOL_HPP
