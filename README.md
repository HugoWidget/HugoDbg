# HugoDbg

希沃前端调试工具，用于对希沃管家前端进行增强与功能补充，主要面向电教管理场景。

## 项目介绍

HugoDbg 通过调试器注入希沃服务进程，利用 Chrome DevTools Protocol (CDP) 向目标前端注入脚本，从而实现以下模式：

- **主模式（Main）**：加载并执行完整的增强脚本。
- **清理模式（Cleanup）**：注入清理脚本，移除先前注入的功能。
- **测试模式（Test）**：注入测试脚本，用于功能验证。

程序会持续检测目标进程的出现，待进程稳定存在后自动执行注入；同时支持外部配置（`hdconfig.ini`）控制锁屏/屏保行为、检测跳过、超时控制等辅助功能。

## 环境依赖

- Visual Studio 2022
- Windows SDK

## 编译运行

1. 克隆仓库（注意包含子模块）：

   ```bash
   git clone https://github.com/HugoWidget/HugoDbg --recursive
   ```

2. 使用 Visual Studio 2022 打开解决方案 `HugoDbg.slnx`。

3. 选择配置（Debug/Release）与平台（x64），生成解决方案。

4. 运行生成的 `HugoDbg.exe`，可通过命令行参数控制行为（见下文）。

## 命令行参数

```
HugoDbg.exe [选项]
```

| 参数                    | 说明                                                         |
| ----------------------- | ------------------------------------------------------------ |
| `-nocheck`              | 跳过进程稳定性检测，只要目标进程出现即注入（默认要求连续检测到 3 次才注入）。 |
| `-cleanup`              | 运行清理模式，注入移除脚本。                                 |
| `-test`                 | 运行测试模式，注入测试脚本。                                 |
| `-interval <毫秒>`      | 检测目标进程的轮询间隔（默认 1000 毫秒）。                   |
| `-timeout <毫秒>`       | 最大等待时间，超时退出（默认 10,000,000 毫秒 ≈ 2.78 小时）。 |
| `-lockfile fso_assist`  | 在用户目录（或 `-dir` 指定目录）下的 `hdconfig.ini` 中写入 `FullScreenOperation=Assist`，前端不作解锁。 |
| `-lockfile fso_direct`  | 写入 `FullScreenOperation=Direct`，前端解锁并自动将配置改为 `Assist`，即仅单次解锁。 |
| `-lockfile fso_disable` | 写入 `FullScreenOperation=Disable`，前端直接解锁且不修改配置，锁屏/屏保将永久禁用。 |
| `-noscreensaver`        | 向 `hdconfig.ini` 中写入 `ScreenSaver=false`，前端检测到屏保窗口（含“屏保”文字）时自动关闭。如果没有该参数，会写入`ScreenSaver=true` |
| `-dir <路径>`           | 与 `-lockfile`、`-noscreensaver` 配合，指定 `hdconfig.ini` 的存放目录（默认为 `C:\Users\<当前用户名>`）。 |

若未指定模式，默认运行主模式。

### 配置说明

所有外部控制配置均写入统一的 `hdconfig.ini` 文件（位于用户目录下），格式为标准 INI：

```ini
[General]
FullScreenOperation=Assist
ScreenSaver=false
```

C++ 程序通过 `-lockfile` 和 `-noscreensaver` 参数更新对应键值，前端脚本会实时读取并响应。

### 使用示例

你可以在 [HugoSetup](https://github.com/HugoWidget/HugoSetup) 获取已配置版本

* 常规运行（检测到目标稳定后注入主脚本）：

  ```bash
  HugoDbg.exe
  ```

* 无需稳定性检测，立即注入：

  ```bash
  HugoDbg.exe -nocheck
  ```

* 运行清理脚本：

  ```bash
  HugoDbg.exe -cleanup
  ```

* 调整轮询间隔为 500 ms，总超时 60 秒：

  ```bash
  HugoDbg.exe -interval 500 -timeout 60000
  ```

* 创建一个“直接解锁”配置（前端将在全屏时解锁并转为 Assist 状态）：

  ```bash
  HugoDbg.exe -lockfile fso_direct
  ```

* 禁止屏保（自动关闭希沃集控屏保窗口）：

  ```bash
  HugoDbg.exe -noscreensaver
  ```

* 指定配置目录：

  ```bash
  HugoDbg.exe -lockfile fso_direct -dir D:\MyConfig
  ```

## 项目依赖

[HugoUtils](https://github.com/HugoWidget/HugoUtils) (Hugo系列核心库)

## 许可证

本项目采用 GPLv3 许可证，详情参见 [LICENSE](LICENSE) 文件。

HugoUtils: [LGPLv3 许可证](licenses/LICENSE.LESSER-HugoUtils)

WinUtils:  [MIT 许可证](licenses/LICENSE-WinUtils)

hash-library: [zlib 许可证](licenses/LICENSE-hash-library)

swhelper：[MIT 许可证](licenses/LICENSE-swhelper)

cpp-httplib: [MIT 许可证](licenses/LICENSE-cpp-httplib)

mINI: [MIT 许可证](licenses/LICENSE-mINI)

WinReg: [MIT 许可证](licenses/LICENSE-WinReg)

## 免责声明

本项目仅用于研究或教育目的，请勿将本项目用于可能违反当地法律、侵犯著作权或其他软件 EULA 的用途。若将本项目用于非法用途，一切后果由使用者承担，开发者不承担此类行为带来的任何后果或责任。
