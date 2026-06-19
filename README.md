# HugoDbg

希沃前端调试工具，用于对希沃管家前端进行增强与功能补充，主要面向电教管理场景。

## 项目介绍

HugoDbg 通过调试器注入希沃服务进程，利用 Chrome DevTools Protocol (CDP) 向目标前端注入脚本，从而实现以下模式：

- **主模式（Main）**：先注入加载器（`loader_js`），然后将增强脚本拆分为多个片段依次注入，最终加载并执行完整的增强功能。
- **清理模式（Cleanup）**：注入清理脚本，移除先前注入的功能。
- **测试模式（Test）**：注入测试脚本，用于功能验证。

程序会持续检测目标进程的出现，待进程稳定存在后自动执行注入；同时始终会向 `hdconfig.ini` 写入配置（无论是否传入相关参数），用于控制锁屏/屏保行为、检测跳过、超时控制等辅助功能。默认会将 `FullScreenOperation` 设为 `Assist`，并将 `ScreenSaver` 设为 `true`。

## 环境依赖

- Visual Studio 2022
- Windows SDK

## 编译运行

1. 克隆仓库（注意包含子模块）：

   ```bash
   git clone https://github.com/HugoWidget/HugoDbg --recursive
   ```

2. 将 `frontend` 目录复制到 `script` 目录下，使用 `build.py` 构建 `script.js`（`build` 目录下），将其放入 `src` 目录（`script.h` 不再上传更新，请自行使用脚本生成）。

3. 使用 Visual Studio 2022 打开解决方案 `HugoDbg.slnx`。

4. 选择配置（Debug/Release）与平台（x64），生成解决方案。

5. 运行生成的 `HugoDbg.exe`，可通过命令行参数控制行为（见下文）。

## 命令行参数

```
HugoDbg.exe [选项]
```

| 参数               | 说明                                                         |
| ------------------ | ------------------------------------------------------------ |
| `-nocheck`         | 跳过进程稳定性检测，只要目标进程出现即注入（默认要求连续检测到 3 次才注入，间隔时间可用`interval`参数调整）。 |
| `-cleanup`         | 运行清理模式，注入移除脚本。                                 |
| `-test`            | 运行测试模式，注入测试脚本。                                 |
| `-interval <毫秒>` | 检测目标进程的轮询间隔（默认 1000 毫秒）。                   |
| `-timeout <毫秒>`  | 最大等待时间，超时退出（默认 10,000,000 毫秒 ≈ 2.78 小时）。 |
| `-fso assist`      | 在配置文件（默认用户目录或 `-dir` 指定目录）的 `hdconfig.ini` 中写入 `FullScreenOperation=Assist`，前端不作解锁。 |
| `-fso direct`      | 写入 `FullScreenOperation=Direct`，前端解锁并自动将配置改为 `Assist`，即仅单次解锁。 |
| `-fso disable`     | 写入 `FullScreenOperation=Disable`，前端直接解锁且不修改配置，锁屏/屏保将永久禁用。 |
| `-nss`             | 向 `hdconfig.ini` 中写入 `ScreenSaver=false`，前端检测到屏保窗口（含“屏保”文字）时自动关闭。 |
| `-dir <路径>`      | 与 `-fso`、`-nss` 配合，指定 `hdconfig.ini` 的存放目录（默认为 `C:\Users\<当前用户名>`）。 |

若未指定模式，默认运行主模式。  
**注意**：即便不传入 `-fso` 或 `-nss`，程序仍会写入默认配置：  
- `FullScreenOperation=Assist`  
- `ScreenSaver=true`

### 配置说明

所有外部控制配置均写入统一的 `hdconfig.ini` 文件（默认位于用户目录下），格式为标准 INI：

```ini
[General]
FullScreenOperation=Assist
ScreenSaver=true
```

程序运行时会根据命令行参数更新对应键值，前端脚本会实时读取并响应。

### 使用示例

你可以在 [HugoSetup](https://github.com/HugoWidget/HugoSetup) 获取已配置版本。

* 常规运行（检测到目标稳定后注入主脚本，配置文件将设为默认值）：

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
  HugoDbg.exe -fso direct
  ```

* 禁止屏保（自动关闭希沃集控屏保窗口）：

  ```bash
  HugoDbg.exe -nss
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
