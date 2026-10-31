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
#ifndef STRINGS_H
#define STRINGS_H

#include <string_view>

namespace Strings {
    constexpr std::wstring_view InitFailed = L"初始化失败\n";
    constexpr std::wstring_view PipeTimeout = L"管道连接超时\n";
    constexpr std::wstring_view PipeFailed = L"管道连接失败\n";
    constexpr std::wstring_view RuntimeFailed = L"Runtime启用失败\n";
    constexpr std::wstring_view Waiting = L"请稍候...\n";
    constexpr std::wstring_view LoaderName = L"Loader";
    constexpr std::wstring_view MainName = L"Main";
    constexpr std::wstring_view CleanupName = L"Cleanup";
    constexpr std::wstring_view TestName = L"Test";
    constexpr std::wstring_view RunningMode = L"运行{}模式...\n";
    constexpr std::wstring_view MainStart = L"启动,监控目标进程...\n";
    constexpr std::wstring_view MainDetected = L"检测到目标进程 (PID: {}) - {}/{}\n";
    constexpr std::wstring_view MainLost = L"目标进程消失，重置计数\n";
    constexpr std::wstring_view MainInjecting = L"开始注入...\n";
    constexpr std::wstring_view MainDone = L"完成 - 状态: {}\n";
    constexpr std::wstring_view InjectStart = L"正在注入...\n";
    constexpr std::wstring_view InjectFailed = L"注入失败: {}\n";
    constexpr std::wstring_view OpSucceeded = L"{}成功\n";
    constexpr std::wstring_view OpFailed = L"{}失败\n";
    constexpr std::wstring_view Succeeded = L"成功";
    constexpr std::wstring_view Failed = L"失败";
}

#endif // STRINGS_H