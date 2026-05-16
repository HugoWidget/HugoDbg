#ifndef STRINGS_H
#define STRINGS_H

#include <string_view>

namespace Strings {
    constexpr std::string_view InitFailed = "初始化失败\n";
    constexpr std::string_view PipeTimeout = "管道连接超时\n";
    constexpr std::string_view PipeFailed = "管道连接失败\n";
    constexpr std::string_view RuntimeFailed = "Runtime启用失败\n";
    constexpr std::string_view Waiting = "请稍候...\n";
    constexpr std::string_view LoaderName = "Loader";
    constexpr std::string_view RunningMode = "运行%s模式...\n";
    constexpr std::string_view MainStart = "启动,监控目标进程...\n";
    constexpr std::string_view MainDetected = "检测到目标进程 (PID: %ld) - %d/%d\n";
    constexpr std::string_view MainLost = "目标进程消失，重置计数\n";
    constexpr std::string_view MainInjecting = "开始注入...\n";
    constexpr std::string_view MainDone = "完成 - 状态: %s\n";
    constexpr std::string_view InjectStart = "正在注入...\n";
    constexpr std::string_view InjectFailed = "注入失败: %s\n";
    constexpr std::string_view OpSucceeded = "%s成功\n";
    constexpr std::string_view OpFailed = "%s失败\n";
    constexpr std::string_view Succeeded = "成功";
    constexpr std::string_view Failed = "失败";
}

#endif // STRINGS_H