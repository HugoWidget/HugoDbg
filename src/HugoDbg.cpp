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
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <Windows.h>
#include <tlhelp32.h>

#include "Config.h"
#include "Heartbeat.h"
#include "Injector.h"
#include "ScriptsLoader.h"
#include "Strings.h"

#include "WinUtils/Console.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/Logger.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/WinUtils.h"

using namespace std;
using namespace WinUtils;
using namespace std::chrono_literals;

inline constexpr int CMD_PARSER_FAILED = 2;
inline constexpr int TIMEOUT_NOT_INJECTED = 3;

namespace HugoDbg {
	std::optional<std::wstring> getProcessName(DWORD pid) {
		auto pe32 = WinUtils::FindFirstProcess([pid](const TF(PROCESSENTRY32)& e) {
			return pid == e.th32ProcessID;
			});
		return pe32 ? std::optional<std::wstring>(pe32->szExeFile) : std::nullopt;
	}

	// 心跳失效后的重注入冷却（秒）
	inline constexpr int REINJECT_COOLDOWN_SECONDS = 5;
	// 常驻监控下周期性状态打印间隔
	inline constexpr int STATUS_PRINT_INTERVAL_MS = 5000;
	// 冷却提示的限频打印间隔
	inline constexpr int COOLDOWN_PRINT_INTERVAL_MS = 5000;
	// 阶段 2 常驻监控的轮询节拍，与 --interval 无关
	inline constexpr int MONITOR_INTERVAL_MS = 1000;

	static int mainProcess(bool check, std::chrono::milliseconds interval,
		std::chrono::milliseconds timeout,
		DbgMode mode, const ScriptsBundle& scripts) {
		const bool isMain = (mode == DbgMode::Main);

		// ---- 阶段 0：参数 ----
		auto printParam = [](std::wstring_view name, const auto& value) {
			std::wcout << std::format(Strings::ParamLine, name, value);
			};
		printParam(L"check:", check ? L"true" : L"false");
		printParam(L"interval:", std::format(L"{}ms", interval.count()));
		printParam(L"timeout:", std::format(L"{}ms", timeout.count()));
		printParam(L"monitor tick:", std::format(L"{}ms", MONITOR_INTERVAL_MS));
		printParam(L"mode:", mode == DbgMode::Main ? Strings::MainName
			: mode == DbgMode::Cleanup ? Strings::CleanupName
			: Strings::TestName);

		std::wcout << Strings::MainStart;

		auto findTarget = []() {
			return FindFirstProcess([](const TF(PROCESSENTRY32)& entry) {
				if (entry.szExeFile == (std::wstring)L"SeewoServiceAssistant.exe") {
					if (auto parentName = getProcessName(entry.th32ParentProcessID))
						if (*parentName == L"SeewoCore.exe") return true;
				}
				return false;
				});
			};

		auto start = std::chrono::steady_clock::now();

		// ========== 阶段 1：等待目标进程 ==========
		DWORD targetPid = 0;
		{
			constexpr int required = 3;
			int consecutive = 0;
			while (targetPid == 0) {
				auto pe32 = findTarget();
				if (pe32) {
					if (check) {
						++consecutive;
						std::wcout << std::format(Strings::Phase1Found,
							pe32->th32ProcessID, consecutive, required);
						if (consecutive >= required)
							targetPid = pe32->th32ProcessID;
					}
					else {
						targetPid = pe32->th32ProcessID;
					}
				}
				else if (consecutive > 0) {
					std::wcout << Strings::Phase1Lost;
					consecutive = 0;
				}

				if (targetPid != 0) break;

				if (std::chrono::steady_clock::now() - start > timeout) {
					std::wcout << std::format(Strings::Phase1Timeout,
						timeout.count() / 1000.0);
					return TIMEOUT_NOT_INJECTED;
				}

				std::this_thread::sleep_for(interval);
			}
		}
		// ==========================================================================

		// ========== 非 Main：一次性注入后退出 ==========
		if (!isMain) {
			return injectOnce(targetPid, mode, scripts);
		}

		// ========== Main 阶段 2：常驻监控 ==========
		bool everInjected = false;   // 仅表示"首次成功注入"是否发生过
		auto lastInjectTime = std::chrono::steady_clock::now() -
			std::chrono::seconds(REINJECT_COOLDOWN_SECONDS + 1);
		DWORD lastPid = targetPid;
		auto lastStatusPrint = std::chrono::steady_clock::now();
		auto lastCooldownPrint = std::chrono::steady_clock::now() -
			std::chrono::milliseconds(COOLDOWN_PRINT_INTERVAL_MS);

		// 常驻监控：本循环无退出条件，需 Ctrl+C 终止。
		while (true) {
			auto pe32 = findTarget();
			DWORD currentPid = pe32 ? pe32->th32ProcessID : 0;

			if (!pe32) {
				if (lastPid != 0) {
					std::wcout << Strings::Phase2Lost;
					lastPid = 0;
				}
			}
			else {
				// PID 变化：旧进程遗留的活跃 alive 对新进程无意义，删掉避免误判
				if (lastPid != 0 && lastPid != currentPid) {
					std::wcout << std::format(Strings::PidChanged,
						lastPid, currentPid);
					if (isAliveFileActive()) {
						std::wcout << Strings::StaleAlive;
						removeAliveFile();
					}
				}
				lastPid = currentPid;

				if (isAliveFileActive()) {
					if (!everInjected) {
						std::wcout << Strings::AlreadyActive;
						everInjected = true;
						lastInjectTime = std::chrono::steady_clock::now();
					}
				}
				else {
					auto now = std::chrono::steady_clock::now();
					bool canRetry = (now - lastInjectTime) >=
						std::chrono::seconds(REINJECT_COOLDOWN_SECONDS);
					if (canRetry) {
						// 注意：无论注入成功与否，都以本次调用时刻作为冷却起点，
						// 避免失败后立即重试。
						lastInjectTime = now;
						int rc = injectOnce(currentPid, mode, scripts);
						if (rc == 0) {
							everInjected = true;
							std::wcout << std::format(Strings::InjectOk,
								describeAliveStatus());
						}
						else {
							std::wcout << Strings::InjectRetry;
						}
					}
					else if (!everInjected &&
						(now - lastCooldownPrint) >= std::chrono::milliseconds(COOLDOWN_PRINT_INTERVAL_MS)) {
						lastCooldownPrint = now;
						auto remain = std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::seconds(REINJECT_COOLDOWN_SECONDS) - (now - lastInjectTime)).count();
						std::wcout << std::format(Strings::Cooldown, remain);
					}
				}
			}

			// 周期性状态打印
			auto nowTick = std::chrono::steady_clock::now();
			if (nowTick - lastStatusPrint >= std::chrono::milliseconds(STATUS_PRINT_INTERVAL_MS)) {
				lastStatusPrint = nowTick;
				if (pe32) {
					std::wcout << std::format(Strings::StatusAlive,
						currentPid, describeAliveStatus());
				}
				else {
					std::wcout << std::format(Strings::StatusGone,
						describeAliveStatus());
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_INTERVAL_MS));
		}
		// ==========================================================================
	}
} // namespace HugoDbg

int main(int argc, char* argv[]) {
	Console console;
	console.setLocale();

	CmdParser parser;
	auto cmd = ExtractArguments(GetCommandLineW());
	if (!parser.parse(cmd)) {
		std::wcout << L"命令行解析失败：" << cmd;
		return CMD_PARSER_FAILED;
	}

	if (parser.hasCommand(L"help")) {
		std::wcout << Strings::HelpText;
		return 0;
	}

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	EnsureSingleInstance(true);

	auto& core = LoggerCore::Inst();
	core.SetDefaultStrategies(GetCurrentProcessDir() + L"HugoDbg.log");
	auto emptyFmt = std::make_shared<LogFormatter>(std::vector<LogFormat>{});
	core.AddStrategy<ConsoleLogStrategy>(emptyFmt);
	core.EnableApartment(DftLogger);

	bool check = !parser.hasCommand(L"nocheck");
	HugoDbg::handleConfigParams(parser);
	HugoDbg::setAliveDir(HugoDbg::resolveConfigDir(parser));

	HugoDbg::DbgMode mode = HugoDbg::DbgMode::Main;
	if (parser.hasCommand(L"cleanup")) mode = HugoDbg::DbgMode::Cleanup;
	else if (parser.hasCommand(L"test"))    mode = HugoDbg::DbgMode::Test;

	// 心跳文件路径与初始状态只在 Main 模式下有意义
	if (mode == HugoDbg::DbgMode::Main) {
		std::wcout << L"[心跳] 检测文件: " << HugoDbg::getAliveFilePath().wstring() << L'\n';
		std::wcout << L"[心跳] 初始状态: " << HugoDbg::describeAliveStatus() << L'\n';
	}

	// --interval / --timeout 仅作用于"等待目标进程"阶段；
	auto interval = 1000ms;
	auto timeout = 10000000ms;
	if (auto p = parser.getParam(L"interval", 0))
		interval = std::chrono::milliseconds(_wtoi(p->c_str()));
	if (auto p = parser.getParam(L"timeout", 0))
		timeout = std::chrono::milliseconds(_wtoi(p->c_str()));
	// 防止 --interval 0 变成忙等待
	interval = interval.count() ? interval : 1ms;

	bool allowOverride = parser.hasCommand(L"override");
	auto scripts = HugoDbg::loadScripts(allowOverride);

	int result = HugoDbg::mainProcess(check, interval, timeout, mode, scripts);
	std::wcout << std::format(L"\n执行结束，返回码: {}\n", result);
	return result;
}