# Frontend Build Script

将前端 JavaScript 模块打包为 C 头文件，用于在目标应用中注入执行。

## 项目结构

```
.
├── build.py                  # 主构建脚本
├── frontend/                 # 源 JS 文件目录
│   ├── pluginLoader.js       # 插件加载器
│   ├── selfcheck.js          # 自检模块
│   ├── divWindow.js          # 窗口组件
│   ├── divDialog.js          # 对话框组件
│   ├── miniConsole.js        # 迷你控制台
│   ├── vKeyboard.js          # 虚拟键盘
│   ├── main.js               # 主入口
│   ├── test.js               # 测试脚本
│   ├── clean.js              # 清理脚本
│   └── loader.js             # 加载器脚本
└── build/                    # 输出目录
    ├── scripts.h             # 最终 C 头文件
    ├── inject.json           # 注入命令列表
    ├── bundle.min.js         # 压缩后的完整脚本
    └── *.min.js              # 各模块压缩版本
```

## 构建依赖

- Python 3.6+
- [jsmin](https://pypi.org/project/jsmin/) – JavaScript 压缩
- [brotli](https://pypi.org/project/brotli/) – Brotli 压缩

安装依赖：
```bash
pip install jsmin brotli
```

## 使用方法

1. 将需要打包的 JS 文件放入 `frontend/` 目录
2. 运行构建脚本：
   ```bash
   python build.py
   ```
3. 在 `build/` 目录下得到 `scripts.h`，在 C/C++ 代码中 `#include` 即可使用

## 输出说明

`scripts.h` 包含以下常量：

| 常量名      | 类型     | 说明                             |
| ----------- | -------- | -------------------------------- |
| `test_js`   | `char*`  | 测试脚本                         |
| `clean_js`  | `char*`  | 清理脚本                         |
| `loader_js` | `char*`  | 加载器脚本                       |
| `bundle_js` | `char[]` | 注入命令数组，用于动态加载主脚本 |

## 构建流程

1. 读取 `frontend/` 下所有模块，拼接为一个完整脚本
2. 替换 `pluginLoader.js` 占位符，并包裹为 IIFE
3. 使用 `jsmin` 压缩
4. Brotli 压缩
5. 添加填充至 4 字节对齐，Z85 编码
6. 分块（每块 768 字符）并生成注入 JavaScript 命令
7. 生成 `inject.json`（调试用）和 `bundle.min.js`
8. 单独处理 `test.js`、`clean.js`、`loader.js`
9. 生成 `scripts.h` C 头文件

## 注入原理

目标应用需提供以下全局函数：
- `global.___j_(code)` – 执行解压后的脚本
- `global.___i_(code)` – 执行清理脚本

构建脚本生成的 `bundle_js` 命令数组会在运行时：
1. 拼接分块的 Z85 字符串
2. Z85 解码还原压缩数据
3. Brotli 解压
4. 调用 `___j_` 执行原始脚本
5. 清理临时变量

## 注意事项

- 所有输入 JS 文件必须使用 UTF-8 编码
- `pluginLoader.js` 中必须包含占位符 `const pluginLoaderJS = "";`，构建时会替换为实际内容
- 构建前请确保 `frontend/` 目录存在且包含所有必需文件
- 输出文件默认编码为 UTF-8

## 许可证

本脚本以 MIT License 发布，详见 [LICENSE](LICENSE) 文件