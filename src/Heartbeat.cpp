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
#include "Heartbeat.h"
#include <chrono>
#include <format>
#include <iostream>
#include <system_error>
#include <Windows.h>
#include "CDPClient.h"
#include "WinUtils/WinUtils.h"

namespace HugoDbg {
    namespace fs = std::filesystem;

    // 由 main 在解析完 --dir 后设置一次，之后只读
    static fs::path& aliveDirOverride() {
        static fs::path p;
        return p;
    }

    void setAliveDir(const fs::path& dir) {
        aliveDirOverride() = dir;
    }

    fs::path getAliveFilePath() {
        const auto& override = aliveDirOverride();
        if (!override.empty()) return override / ALIVE_FILE;
        // 未设置时回退默认用户目录（与 Config.cpp 的默认值一致）
        return fs::path(L"C:\\Users") / WinUtils::GetCurrentUserName() / ALIVE_FILE;
    }

    bool isAliveFileActive() {
        try {
            auto p = getAliveFilePath();
            if (!fs::exists(p)) return false;
            auto ftime = fs::last_write_time(p);
            auto now = fs::file_time_type::clock::now();
            return (now - ftime) < std::chrono::seconds(ALIVE_TIMEOUT_SECONDS);
        }
        catch (...) { return false; }
    }

    static std::string toUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
            nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
            s.data(), len, nullptr, nullptr);
        return s;
    }

    std::string buildHeartbeatScript() {
        auto utf8Path = toUtf8(getAliveFilePath().wstring());

        std::string escaped;
        escaped.reserve(utf8Path.size() * 2);
        for (char c : utf8Path) {
            if (c == '\\')      escaped += "\\\\";
            else if (c == '\'') escaped += "\\'";
            else if (c == '\n') escaped += "\\n";
            else                escaped += c;
        }

        std::string js = "(()=>{try{const fs=require('fs');const p='";
        js += escaped;
        js += "';const w=()=>{try{fs.writeFileSync(p,String(Date.now()))}catch(e){}};"
            "w();const g=globalThis;"
            "if(g.___hugodbg_hb_)clearInterval(g.___hugodbg_hb_);"
            "g.___hugodbg_hb_=setInterval(w,";
        js += std::to_string(HEARTBEAT_INTERVAL_MS);
        js += ")}catch(e){}})()";
        return js;
    }

    bool injectHeartbeat(CDPClient* client) {
        std::string js = buildHeartbeatScript();
        char result[256] = {};
        return client->evaluate(js.c_str(), result, sizeof(result)) == 0;
    }

    bool clearHeartbeat(CDPClient* client) {
        const char* js =
            "(()=>{try{"
            "const g=globalThis;"
            "if(g.___hugodbg_hb_){clearInterval(g.___hugodbg_hb_);g.___hugodbg_hb_=undefined;return 'cleared';}"
            "return 'absent';"
            "}catch(e){return 'err:'+e}})()";
        char result[128] = {};
        if (client->evaluate(js, result, sizeof(result)) != 0) return false;
        return true;
    }

    void removeAliveFile() {
        try {
            auto p = getAliveFilePath();
            std::error_code ec;
            if (!fs::exists(p, ec)) {
                std::wcout << L"心跳文件不存在，无需删除: " << p.wstring() << L'\n';
                return;
            }
            bool removed = fs::remove(p, ec);
            if (!ec && removed) {
                std::wcout << L"已删除心跳文件: " << p.wstring() << L'\n';
            }
            else {
                std::wcout << L"警告：删除心跳文件失败: " << p.wstring() << L'\n';
            }
        }
        catch (...) {
            std::wcout << L"警告：删除心跳文件异常\n";
        }
    }

    std::wstring describeAliveStatus() {
        try {
            auto p = getAliveFilePath();
            if (!fs::exists(p)) {
                return std::format(L"心跳文件不存在: {}", p.wstring());
            }
            auto ftime = fs::last_write_time(p);
            auto now = fs::file_time_type::clock::now();
            auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - ftime).count();
            if (ageMs < 0) ageMs = 0;

            bool active = ageMs < (long long)ALIVE_TIMEOUT_SECONDS * 1000;
            return std::format(
                L"心跳文件: {} | 最后更新: {:.1f} 秒前 | 状态: {}",
                p.wstring(),
                ageMs / 1000.0,
                active ? L"活跃（注入有效）" : L"已失效（将触发重注入）");
        }
        catch (const std::exception&) {
            return L"心跳检测异常";
        }
    }
}