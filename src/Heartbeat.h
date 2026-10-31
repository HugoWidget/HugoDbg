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
#include <string>
class CDPClient;
namespace HugoDbg {

	inline constexpr wchar_t ALIVE_FILE[] = L"HugoDbg.alive";
	inline constexpr int     ALIVE_TIMEOUT_SECONDS = 40;
	inline constexpr int     HEARTBEAT_INTERVAL_MS = 20000;

	// 设置心跳文件所在目录
	void setAliveDir(const std::filesystem::path& dir);

	std::filesystem::path getAliveFilePath();
	bool isAliveFileActive();
	std::string buildHeartbeatScript();
	bool injectHeartbeat(CDPClient* client);

	// 通过 CDP 清除注入时设置的 setInterval（Main 模式写入的 ___hugodbg_hb_）
	bool clearHeartbeat(CDPClient* client);

	// 删除本地 HugoDbg.alive 文件（无论清除定时器是否成功都可调用）
	void removeAliveFile();

	// 可打印的检测结果
	std::wstring describeAliveStatus();
}