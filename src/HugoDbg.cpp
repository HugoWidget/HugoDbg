#include "WinUtils/WinPch.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tlhelp32.h>

#include "CDPClient.h"
#include "Debugger.h"
#include "HttpClient.h"
#include "Strings.h"
#include "scripts.h"

#include "WinUtils/AWDef.h"
#include "WinUtils/Console.h"
#include "WinUtils/WinUtilsDef.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/INI.h"
#include "WinUtils/Logger.h"
using namespace std;

namespace fs = filesystem;
using namespace WinUtils;
using namespace chrono_literals;

// 配置文件固定名
constexpr wchar_t CONFIG_FILE[] = L"hdconfig.ini";
// 统一使用一个节名（前端可不关心）
constexpr wchar_t CONFIG_SECTION[] = L"General";

// HTTP 初始化/清理
struct HttpClientGuard {
	HttpClientGuard() { HttpClient::init(); }
	~HttpClientGuard() { HttpClient::cleanup(); }
};

// 调试会话：连接调试器并持有 CDPClient
class DebuggerSession {
	CDPClient* client_ = nullptr;
public:
	explicit DebuggerSession(DWORD pid) {
		if (Debugger::connect(pid) != DEBUGGER_SUCCESS)
			throw runtime_error("Debugger::connect 失败");
	}
	void setClient(CDPClient* c) { client_ = c; }
	CDPClient* client() const { return client_; }
	~DebuggerSession() {
		if (client_) {
			Debugger::disconnect(client_);
			delete client_;
		}
	}
	// 禁止拷贝
	DebuggerSession(const DebuggerSession&) = delete;
	DebuggerSession& operator=(const DebuggerSession&) = delete;
};

static optional<wstring> getProcessName(DWORD pid) {
	auto pe32 = FindFirstProcess([pid](const TF(PROCESSENTRY32)& entry) {
		return pid == entry.th32ProcessID;
		});
	return pe32 ? optional<wstring>(pe32->szExeFile) : nullopt;
}

static void writeConfigValue(const fs::path& dir,
	const std::wstring& key,
	const std::wstring& value) {
	fs::path cfgPath = dir / CONFIG_FILE;
	try {
		WinUtils::INIFile ini(cfgPath);
		WinUtils::INIStructure data;

		// 读取已有配置（文件不存在时 data 为空）
		if (fs::exists(cfgPath)) {
			ini.read(data);
		}

		// 设置目标键值
		data[CONFIG_SECTION][key] = value;

		// 写回文件
		ini.write(data, false);
	}
	catch (const std::exception& e) {
		std::wcerr << L"写入配置失败: " << ConvertString(e.what()) << L'\n';
	}
}

// 统一处理所有与配置文件相关的命令行参数
static void handleConfigParams(CmdParser& parser) {
	auto dir = parser.getParam(L"dir", 0);
	fs::path cfgDir = fs::path(L"C:\\Users") / GetCurrentUserName();
	if (dir) cfgDir = *dir;

	// fso 相关（映射为 FullScreenOperation）
	if (auto param = parser.getParam(L"fso", 0)) {
		std::wstring value;
		if (*param == L"assist")      value = L"Assist";
		else if (*param == L"direct") value = L"Direct";
		else if (*param == L"disable") value = L"Disable";
		else {
			wcerr << L"无效的 fso 参数值\n";
			goto ss;
		}
		writeConfigValue(cfgDir, L"FullScreenOperation", value);
		WuLog::Info(L"已设置 FullScreenOperation = {}", ConvertString(value));
	}
	else {// 默认值（如果用户未指定 lockfile 参数，则设置为 Assist）
		writeConfigValue(cfgDir, L"FullScreenOperation", L"Assist");
		WuLog::Info(L"已设置 FullScreenOperation = Assist");
	}
	ss:
	// -nss no screensaver
	if (parser.hasCommand(L"nss")) {
		writeConfigValue(cfgDir, L"ScreenSaver", L"false");
		WuLog::Info(L"已设置 ScreenSaver = false");
	}
	else {
		writeConfigValue(cfgDir, L"ScreenSaver", L"true");
		WuLog::Info(L"已设置 ScreenSaver = true");
	}
}

// 网络与脚本执行
static bool waitForDebugPort(string_view host, int port,
	chrono::seconds timeout) {
	auto deadline = chrono::steady_clock::now() + timeout;
	while (chrono::steady_clock::now() < deadline) {
		if (HttpClient::checkPort(host.data(), port))
			return true;
		this_thread::sleep_for(1ms);
	}
	return false;
}

static bool executeScript(CDPClient* client, string_view script,
	wstring_view opName) {
	string result(4096, '\0');
	if (client->evaluate(script.data(), result.data(), result.size()) == 0) {
		wcout << format(Strings::OpSucceeded, opName) << ConvertString(result) << endl;
		return true;
	}
	wcout << format(Strings::OpFailed, opName);
	return false;
}

// 通用模式运行
static int runModeWithLoader(CDPClient* client, string_view script,
	wstring_view modeName) {
	wcout << format(Strings::RunningMode, modeName);
	if (!executeScript(client, loader_js, Strings::LoaderName))
		return 0;

	int success = 1;
	this_thread::sleep_for(20ms);
	wcout << Strings::Waiting;
	if (executeScript(client, script, modeName))
		++success;
	return success;
}

// 三种运行模式
static int runMainMode(CDPClient* client) {
	int success = 0;
	wcout << format(Strings::RunningMode, Strings::MainName);

	if (executeScript(client, loader_js, Strings::LoaderName)) {
		++success;
		this_thread::sleep_for(20ms);

		span bundle(bundle_js);
		wcout << Strings::Waiting;
		for (size_t i = 0; i < bundle.size(); ++i) {
			auto desc = format(L"片段 #{}/{}", i + 1, bundle.size());
			if (executeScript(client, bundle[i], desc))
				++success;
			this_thread::sleep_for(15ms);
		}
	}
	return success;
}

static int runCleanupMode(CDPClient* client) {
	return runModeWithLoader(client, clean_js, Strings::CleanupName);
}

static int runTestMode(CDPClient* client) {
	return runModeWithLoader(client, test_js, Strings::TestName);
}

// 主流程
enum class DbgMode { Main, Cleanup, Test };

static int mainProcess(bool check, chrono::milliseconds interval,
	chrono::milliseconds timeout, DbgMode mode) {
	wcout << format(L"check?: {}\n", check);
	wcout << format(L"interval: {}ms\n", interval.count());
	wcout << format(L"timeout:  {}ms\n", timeout.count());
	wcout << format(L"mode:     {}\n",
		mode == DbgMode::Main ? Strings::MainName :
		mode == DbgMode::Cleanup ? Strings::CleanupName
		: Strings::TestName);

	wcout << Strings::MainStart;
	int consecutive = 0;
	constexpr int required = 3;
	DWORD targetPid = 0;
	optional<TF(PROCESSENTRY32)>pe32 = nullopt;
	auto start = chrono::steady_clock::now();
	while (true) {
		pe32 = FindFirstProcess([](const TF(PROCESSENTRY32)& entry) {
			if (entry.szExeFile == (wstring)L"SeewoServiceAssistant.exe") {
				if (auto parentName = getProcessName(entry.th32ParentProcessID))
					if (*parentName == L"SeewoCore.exe")
						return true;
			}
			return false;
			});
		if (!check && pe32) break;

		if (pe32) {
			++consecutive;
			wcout << format(Strings::MainDetected, pe32->th32ProcessID, consecutive, required);
			if (consecutive >= required) {
				wcout << Strings::MainInjecting;
				break;
			}
		}
		else {
			if (consecutive > 0) {
				wcout << Strings::MainLost;
				consecutive = 0;
			}
		}

		if (chrono::steady_clock::now() - start > timeout) {
			wcout << format(L"\n运行时间超过 {:.1f} 秒，程序停止\n",
				timeout.count() / 1000.0);
			return 0;
		}
		this_thread::sleep_for(interval);
	}
	targetPid = pe32->th32ProcessID;
	HttpClientGuard httpGuard;
	wcout << Strings::InjectStart;

	try {
		DebuggerSession session(targetPid);

		if (!waitForDebugPort("127.0.0.1", 9229, 5s)) {
			wcout << Strings::PipeTimeout;
			return 1;
		}

		auto client = new CDPClient();
		if (client->connectTarget("127.0.0.1", 9229, "node") != 0) {
			delete client;
			wcout << Strings::PipeFailed;
			return 1;
		}
		if (client->enableRuntime() != 0) {
			delete client;
			wcout << Strings::RuntimeFailed;
			return 1;
		}

		session.setClient(client);

		int successCount = 0;
		switch (mode) {
		case DbgMode::Main:    successCount = runMainMode(client);    break;
		case DbgMode::Cleanup: successCount = runCleanupMode(client); break;
		case DbgMode::Test:    successCount = runTestMode(client);    break;
		}

		wcout << format(Strings::MainDone,
			successCount > 0 ? Strings::Succeeded : Strings::Failed);
		return (successCount > 0) ? 0 : 1;
	}
	catch (const exception& e) {
		wcout << format(Strings::InjectFailed, ConvertString(e.what()));
		return 1;
	}
}

int main(int argc, char* argv[]) {
	Console console;
	console.setLocale();

	auto& core = LoggerCore::Inst();
	core.AddFileStrategy(L"HugoDbg.log");
	auto emptyFmt = std::make_shared<LogFormatter>(std::vector<LogFormat>{});
	core.AddStrategy<ConsoleLogStrategy>(emptyFmt);
	core.EnableApartment(DftLogger);

	CmdParser parser;
	auto cmd = ExtractArguments(GetCommandLineW());
	if (!parser.parse(cmd)) {
		wcout << L"命令行解析失败：" << cmd;
		return 0;
	}

	bool check = !parser.hasCommand(L"nocheck");
	handleConfigParams(parser);

	DbgMode mode = DbgMode::Main;
	if (parser.hasCommand(L"cleanup")) mode = DbgMode::Cleanup;
	else if (parser.hasCommand(L"test")) mode = DbgMode::Test;

	auto interval = 1000ms;
	auto timeout = 10000000ms;
	if (auto p = parser.getParam(L"interval", 0))
		interval = chrono::milliseconds(_wtoi(p->c_str()));
	if (auto p = parser.getParam(L"timeout", 0))
		timeout = chrono::milliseconds(_wtoi(p->c_str()));

	int result = mainProcess(check, interval, timeout, mode);
	wcout << format(L"\n执行结束，返回码: {}\n", result);
	return result;
}