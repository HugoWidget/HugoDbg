#include "HttpClient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <time.h>

#define HTTP_REQUEST_FORMAT "GET %s HTTP/1.0\r\n" \
                           "Host: %s:%d\r\n" \
                           "Connection: close\r\n" \
                           "\r\n"

static bool safe_inet_addr(const char* host, in_addr* addr) {
    return inet_pton(AF_INET, host, addr) > 0;
}

static bool s_winsock_initialized = false;

bool HttpClient::init() {
    if (s_winsock_initialized) return true;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        s_winsock_initialized = true;
        return true;
    }
    return false;
}

void HttpClient::cleanup() {
    if (s_winsock_initialized) {
        WSACleanup();
        s_winsock_initialized = false;
    }
}

bool HttpClient::checkPort(const char* host, int port) {
    if (!s_winsock_initialized) return false;

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) return false;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);

    if (!safe_inet_addr(host, &server_addr.sin_addr)) {
        closesocket(sockfd);
        return false;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(sockfd);
        return false;
    }

    closesocket(sockfd);
    return true;
}

int HttpClient::get(const char* host, int port, const char* path, char* response, size_t response_size) {
    if (!s_winsock_initialized) return -1;

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) return -1;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);

    if (!safe_inet_addr(host, &server_addr.sin_addr)) {
        closesocket(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(sockfd);
        return -1;
    }

    char request[2048];
    snprintf(request, sizeof(request), HTTP_REQUEST_FORMAT, path, host, port);
    if (send(sockfd, request, (int)strlen(request), 0) == SOCKET_ERROR) {
        closesocket(sockfd);
        return -1;
    }

    int timeout = 2000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    size_t total_received = 0;
    memset(response, 0, response_size);
    while (total_received < response_size - 1) {
        int bytes_received = recv(sockfd, response + total_received, (int)(response_size - total_received - 1), 0);
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT && total_received > 0) break;
            else break;
        }
        else if (bytes_received == 0) {
            break;
        }
        else {
            total_received += (size_t)bytes_received;
            if (total_received >= 4) {
                char* end_marker = strstr(response, "\r\n\r\n");
                if (end_marker) {
                    char* content_length = strstr(response, "Content-Length:");
                    if (content_length) {
                        int body_length = atoi(content_length + 15);
                        int header_length = (int)(end_marker - response + 4);
                        if (total_received >= (size_t)(header_length + body_length)) break;
                    }
                    else {
                        timeout = 500;
                        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
                    }
                }
            }
        }
    }

    closesocket(sockfd);
    return (int)total_received;
}