# HugoDbg

> [!NOTE]
>
> 建议阅读 [HugoWidget 简介](https://github.com/HugoWidget/HugoWidget) 以了解开发情况

希沃前端调试工具，用于对希沃管家前端进行增强与功能补充，主要面向电教管理场景。

你可以在 [HugoSetup](https://github.com/HugoWidget/HugoSetup) 获取已配置版本。

## 项目介绍

HugoDbg 通过调试器注入希沃管家进程，利用 Chrome DevTools Protocol (CDP) 向目标前端注入脚本，实现以下三种模式：

- **主模式（Main）**：注入加载器（`loader_js`），随后将增强脚本拆分为多个片段依次注入并执行；成功后注入**心跳**脚本维持常驻状态，并在心跳失效时自动重注入。
- **清理模式（Cleanup）**：注入清理脚本，移除先前注入的功能；同时通过 CDP 清除心跳定时器，并删除本地的 `HugoDbg.alive` 文件。执行一次即退出。
- **测试模式（Test）**：注入测试脚本，用于功能验证。执行一次即退出。

### 心跳与常驻守护（仅 Main 模式）

- 注入成功后，向目标进程注入心跳脚本，每 **20 秒**刷新一次 `C:\Users\<用户名>\HugoDbg.alive` 文件。
- 主循环每 **5 秒**打印一次检测结果：目标进程是否存活、心跳文件上次更新时间、是否活跃。
- 心跳文件超过 **40 秒**未更新视为注入失效，进入 **40 秒**冷却后重新注入。
- 启动时若检测到已有 `HugoDbg.alive` 且活跃（例如上一次注入仍在运行），直接进入监控状态，不重复注入。
- 目标进程退出后，`injected` 状态复位；进程再次出现时会重新注入（**仍受 40 秒冷却约束**）。
- **目标进程重启检测**：主循环跟踪目标进程 PID。若 PID 发生变化（旧进程退出后立即出现新进程，且中间未观察到"进程不存在"状态），会：
  1. 输出 `目标进程 PID 变化 (旧 -> 新)，重置注入状态`；
  2. 重置稳定性计数并要求重新通过连续 3 次检测；
  3. 删除旧进程遗留的活跃心跳文件，避免新进程被误判为"已注入"；
  4. 重新注入（仍受 40 秒冷却约束）。

## 环境依赖

- Visual Studio 2022
- Windows SDK

## 编译运行

1. 克隆仓库（注意包含子模块）：

   ```bash
   git clone https://github.com/HugoWidget/HugoDbg --recursive
   ```

2. 将 `frontend` 目录复制到 `script` 目录下，使用 `build.py` 构建 `script.js`（`build` 目录下），将其放入 `src` 目录（`scripts.h` 不再上传更新，请自行使用脚本生成）。

3. 使用 Visual Studio 2022 打开解决方案 `HugoDbg.slnx`。

4. 选择配置（Debug/Release）与平台（x64），生成解决方案。

5. 运行生成的 `HugoDbg.exe`，可通过命令行参数控制行为（见下文）。

> `build.py` 负责把 `frontend/` 下的 JS 源码压缩、打包为 C++ 可直接嵌入的脚本资源（`scripts.h`），并同时导出可用于 `--override` 的 `HugoDbgJs/` 目录。

## 命令行参数

```text
HugoDbg.exe [选项]
```

### 模式选择

| 参数        | 说明                                                         |
| ----------- | ------------------------------------------------------------ |
| （默认）    | 主模式（Main）。注入 loader + 全部 bundle 片段，成功后启动心跳常驻监控。 |
| `--cleanup` | 清理模式。执行 `clean.js`、清除心跳定时器、删除 `HugoDbg.alive`，然后退出。 |
| `--test`    | 测试模式。执行 `test.js`（屏幕显示绿色 "OK"），然后退出。    |

`--cleanup` 与 `--test` 互斥；同时传入时 `--cleanup` 优先。

### 配置写入

通过 `--config key=value` 将配置写入 `hdconfig.ini`。默认位于 `C:\Users\<当前用户>`，可用 `--dir` 覆盖。

| 参数                                   | 说明                                                         |
| -------------------------------------- | ------------------------------------------------------------ |
| `--config key=value [key2=value2 ...]` | 将键值写入 `hdconfig.ini` 的 `[General]` 节；也支持重复 `--config`。 |
| `--dir <路径>`                         | 指定 `hdconfig.ini` **与** `HugoDbg.alive` 所在目录，默认 `C:\Users\<当前用户名>`。建议传入绝对路径。 |

支持别名：

- `fso` → `FullScreenOperation`
- `ss` → `ScreenSaver`
- 其他键原样写入。

取值约束：

- `fso` / `FullScreenOperation`：`assist` / `direct` / `disable`（大小写不敏感）
- `ss` / `ScreenSaver`：`true` / `false` / `1` / `0` / `on` / `off`（大小写不敏感）
- 自定义键：任意值，原样写入

行为：

- 仅写入用户显式传入的项。
- **未传入 `--config` 时不修改 `hdconfig.ini`**。
- 只更新传入的键，其他已有键保持原值。

### 脚本覆盖

| 参数         | 说明                                                         |
| ------------ | ------------------------------------------------------------ |
| `--override` | 从 exe 同目录 `HugoDbgJs\` 读取自定义脚本：`loader.js`、`test.js`、`clean.js`、`bundle_1.js`、`bundle_2.js`…；缺失项回退内置默认。 |

自定义目录结构示例：

```text
HugoDbg.exe
HugoDbgJs\
├── loader.js
├── test.js
├── clean.js
├── bundle_1.js
├── bundle_2.js
└── bundle_3.js
```

加载规则：

- `loader.js` / `test.js` / `clean.js`：文件存在且非空时覆盖内置默认；缺失或为空则保持内置默认。
- bundle：按序号递增加载（`bundle_1.js`、`bundle_2.js`…），遇到缺失或空文件即停止；一旦自定义目录下提供了 `bundle_1.js`，则内置 bundle 整体被替换为自定义集合。
- 单个 CDP 请求（含自定义 bundle 经 JSON 转义后）受 WebSocket 帧缓冲限制（约 32KB），过大的脚本会导致注入失败。

### 运行控制

| 参数                | 默认值                    | 说明                                                         |
| ------------------- | ------------------------- | ------------------------------------------------------------ |
| `--nocheck`         | 关                        | 跳过「连续 3 次检测」的要求，检测到目标进程即注入。          |
| `--interval <毫秒>` | `1000`                    | 主循环轮询间隔。传 `0` 时按 `1ms` 处理，避免忙等待。         |
| `--timeout <毫秒>`  | `10000000`（约 2h46m40s） | **Main**：从启动到首次成功注入的最长等待时间；一旦成功（或检测到已有 alive 活跃），timeout **永久失效**，进入常驻监控。**Cleanup/Test**：等待目标进程并完成单次注入的最长等待时间；注入完成即退出。 |

### 退出码

| 退出码 | 含义                                                         |
| ------ | ------------------------------------------------------------ |
| `0`    | 成功。Main：首次注入成功（或检测到 alive 活跃）；Cleanup/Test：单次注入完成且脚本执行成功。 |
| `1`    | 注入失败。Main：最近一次注入尝试失败且未达 timeout；Cleanup/Test：注入或脚本执行失败。 |
| `2`    | 命令行解析失败。                                             |
| `3`    | 等待超时。Main：从启动到首次成功注入超过 `--timeout`；Cleanup/Test：等待目标进程超过 `--timeout`。 |

### 使用示例

* 常规运行（Main，使用默认脚本，不改配置）：

  ```bash
  HugoDbg.exe
  ```

* 设置配置后启动 Main：

  ```bash
  HugoDbg.exe --config fso=direct ss=false
  ```

* 自定义配置项（混合缩写与自定义键）：

  ```bash
  HugoDbg.exe --config fso=assist ss=off LogLevel=3 MyKey=Hello
  ```

* 指定配置目录：

  ```bash
  HugoDbg.exe --dir "D:\SeewoConfig" --config fso=disable
  ```

* 使用自定义脚本目录：

  ```bash
  HugoDbg.exe --override
  ```

* 跳过稳定性检测立即注入：

  ```bash
  HugoDbg.exe --nocheck
  ```

* 测试模式（30 秒内未完成注入则退出）：

  ```bash
  HugoDbg.exe --nocheck --test --timeout 30000
  ```

* 清理模式（执行 `clean.js`、清心跳定时器、删 alive）：

  ```bash
  HugoDbg.exe --nocheck --cleanup
  ```

* 完整参数组合：

  ```bash
  HugoDbg.exe --nocheck --override --dir "D:\Cfg" --config fso=direct ss=off Foo=bar --interval 500 --timeout 600000
  ```

## 启动与运行日志

### Main 模式

- 启动时先打印心跳文件路径与初始状态。
- 若心跳超过 40 秒未刷新，会打印信息，随后进入 40 秒冷却并重新注入；冷却期内每 5 秒打印一次剩余等待时间。
- 目标进程 PID 变化时打印 `目标进程 PID 变化 (旧 -> 新)，重置注入状态`，并按需删除旧 alive 文件。

### Cleanup 模式

Cleanup 完成后，进程内的 `___hugodbg_hb_` 定时器已停止，`HugoDbg.alive` 已删除，再运行 Main 不会被旧的注入状态阻塞。

## 配置文件说明

所有外部控制配置均写入 `hdconfig.ini`（默认用户目录，`--dir` 可覆盖），使用标准 INI 格式：

```ini
[General]
FullScreenOperation=Assist
ScreenSaver=true
LogLevel=3
MyKey=Hello
```

程序仅更新 `--config` 传入的键，不触碰其他已有键。

## 项目依赖

[HugoUtils](https://github.com/HugoWidget/HugoUtils)（Hugo 系列核心库）

## 许可证

本项目采用 GPLv3 许可证，详情参见 [LICENSE](LICENSE) 文件。

- HugoUtils：[LGPLv3 许可证](licenses/LICENSE.LESSER-HugoUtils)
- WinUtils：[MIT 许可证](licenses/LICENSE-WinUtils)
- hash-library：[zlib 许可证](licenses/LICENSE-hash-library)
- swhelper：[MIT 许可证](licenses/LICENSE-swhelper)
- cpp-httplib：[MIT 许可证](licenses/LICENSE-cpp-httplib)
- mINI：[MIT 许可证](licenses/LICENSE-mINI)
- WinReg：[MIT 许可证](licenses/LICENSE-WinReg)
- libsharedmemory：[MIT 许可证](licenses/LICENSE-libsharedmemory)

## 致谢

[swhelper](https://sw.s3.fan/swhelper/introduce.html)：方案来源

## 免责声明

本项目仅用于研究或教育目的，请勿将本项目用于可能违反当地法律、侵犯著作权或其他软件 EULA 的用途。若将本项目用于非法用途，一切后果由使用者承担，开发者不承担此类行为带来的任何后果或责任。