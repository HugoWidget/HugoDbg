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
#include <Windows.h>
#include "ScriptsLoader.h"

namespace HugoDbg {
	enum class DbgMode { Main, Cleanup, Test };

	// 返回 0 表示注入成功（心跳已写入），非 0 失败
	int injectOnce(DWORD targetPid, DbgMode mode, const ScriptsBundle& scripts);
}