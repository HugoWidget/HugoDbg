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
#include "Injector.h"
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include "CDPClient.h"
#include "DebuggerSession.h"
#include "Heartbeat.h"
#include "HttpClient.h"
#include "Strings.h"
#include "WinUtils/StrConvert.h"

namespace HugoDbg {
    using namespace std;
    using namespace std::chrono_literals;
    using namespace WinUtils;

    static bool waitForDebugPort(std::string_view host, int port,
        std::chrono::seconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (HttpClient::checkPort(host.data(), port)) return true;
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    static bool executeScript(CDPClient* client, std::string_view script,
        std::wstring_view opName) {
        std::string result(4096, '\0');
        if (client->evaluate(script.data(), result.data(), result.size()) == 0) {
            std::wcout << std::format(Strings::OpSucceeded, opName)
                << ConvertString(result) << std::endl;
            return true;
        }
        std::wcout << std::format(Strings::OpFailed, opName);
        return false;
    }

    static int runModeWithLoader(CDPClient* client,
        const std::string& loader,
        std::string_view script,
        std::wstring_view modeName) {
        std::wcout << std::format(Strings::RunningMode, modeName);
        if (!executeScript(client, loader, Strings::LoaderName)) return 0;
        int success = 1;
        std::this_thread::sleep_for(20ms);
        std::wcout << Strings::Waiting;
        if (executeScript(client, script, modeName)) ++success;
        return success;
    }

    static int runMainMode(CDPClient* client,
        const std::string& loader,
        const std::vector<std::string>& bundle) {
        int success = 0;
        std::wcout << std::format(Strings::RunningMode, Strings::MainName);
        if (executeScript(client, loader, Strings::LoaderName)) {
            ++success;
            std::this_thread::sleep_for(20ms);
            std::wcout << Strings::Waiting;
            for (size_t i = 0; i < bundle.size(); ++i) {
                auto desc = std::format(L"片段 #{}/{}", i + 1, bundle.size());
                if (executeScript(client, bundle[i], desc)) ++success;
                std::this_thread::sleep_for(15ms);
            }
        }
        return success;
    }

    int injectOnce(DWORD targetPid, DbgMode mode, const ScriptsBundle& scripts) {
        HttpClientGuard httpGuard;
        std::wcout << Strings::InjectStart;

        try {
            DebuggerSession session(targetPid);

            if (!waitForDebugPort("127.0.0.1", 9229, 5s)) {
                std::wcout << Strings::PipeTimeout;
                return 1;
            }

            auto client = new CDPClient();
            if (client->connectTarget("127.0.0.1", 9229, "node") != 0) {
                delete client;
                std::wcout << Strings::PipeFailed;
                return 1;
            }
            if (client->enableRuntime() != 0) {
                delete client;
                std::wcout << Strings::RuntimeFailed;
                return 1;
            }
            session.setClient(client);

            int successCount = 0;
            switch (mode) {
            case DbgMode::Main:
                successCount = runMainMode(client, scripts.loader_js, scripts.bundle_js);
                break;

            case DbgMode::Cleanup: {
                successCount = runModeWithLoader(client, scripts.loader_js,
                    scripts.clean_js, Strings::CleanupName);

                // ---- Cleanup 语义：清理心跳 ----
                // 1) 通过 CDP 清除 Main 模式写入的 setInterval（避免 alive 被持续重建）
                // 2) 删除本地 HugoDbg.alive（让下一次 Main 能立即重新注入）
                if (successCount > 0) {
                    if (clearHeartbeat(client)) {
                        std::wcout << L"已清除心跳定时器\n";
                    }
                    else {
                        std::wcout << L"警告：清除心跳定时器失败，进程内 setInterval 可能继续写 alive 文件\n";
                    }
                }
                else {
                    std::wcout << L"clean.js 执行失败，跳过定时器清理（但仍删除本地 alive 文件）\n";
                }
                // 无论 clean 是否成功、定时器是否清除，都尝试删本地 alive：
                // 用户显式执行 cleanup，就应该让后续 Main 能重新注入。
                removeAliveFile();
                // -------------------------------
                break;
            }

            case DbgMode::Test:
                successCount = runModeWithLoader(client, scripts.loader_js,
                    scripts.test_js, Strings::TestName);
                break;
            }

            std::wcout << std::format(Strings::MainDone,
                successCount > 0 ? Strings::Succeeded : Strings::Failed);

            // 心跳只服务于 Main 模式的常驻守护
            if (mode == DbgMode::Main && successCount > 0 && !injectHeartbeat(client))
                std::wcout << L"心跳脚本注入失败，等待主循环冷却后重试\n";

            return (successCount > 0) ? 0 : 1;
        }
        catch (const std::exception& e) {
            std::wcout << std::format(Strings::InjectFailed, ConvertString(e.what()));
            // Cleanup 时如果连 CDP 都没连上，仍尝试删除本地 alive
            if (mode == DbgMode::Cleanup) removeAliveFile();
            return 1;
        }
    }
}