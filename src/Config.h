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
#include <filesystem>
#include <optional>
#include <string>
#include "WinUtils/CmdParser.h"

namespace HugoDbg {
    namespace fs = std::filesystem;

    inline constexpr wchar_t CONFIG_FILE[] = L"hdconfig.ini";
    inline constexpr wchar_t CONFIG_SECTION[] = L"General";

    // 解析 --dir；未传入时返回默认用户目录 C:\Users\<用户名>
    // 供 handleConfigParams 与心跳文件路径共用，保证二者始终一致
    fs::path resolveConfigDir(WinUtils::CmdParser& parser);

    // 缩写 -> 全名
    std::wstring resolveConfigKey(const std::wstring& key);

    // 基于映射后的 key 校验 value；返回 nullopt 表示非法值
    std::optional<std::wstring> resolveConfigValue(const std::wstring& resolvedKey,
        const std::wstring& value);

    void writeConfigValue(const fs::path& dir,
        const std::wstring& key,
        const std::wstring& value);

    void handleConfigParams(WinUtils::CmdParser& parser);
}