#ifndef STRINGS_H
#define STRINGS_H

#include <string_view>

namespace Strings {
    constexpr std::string_view INIT_FAIL = "初始化失败\n";
    constexpr std::string_view PIPE_TIMEOUT = "管道连接超时\n";
    constexpr std::string_view PIPE_FAIL = "管道连接失败\n";
    constexpr std::string_view RUNTIME_FAIL = "Runtime启用失败\n";
    constexpr std::string_view WAITING = "请稍候...\n";
    constexpr std::string_view MODE_LOADER_NAME = "Loader";
    constexpr std::string_view MODE_RUNNING = "运行%s模式...\n";
    constexpr std::string_view DAEMON_START = "启动,监控目标进程...\n";
    constexpr std::string_view DAEMON_DETECTED = "检测到目标进程 (PID: %ld) - %d/%d\n";
    constexpr std::string_view DAEMON_LOST = "目标进程消失，重置计数\n";
    constexpr std::string_view DAEMON_INJECTING = "开始注入...\n";
    constexpr std::string_view DAEMON_DONE = "完成 - 状态: %s\n";
    constexpr std::string_view INJECT_START = "正在注入...\n";
    constexpr std::string_view INJECT_FAIL = "注入失败: %s\n";
    constexpr std::string_view OP_SUCCESS = "%s成功\n";
    constexpr std::string_view OP_FAIL = "%s失败\n";
    constexpr std::string_view SUCCESS = "成功";
    constexpr std::string_view FAIL = "失败";
}

#endif // STRINGS_H