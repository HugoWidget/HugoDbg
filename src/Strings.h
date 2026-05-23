#ifndef STRINGS_H
#define STRINGS_H

#include <string_view>

namespace Strings {
    constexpr std::wstring_view InitFailed = L"初始化失败\n";
    constexpr std::wstring_view PipeTimeout = L"管道连接超时\n";
    constexpr std::wstring_view PipeFailed = L"管道连接失败\n";
    constexpr std::wstring_view RuntimeFailed = L"Runtime启用失败\n";
    constexpr std::wstring_view Waiting = L"请稍候...\n";
    constexpr std::wstring_view LoaderName = L"Loader";
    constexpr std::wstring_view MainName = L"Main";
    constexpr std::wstring_view CleanupName = L"Cleanup";
    constexpr std::wstring_view TestName = L"Test";
    constexpr std::wstring_view RunningMode = L"运行{}模式...\n";
    constexpr std::wstring_view MainStart = L"启动,监控目标进程...\n";
    constexpr std::wstring_view MainDetected = L"检测到目标进程 (PID: {}) - {}/{}\n";
    constexpr std::wstring_view MainLost = L"目标进程消失，重置计数\n";
    constexpr std::wstring_view MainInjecting = L"开始注入...\n";
    constexpr std::wstring_view MainDone = L"完成 - 状态: {}\n";
    constexpr std::wstring_view InjectStart = L"正在注入...\n";
    constexpr std::wstring_view InjectFailed = L"注入失败: {}\n";
    constexpr std::wstring_view OpSucceeded = L"{}成功\n";
    constexpr std::wstring_view OpFailed = L"{}失败\n";
    constexpr std::wstring_view Succeeded = L"成功";
    constexpr std::wstring_view Failed = L"失败";
}

#endif // STRINGS_H