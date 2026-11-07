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
	// ---- --help ----
	constexpr std::wstring_view HelpText =
		L"HugoDbg - SeewoServiceAssistant 调试器注入工具\n"
		L"\n"
		L"用法: HugoDbg [选项]\n"
		L"\n"
		L"模式:\n"
		L"  (无)              Main 模式：等待目标进程、注入并常驻监控，心跳失效时自动重注入\n"
		L"  --cleanup         一次性清理：运行 clean.js、清除心跳定时器并删除本地 alive 文件\n"
		L"  --test            一次性测试：运行 test.js 以验证注入链路是否正常\n"
		L"\n"
		L"选项:\n"
		L"  --help            显示本帮助并退出\n"
		L"  --nocheck         跳过多帧连续命中检测（阶段 1 首次命中即锁定目标）\n"
		L"  --interval <ms>   阶段 1 轮询间隔，默认 1000（0 会被修正为 1）\n"
		L"  --timeout  <ms>   阶段 1 等待目标进程超时时间，默认 10000000\n"
		L"  --dir <path>      配置文件与心跳文件所在目录，默认 C:\\Users\\<用户名>\n"
		L"  --override        从 exe 同目录 HugoDbgJs/ 加载自定义 JS 脚本\n"
		L"  --config k=v      写入配置到 <dir>/hdconfig.ini（可重复）\n"
		L"\n"
		L"可识别配置键（--config 使用）:\n"
		L"  fso = FullScreenOperation    取值: assist | direct | disable\n"
		L"  ss  = ScreenSaver            取值: true | false | on | off | 1 | 0\n"
		L"  其他键原样写入 [General] 节\n"
		L"\n"
		L"退出码:\n"
		L"  0  成功\n"
		L"  1  注入失败\n"
		L"  2  命令行解析失败\n"
		L"  3  阶段 1 等待目标进程超时\n";

	// ---- 通用 ----
	constexpr std::wstring_view InitFailed = L"初始化失败\n";
	constexpr std::wstring_view PipeTimeout = L"管道连接超时\n";
	constexpr std::wstring_view PipeFailed = L"管道连接失败\n";
	constexpr std::wstring_view RuntimeFailed = L"Runtime启用失败\n";
	constexpr std::wstring_view Waiting = L"请稍候...\n";

	// ---- 模式名 ----
	constexpr std::wstring_view LoaderName = L"Loader";
	constexpr std::wstring_view MainName = L"Main";
	constexpr std::wstring_view CleanupName = L"Cleanup";
	constexpr std::wstring_view TestName = L"Test";

	// ---- 注入过程（Injector 使用）----
	constexpr std::wstring_view RunningMode = L"运行{}模式...\n";
	constexpr std::wstring_view MainDone = L"完成 - 状态: {}\n";
	constexpr std::wstring_view InjectStart = L"正在注入...\n";
	constexpr std::wstring_view InjectFailed = L"注入失败: {}\n";
	constexpr std::wstring_view OpSucceeded = L"{}成功\n";
	constexpr std::wstring_view OpFailed = L"{}失败\n";
	constexpr std::wstring_view Succeeded = L"成功";
	constexpr std::wstring_view Failed = L"失败";

	// ---- 阶段 0：启动与参数 ----
	constexpr std::wstring_view MainStart = L"启动，监控目标进程...\n";
	constexpr std::wstring_view ParamLine = L"{:<14}{}\n";

	// ---- 阶段 1：等待目标进程 ----
	constexpr std::wstring_view Phase1Found = L"[阶段1] 命中目标 (PID: {}) - {}/{}\n";
	constexpr std::wstring_view Phase1Lost = L"[阶段1] 目标进程消失，重置连续计数\n";
	constexpr std::wstring_view Phase1Timeout = L"\n[阶段1] 等待目标进程超过 {:.1f} 秒，程序停止\n";

	// ---- 阶段 2：常驻监控 ----
	constexpr std::wstring_view Phase2Lost = L"[检测] 目标进程消失\n";
	constexpr std::wstring_view PidChanged = L"[检测] 目标进程 PID 变化 ({} -> {})，重置注入状态\n";
	constexpr std::wstring_view StaleAlive = L"[检测] 旧进程遗留的活跃心跳文件，已删除\n";
	constexpr std::wstring_view AlreadyActive = L"[检测] 检测到注入正在运行，继续监控\n";
	constexpr std::wstring_view InjectOk = L"[检测] 注入成功 | {}\n";
	constexpr std::wstring_view InjectRetry = L"[检测] 注入失败，将在下一次迭代重试\n";
	constexpr std::wstring_view Cooldown = L"[冷却] 距允许重试注入约 {} 秒\n";
	constexpr std::wstring_view StatusAlive = L"[检测] 目标进程存在 (PID: {}) | {}\n";
	constexpr std::wstring_view StatusGone = L"[检测] 目标进程不存在 | {}\n";

	// ---- 兼容保留（语义已由 Phase1*/Phase2* 取代，新代码不应使用）----
	constexpr std::wstring_view MainDetected = Phase1Found;
	constexpr std::wstring_view MainLost = Phase1Lost;
	constexpr std::wstring_view MainInjecting = L"开始注入...\n";
}

#endif // STRINGS_H