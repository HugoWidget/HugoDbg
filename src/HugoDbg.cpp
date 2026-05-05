#include "WinUtils/WinPch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <tlhelp32.h>

#include "CDPClient.h"
#include "Debugger.h"
#include "HttpClient.h"
#include "scripts.h"

#include "WinUtils/AWDef.h"
#include "WinUtils/WinUtilsDef.h"
#include "Strings.h"

using namespace WinUtils;
using namespace std::chrono_literals;

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
        printf(Strings::OP_SUCCESS.data(), operation_name);
        std::cout << result << std::endl;
        return true;
    }
    printf(Strings::OP_FAIL.data(), operation_name);
    return false;
}

// Loader 模式
static int run_loader_mode(CDPClient* client) {
    int success_count = 0;
    printf(Strings::MODE_RUNNING.data(), Strings::MODE_LOADER_NAME.data());

    if (execute_script(client, loader_js, Strings::MODE_LOADER_NAME.data())) {
        ++success_count;
        std::this_thread::sleep_for(20ms);

        size_t num_commands = sizeof(bundle_js) / sizeof(bundle_js[0]);
        printf(Strings::WAITING.data());
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

// Daemon 模式主逻辑
static int run_daemon_mode() {
    printf(Strings::DAEMON_START.data());
    int consecutive_detections = 0;
    const int required_detections = 3;
    DWORD target_pid = 0;

    while (true) {
        target_pid = find_process_by_name("SeewoServiceAssistant.exe", "SeewoCore.exe");
        if (target_pid != 0) {
            ++consecutive_detections;
            printf(Strings::DAEMON_DETECTED.data(), target_pid, consecutive_detections, required_detections);
            if (consecutive_detections >= required_detections) {
                printf(Strings::DAEMON_INJECTING.data());
                break;
            }
        }
        else {
            if (consecutive_detections > 0) {
                printf(Strings::DAEMON_LOST.data());
                consecutive_detections = 0;
            }
        }
        std::this_thread::sleep_for(2s);
    }

    std::this_thread::sleep_for(3s);

    HttpClientGuard http_guard;

    printf(Strings::INJECT_START.data());

    try {
        DebuggerSession debug_session(target_pid);
        if (!wait_for_debug_port("127.0.0.1", 9229, 5)) {
            printf(Strings::PIPE_TIMEOUT.data());
            return 1;
        }

        auto client = std::make_unique<CDPClient>();
        if (client->connectTarget("127.0.0.1", 9229, "node") != 0) {
            printf(Strings::PIPE_FAIL.data());
            return 1;
        }

        if (client->enableRuntime() != 0) {
            printf(Strings::RUNTIME_FAIL.data());
            return 1;
        }

        debug_session.setClient(client.release()); // 转移所有权给 DebuggerSession

        int success_count = run_loader_mode(debug_session.client);
        printf(Strings::DAEMON_DONE.data(), success_count > 0 ? Strings::SUCCESS.data() : Strings::FAIL.data());

        return (success_count > 0) ? 0 : 1;
    }
    catch (const std::exception& e) {
        printf(Strings::INJECT_FAIL.data(), e.what());
        return 1;
    }
}

int main(int argc, char* argv[]) {
    int result = run_daemon_mode();
    printf("\n执行结束，返回码: %d\n", result);
    return result;
}