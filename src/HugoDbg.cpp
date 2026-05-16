#include "WinUtils/WinPch.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <tlhelp32.h>
#include <filesystem>
#include <string>
#include <cstdlib>
#include <fstream>

#include "CDPClient.h"
#include "Debugger.h"
#include "HttpClient.h"
#include "scripts.h"

#include "WinUtils/AWDef.h"
#include "WinUtils/WinUtilsDef.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"
#include "Strings.h"
using namespace std;

namespace fs = std::filesystem;
using namespace WinUtils;
using namespace std::chrono_literals;

fs::path getHomeDirectory() {
	const char* home = nullptr;
	home = std::getenv("USERPROFILE");
	if (!home) home = std::getenv("HOMEDRIVE") && std::getenv("HOMEPATH") ?
		(std::string(std::getenv("HOMEDRIVE")) + std::getenv("HOMEPATH")).c_str() : nullptr;
	if (!home)
		throw std::runtime_error("无法获取用户主目录");
	return fs::path(home);
}

void handleLockFileParam(const std::wstring& param, wstring_view dir) {
	fs::path lockFilePath = dir.empty() ? (getHomeDirectory() / "unlock.bin") : (fs::path(dir) / "unlock.bin");

	try {
		if (param == L"create") {
			std::ofstream ofs(lockFilePath, std::ios::trunc);
			if (!ofs)
				throw std::runtime_error("无法创建文件: " + lockFilePath.string());
			ofs.close();
			std::wcout << L"已创建文件: " << lockFilePath << std::endl;
		}
		else if (param == L"delete") {
			if (fs::exists(lockFilePath)) {
				if (!fs::remove(lockFilePath))
					throw std::runtime_error("删除文件失败: " + lockFilePath.string());
				std::wcout << L"已删除文件: " << lockFilePath << std::endl;
			}
			else {
				std::wcout << L"文件不存在，无需删除: " << lockFilePath << std::endl;
			}
		}
	}
	catch (const std::exception& e) {
		std::wcerr << L"操作 lockfile 失败: " << e.what() << std::endl;
	}
}

static bool get_process_name(DWORD pid, char* process_name, size_t buffer_size) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return false;

	PROCESSENTRY32A pe32 = { .dwSize = sizeof(PROCESSENTRY32A) };
	bool found = false;

	if (Process32FirstA(hSnapshot, &pe32)) {
		do {
			if (pe32.th32ProcessID == pid) {
				snprintf(process_name, buffer_size, "%s", pe32.szExeFile);
				found = true;
				break;
			}
		} while (Process32NextA(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return found;
}

static DWORD find_process_by_name(const char* target_process, const char* parent_process) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::cerr << "创建进程快照失败" << std::endl;
		return 0;
	}

	PROCESSENTRY32A pe32 = { .dwSize = sizeof(PROCESSENTRY32A) };
	DWORD target_pid = 0;
	char parent_name[MAX_PATH];

	if (Process32FirstA(hSnapshot, &pe32)) {
		do {
			if (_stricmp(pe32.szExeFile, target_process) == 0) {
				if (get_process_name(pe32.th32ParentProcessID, parent_name, sizeof(parent_name))) {
					if (_stricmp(parent_name, parent_process) == 0) {
						target_pid = pe32.th32ProcessID;
						break;
					}
				}
			}
		} while (Process32NextA(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return target_pid;
}

// 等待调试端口可用
static bool wait_for_debug_port(const char* host, int port, int timeout_seconds) {
	for (int i = 0; i < timeout_seconds * 1000; ++i) {
		if (HttpClient::checkPort(host, port)) return true;
		std::this_thread::sleep_for(1ms);
	}
	return false;
}

// 执行 JS 脚本
static bool execute_script(CDPClient* client, const char* script_js, const char* operation_name) {
	char result[4096]{};
	if (client->evaluate(script_js, result, sizeof(result)) == 0) {
		printf(Strings::OpSucceeded.data(), operation_name);
		std::cout << result << std::endl;
		return true;
	}
	printf(Strings::OpFailed.data(), operation_name);
	return false;
}

// Loader 模式
static int run_loader_mode(CDPClient* client) {
	int success_count = 0;
	printf(Strings::RunningMode.data(), Strings::LoaderName.data());

	if (execute_script(client, loader_js, Strings::LoaderName.data())) {
		++success_count;
		std::this_thread::sleep_for(20ms);

		size_t num_commands = sizeof(bundle_js) / sizeof(bundle_js[0]);
		printf(Strings::Waiting.data());
		char description_buffer[100];

		for (size_t i = 0; i < num_commands; ++i) {
			snprintf(description_buffer, sizeof(description_buffer), "片段 #%zu/%zu", i + 1, num_commands);
			if (execute_script(client, bundle_js[i], description_buffer)) {
				++success_count;
			}
			std::this_thread::sleep_for(15ms);
		}
	}
	return success_count;
}

// RAII 辅助
struct HttpClientGuard {
	HttpClientGuard() { HttpClient::init(); }
	~HttpClientGuard() { HttpClient::cleanup(); }
};

struct DebuggerSession {
	CDPClient* client = nullptr;
	explicit DebuggerSession(DWORD pid) {
		if (Debugger::connect(pid) != DEBUGGER_SUCCESS) {
			throw std::runtime_error("Debugger::connect failed");
		}
	}
	void setClient(CDPClient* c) { client = c; }
	~DebuggerSession() {
		if (client) {
			Debugger::disconnect(client);
			delete client;
		}
	}
};

// 主逻辑
static int main_process(bool check) {
	printf(Strings::MainStart.data());
	int consecutive_detections = 0;
	const int required_detections = check * 3;
	DWORD target_pid = 0;

	while (true) {
		target_pid = find_process_by_name("SeewoServiceAssistant.exe", "SeewoCore.exe");
		if (target_pid != 0) {
			++consecutive_detections;
			printf(Strings::MainDetected.data(), target_pid, consecutive_detections, required_detections);
			if (consecutive_detections >= required_detections) {
				printf(Strings::MainInjecting.data());
				break;
			}
		}
		else {
			if (consecutive_detections > 0) {
				printf(Strings::MainLost.data());
				consecutive_detections = 0;
			}
		}
		std::this_thread::sleep_for(2s);
	}

	std::this_thread::sleep_for(1.5s);

	HttpClientGuard http_guard;

	printf(Strings::InjectStart.data());

	try {
		DebuggerSession debug_session(target_pid);
		if (!wait_for_debug_port("127.0.0.1", 9229, 5)) {
			printf(Strings::PipeTimeout.data());
			return 1;
		}

		auto client = std::make_unique<CDPClient>();
		if (client->connectTarget("127.0.0.1", 9229, "node") != 0) {
			printf(Strings::PipeFailed.data());
			return 1;
		}

		if (client->enableRuntime() != 0) {
			printf(Strings::RuntimeFailed.data());
			return 1;
		}

		debug_session.setClient(client.release());

		int success_count = run_loader_mode(debug_session.client);
		printf(Strings::MainDone.data(), success_count > 0 ? Strings::Succeeded.data() : Strings::Failed.data());

		return (success_count > 0) ? 0 : 1;
	}
	catch (const std::exception& e) {
		printf(Strings::InjectFailed.data(), e.what());
		return 1;
	}
}

int main(int argc, char* argv[]) {
	CmdParser parser;
	parser.parse(ExtractArguments(GetCommandLine()));
	bool check = !parser.hasCommand(L"nocheck");
	if (auto param = parser.getParam(L"lockfile", 0)) {
		auto dir = parser.getParam(L"dir", 0);
		wstring lockDir = fs::path("C:\\Users") / GetCurrentUserName();
		if (dir)lockDir = *dir;
		handleLockFileParam(*param, lockDir);
	}
	int result = main_process(check);
	printf("\n执行结束，返回码: %d\n", result);
	return result;
}