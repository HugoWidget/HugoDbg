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
#ifndef CDP_CLIENT_H
#define CDP_CLIENT_H
#include "WinUtils/WinPch.h"

#include "WebSocket.h"

class CDPClient {
public:
    CDPClient();
    ~CDPClient();

    int connect(const char* host, int port);
    int connectTarget(const char* host, int port, const char* target_type);
    int enableRuntime();
    int evaluate(const char* expression, char* result, size_t result_size);

    WebSocket* getWebSocket() const { return m_ws; }
    int getNextIdAndIncrement() { return m_next_id++; }

private:
    WebSocket* m_ws;
    int m_next_id;

    // 静态辅助方法
    static bool getTargetWebSocketUrl(const char* host, int port, const char* target_type, char* ws_url, size_t url_size);
    static void extractWsInfo(const char* ws_url, char* host, int* port, char* path);
    static int parseWebSocketUrl(const char* json_response, char* ws_url, size_t url_size);
    static int extractResultValue(const char* json_response, char* value, size_t value_size);
};

#endif // CDP_CLIENT_H