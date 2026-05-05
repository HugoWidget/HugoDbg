#include "WebSocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <time.h>

#define WS_HANDSHAKE_FORMAT "GET %s HTTP/1.1\r\n" \
                           "Host: %s:%d\r\n" \
                           "Upgrade: websocket\r\n" \
                           "Connection: Upgrade\r\n" \
                           "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" \
                           "Sec-WebSocket-Version: 13\r\n" \
                           "\r\n"

static bool safe_inet_addr(const char* host, in_addr* addr) {
    return inet_pton(AF_INET, host, addr) > 0;
}

WebSocket::WebSocket() : m_sockfd(INVALID_SOCKET), m_connected(false) {}

WebSocket::~WebSocket() {
    close();
}

bool WebSocket::connect(const char* host, int port, const char* path) {
    if (m_connected) return false;

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd == INVALID_SOCKET) return false;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    if (!safe_inet_addr(host, &server_addr.sin_addr)) {
        closesocket(m_sockfd);
        return false;
    }

    if (::connect(m_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(m_sockfd);
        return false;
    }

    char handshake[1024];
    snprintf(handshake, sizeof(handshake), WS_HANDSHAKE_FORMAT, path, host, port);
    if (send(m_sockfd, handshake, (int)strlen(handshake), 0) == SOCKET_ERROR) {
        closesocket(m_sockfd);
        return false;
    }

    char response[1024];
    int received = recv(m_sockfd, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        closesocket(m_sockfd);
        return false;
    }
    response[received] = '\0';

    if (strstr(response, "101 Switching Protocols") &&
        strstr(response, "Upgrade: websocket")) {
        m_connected = true;
        return true;
    }

    closesocket(m_sockfd);
    return false;
}

void WebSocket::close() {
    if (m_connected) {
        unsigned char close_frame[6] = { 0x88, 0x80, 0x12, 0x34, 0x56, 0x78 };
        send(m_sockfd, (char*)close_frame, 6, 0);
        closesocket(m_sockfd);
        m_connected = false;
    }
    m_sockfd = INVALID_SOCKET;
}

int WebSocket::sendText(const char* message) {
    if (!m_connected) return -1;

    size_t msg_len = strlen(message);
    unsigned char frame[8192] = { 0 };
    size_t frame_len = 0;

    frame[0] = 0x81; // FIN + text opcode

    if (msg_len < 126) {
        frame[1] = (unsigned char)(msg_len | 0x80);
        frame_len = 2;
    }
    else if (msg_len < 65536) {
        frame[1] = 126 | 0x80;
        frame[2] = (unsigned char)(msg_len >> 8);
        frame[3] = (unsigned char)(msg_len & 0xFF);
        frame_len = 4;
    }
    else {
        return -1;
    }

    unsigned char mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    memcpy(frame + frame_len, mask, 4);
    frame_len += 4;

    for (size_t i = 0; i < msg_len; i++) {
        frame[frame_len + i] = message[i] ^ mask[i % 4];
    }
    frame_len += msg_len;

    int sent = send(m_sockfd, (char*)frame, (int)frame_len, 0);
    return (sent > 0) ? 0 : -1;
}

int WebSocket::receive(char* buffer, size_t buffer_size) {
    if (!m_connected) return -1;

    unsigned char frame_header[14];
    int received = recv(m_sockfd, (char*)frame_header, 2, 0);
    if (received <= 0) return -1;

    bool masked = (frame_header[1] & 0x80) != 0;
    size_t payload_len = frame_header[1] & 0x7F;

    if (payload_len == 126) {
        received = recv(m_sockfd, (char*)frame_header + 2, 2, 0);
        if (received <= 0) return -1;
        payload_len = (frame_header[2] << 8) | frame_header[3];
    }
    else if (payload_len == 127) {
        return -1;
    }

    unsigned char mask[4] = { 0 };
    if (masked) {
        received = recv(m_sockfd, (char*)mask, 4, 0);
        if (received <= 0) return -1;
    }

    if (payload_len >= buffer_size) {
        payload_len = buffer_size - 1;
    }

    received = recv(m_sockfd, buffer, (int)payload_len, 0);
    if (received <= 0) return -1;

    if (masked) {
        for (int i = 0; i < received; i++) {
            buffer[i] ^= mask[i % 4];
        }
    }
    buffer[received] = '\0';
    return received;
}

int WebSocket::receiveResponse(int expected_id, char* buffer, size_t buffer_size, int timeout_ms) {
    if (!m_connected) return -1;

    setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));

    char temp_buffer[4096];
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < timeout_ms / 1000 + 1) {
        int received = receive(temp_buffer, sizeof(temp_buffer));
        if (received <= 0) continue;

        char id_str[32];
        snprintf(id_str, sizeof(id_str), "\"id\":%d", expected_id);
        if (strstr(temp_buffer, id_str)) {
            strncpy_s(buffer, buffer_size, temp_buffer, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return received;
        }
    }
    return -1;
}