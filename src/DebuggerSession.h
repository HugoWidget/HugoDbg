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
#pragma once
#include <stdexcept>
#include <Windows.h>
#include "CDPClient.h"
#include "Debugger.h"
#include "HttpClient.h"

namespace HugoDbg {

    struct HttpClientGuard {
        HttpClientGuard() { HttpClient::init(); }
        ~HttpClientGuard() { HttpClient::cleanup(); }
    };

    class DebuggerSession {
        CDPClient* client_ = nullptr;
    public:
        explicit DebuggerSession(DWORD pid) {
            if (Debugger::connect(pid) != DEBUGGER_SUCCESS)
                throw std::runtime_error("Debugger::connect 失败");
        }
        void setClient(CDPClient* c) { client_ = c; }
        CDPClient* client() const { return client_; }
        ~DebuggerSession() {
            if (client_) { Debugger::disconnect(client_); delete client_; }
        }
        DebuggerSession(const DebuggerSession&) = delete;
        DebuggerSession& operator=(const DebuggerSession&) = delete;
    };
}