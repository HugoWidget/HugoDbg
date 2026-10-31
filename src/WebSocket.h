/*
 * Copyright 2025-2026 howdy213, JYardX
 *
 * This file is part of HugoProgs.
 *
 * HugoProgs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HugoProgs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HugoProgs. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef WEB_SOCKET_H
#define WEB_SOCKET_H
#include "WinUtils/WinPch.h"

class WebSocket {
public:
    WebSocket();
    ~WebSocket();

    bool connect(const char* host, int port, const char* path);
    void close();
    bool isConnected() const { return m_connected; }
    void setConnected(bool connected) { m_connected = connected; }

    int sendText(const char* message);
    int receive(char* buffer, size_t buffer_size);
    int receiveResponse(int expected_id, char* buffer, size_t buffer_size, int timeout_ms);

private:
    UINT_PTR m_sockfd;
    bool m_connected;
};

#endif // WEB_SOCKET_H