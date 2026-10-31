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

	inline constexpr int REINJECT_COOLDOWN_SECONDS = 40;
	inline constexpr int STATUS_PRINT_INTERVAL_MS = 5000;
	inline constexpr int COOLDOWN_PRINT_INTERVAL_MS = 5000;

	static int mainProcess(bool check, std::chrono::milliseconds interval,
		std::chrono::milliseconds timeout,
		DbgMode mode, const ScriptsBundle& scripts) {
		const bool isMain = (mode == DbgMode::Main);

		std::wcout << std::format(L"check?: {}\n", check);
		std::wcout << std::format(L"interval: {}ms\n", interval.count());
		std::wcout << std::format(L"timeout:  {}ms\n", timeout.count());
		std::wcout << std::format(L"mode:     {}\n",
			mode == DbgMode::Main ? Strings::MainName :
			mode == DbgMode::Cleanup ? Strings::CleanupName : Strings::TestName);

		std::wcout << Strings::MainStart;
		int consecutive = 0;
		constexpr int required = 3;
		auto start = std::chrono::steady_clock::now();

		// 仅 Main 模式使用：标识当前是否处于"已注入/已检测到 alive"状态
		bool injected = false;
		// 仅 Main 模式使用：一旦为 true，--timeout 永久失效，进入常驻守护
		bool everInjected = false;
		auto lastInjectTime = start - std::chrono::seconds(REINJECT_COOLDOWN_SECONDS + 1);
		// PID 跟踪：用于识别目标进程重启
		DWORD lastPid = 0;
		int lastResult = TIMEOUT_NOT_INJECTED;
		auto lastStatusPrint = std::chrono::steady_clock::now();
		auto lastCooldownPrint = (std::chrono::steady_clock::time_point::min)();

		while (true) {
			auto pe32 = FindFirstProcess([](const TF(PROCESSENTRY32)& entry) {
				if (entry.szExeFile == (std::wstring)L"SeewoServiceAssistant.exe") {
					if (auto parentName = getProcessName(entry.th32ParentProcessID))
						if (*parentName == L"SeewoCore.exe") return true;
				}
				return false;
				});

			DWORD currentPid = pe32 ? pe32->th32ProcessID : 0;

			if (!pe32) {
				if (consecutive > 0) { std::wcout << Strings::MainLost; consecutive = 0; }
				if (isMain) injected = false;
				lastPid = 0;
			}
			else {
				// ===== PID 变化检测（目标进程重启）=====
				// 目标进程重启时，旧进程遗留的活跃 alive 文件对新进程无意义，
				// 必须删除，否则 isAliveFileActive() 会误判新进程"已注入"。
				if (lastPid != 0 && lastPid != currentPid) {
					std::wcout << std::format(
						L"目标进程 PID 变化 ({} -> {})，重置注入状态\n",
						lastPid, currentPid);
					consecutive = 0;
					if (isMain) {
						injected = false;
						if (isAliveFileActive()) {
							std::wcout << L"检测到旧进程遗留的活跃心跳文件，删除以避免误判\n";
							removeAliveFile();
						}
					}
				}
				lastPid = currentPid;
				// =========================================

				if (check && !injected && consecutive < required) {   // 仅默认模式、未注入时才计数打印
					++consecutive;
					std::wcout << std::format(Strings::MainDetected,
						pe32->th32ProcessID, consecutive, required);
				}

				bool shouldRun = (!check && !injected) || (consecutive >= required);
				if (shouldRun) {
					if (isMain) {
						// ========== Main：alive 检测 + 冷却重注入 ==========
						if (isAliveFileActive()) {
							if (!injected) {
								std::wcout << L"检测到注入正在运行，继续监控\n";
								injected = true;
								everInjected = true;
								lastInjectTime = std::chrono::steady_clock::now();
							}
						}
						else {
							auto now = std::chrono::steady_clock::now();
							// 冷却始终生效（含进程重启后的首次注入），与文档一致
							bool canRetry = (now - lastInjectTime) >=
								std::chrono::seconds(REINJECT_COOLDOWN_SECONDS);
							if (canRetry) {
								lastInjectTime = now;
								injected = true;
								lastResult = injectOnce(pe32->th32ProcessID, mode, scripts);
								if (lastResult == 0) {
									everInjected = true;
									std::wcout << L"[检测] 注入成功，当前心跳状态: "
										<< describeAliveStatus() << L'\n';
								}
								else {
									std::wcout << L"注入失败，将在下一次迭代重试\n";
								}
							}
							else if (!injected &&
								(now - lastCooldownPrint) >= std::chrono::milliseconds(COOLDOWN_PRINT_INTERVAL_MS)) {
								// 冷却期间即使未注入也不重试；限频打印避免刷屏
								lastCooldownPrint = now;
								auto remain = std::chrono::duration_cast<std::chrono::seconds>(
									std::chrono::seconds(REINJECT_COOLDOWN_SECONDS) - (now - lastInjectTime)).count();
								std::wcout << std::format(
									L"[冷却] 距允许重试注入约 {} 秒\n", remain);
							}
						}
						// ==================================================
					}
					else {
						// ========== Cleanup / Test：一次性注入后退出 ==========
						// 不检查 alive 文件，不写心跳；无论成功失败都只注入一次。
						lastResult = injectOnce(pe32->th32ProcessID, mode, scripts);
						return lastResult;
						// =======================================================
					}
				}
			}

			// ---------- 周期性打印（仅 Main 有意义） ----------
			if (isMain) {
				auto nowTick = std::chrono::steady_clock::now();
				if (nowTick - lastStatusPrint >= std::chrono::milliseconds(STATUS_PRINT_INTERVAL_MS)) {
					lastStatusPrint = nowTick;
					if (pe32) {
						std::wcout << std::format(L"[检测] 目标进程存在 (PID: {}) | {}\n",
							currentPid, describeAliveStatus());
					}
					else {
						std::wcout << std::format(L"[检测] 目标进程不存在 | {}\n",
							describeAliveStatus());
					}
				}
			}
			// -----------------------------------------------------

			// ---------- timeout 判定 ----------
			// Main：未 everInjected 时生效；一旦首次成功即失效，常驻
			// Cleanup / Test：等待进程 + 单次注入，注入后已 return，故超时即退出
			bool timedOut = false;
			auto elapsed = std::chrono::steady_clock::now() - start;
			if (isMain) {
				if (!everInjected && elapsed > timeout) timedOut = true;
			}
			else {
				if (elapsed > timeout) timedOut = true;
			}
			if (timedOut) {
				if (isMain) {
					std::wcout << std::format(
						L"\n等待目标进程/首次注入超过 {:.1f} 秒，程序停止\n",
						timeout.count() / 1000.0);
				}
				else {
					std::wcout << std::format(
						L"\n等待目标进程超过 {:.1f} 秒，程序停止\n",
						timeout.count() / 1000.0);
				}
				break;
			}
			// ------------------------------------

			std::this_thread::sleep_for(interval);
		}
		return lastResult;
	}
} // namespace HugoDbg

int main(int argc, char* argv[]) {
	Console console;
	console.setLocale();
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	EnsureSingleInstance(true);

	auto& core = LoggerCore::Inst();
	core.SetDefaultStrategies(GetCurrentProcessDir() + L"HugoDbg.log");
	auto emptyFmt = std::make_shared<LogFormatter>(std::vector<LogFormat>{});
	core.AddStrategy<ConsoleLogStrategy>(emptyFmt);
	core.EnableApartment(DftLogger);

	CmdParser parser;
	auto cmd = ExtractArguments(GetCommandLineW());
	if (!parser.parse(cmd)) {
		std::wcout << L"命令行解析失败：" << cmd;
		return CMD_PARSER_FAILED;
	}

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