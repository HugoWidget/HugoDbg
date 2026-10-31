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
#include "Config.h"
#include <algorithm>
#include <cwctype>
#include <iostream>
#include "WinUtils/INI.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"

namespace HugoDbg {
    using namespace WinUtils;

    fs::path resolveConfigDir(CmdParser& parser) {
        if (auto dir = parser.getParam(L"dir", 0)) return *dir;
        return fs::path(L"C:\\Users") / GetCurrentUserName();
    }

    // 缩写 -> 全名；不在映射表中的键（自定义配置）原样返回，供后续直接写入
    std::wstring resolveConfigKey(const std::wstring& key) {
        if (key == L"fso") return L"FullScreenOperation";
        if (key == L"ss")  return L"ScreenSaver";
        return key;
    }

    static std::wstring toLower(std::wstring v) {
        std::transform(v.begin(), v.end(), v.begin(),
            [](wchar_t c) { return (wchar_t)::towlower(c); });
        return v;
    }

    std::optional<std::wstring> resolveConfigValue(const std::wstring& resolvedKey,
        const std::wstring& value) {
        if (resolvedKey == L"FullScreenOperation") {
            auto v = toLower(value);
            if (v == L"assist")  return L"Assist";
            if (v == L"direct")  return L"Direct";
            if (v == L"disable") return L"Disable";
            return std::nullopt;
        }
        if (resolvedKey == L"ScreenSaver") {
            auto v = toLower(value);
            if (v == L"true" || v == L"1" || v == L"on")  return L"true";
            if (v == L"false" || v == L"0" || v == L"off") return L"false";
            return std::nullopt;
        }
        return value;
    }

    void writeConfigValue(const fs::path& dir,
        const std::wstring& key,
        const std::wstring& value) {
        fs::path cfgPath = dir / CONFIG_FILE;
        try {
            INIFile ini(cfgPath);
            INIStructure data;
            if (fs::exists(cfgPath)) ini.read(data);
            data[CONFIG_SECTION][key] = value;
            ini.write(data, false);
        }
        catch (const std::exception& e) {
            std::wcerr << L"写入配置失败: " << ConvertString(e.what()) << L'\n';
        }
    }

    void handleConfigParams(CmdParser& parser) {
        fs::path cfgDir = resolveConfigDir(parser);   // ← 复用统一解析

        for (int i = 0; ; ++i) {
            auto param = parser.getParam(L"config", i);
            if (!param) break;

            auto eq = param->find(L'=');
            if (eq == std::wstring::npos) {
                std::wcerr << L"忽略无效配置项（缺少 '='）: " << *param << L'\n';
                continue;
            }
            std::wstring key = param->substr(0, eq);
            std::wstring value = param->substr(eq + 1);
            if (key.empty()) {
                std::wcerr << L"忽略无效配置项（空键）: " << *param << L'\n';
                continue;
            }

            std::wstring resolvedKey = resolveConfigKey(key);
            auto resolvedValue = resolveConfigValue(resolvedKey, value);
            if (!resolvedValue) {
                std::wcerr << L"忽略无效配置值: " << key << L'=' << value << L'\n';
                continue;
            }

            writeConfigValue(cfgDir, resolvedKey, *resolvedValue);
            std::wcout << L"已设置 " << resolvedKey << L" = " << *resolvedValue << L'\n';
        }
    }
}