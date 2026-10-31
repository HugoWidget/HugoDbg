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
#include "ScriptsLoader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include "scripts.h"

namespace HugoDbg {
    namespace fs = std::filesystem;

    inline constexpr wchar_t SCRIPTS_DIR_NAME[] = L"HugoDbgJs";

    static std::string readFileToString(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        if (!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    ScriptsBundle loadScripts(bool allowOverride) {
        ScriptsBundle s;
        s.loader_js = ::loader_js;
        s.test_js = ::test_js;
        s.clean_js = ::clean_js;
        for (const char* p : ::bundle_js) if (p) s.bundle_js.emplace_back(p);

        if (!allowOverride) return s;

        wchar_t exePath[MAX_PATH] = {};
        DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (n == 0) {
            std::wcout << L"无法获取程序路径，忽略 --override\n";
            return s;
        }

        fs::path jsDir = fs::path(std::wstring(exePath, n)).parent_path() / SCRIPTS_DIR_NAME;
        if (!fs::exists(jsDir) || !fs::is_directory(jsDir)) {
            std::wcout << L"HugoDbgJs 目录不存在，使用默认脚本\n";
            return s;
        }

        auto tryLoad = [&](const std::wstring& name, std::string& target) {
            auto p = jsDir / name;
            if (!fs::exists(p)) return;
            auto content = readFileToString(p);
            if (content.empty()) return;
            target = std::move(content);
            std::wcout << L"已加载自定义脚本: " << name << L'\n';
            };

        tryLoad(L"loader.js", s.loader_js);
        tryLoad(L"test.js", s.test_js);
        tryLoad(L"clean.js", s.clean_js);

        // 自定义 bundle：bundle_1.js, bundle_2.js ... 按序号加载；
        // 一旦发现某个序号缺失（或为空）即停止，缺项时整体使用默认内置 bundle。
        std::vector<std::string> customBundle;
        for (int i = 1; i <= 1024; ++i) {
            auto name = std::wstring(L"bundle_") + std::to_wstring(i) + L".js";
            auto p = jsDir / name;
            if (!fs::exists(p)) break;
            auto content = readFileToString(p);
            if (content.empty()) break;
            customBundle.push_back(std::move(content));
        }
        if (!customBundle.empty()) {
            s.bundle_js = std::move(customBundle);
            std::wcout << L"已加载 " << s.bundle_js.size() << L" 个自定义 bundle\n";
        }
        return s;
    }
}