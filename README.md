# FileMonitor

FileMonitor 是一个面向 Windows 的轻量级文件夹监控工具。它通过系统文件变更通知递归监控一个或多个文件夹，并在桌面界面中实时展示文件和目录的创建、删除、修改与重命名事件。

## 功能

- 同时监控多个文件夹及其子目录
- 展示事件时间、状态、文件大小和绝对路径
- 识别文件与目录的创建、删除、修改和重命名
- 将监控目录保存到 `data/config.json`
- 按日期将完整事件记录到 `data/YYYY-MM-DD.log`
- 支持高 DPI、键盘导航和中文字体

## 环境要求

- Windows 10 或 Windows 11（x64）
- CMake 4.3 或更高版本
- Ninja
- LLVM/Clang，且 `clang-cl` 可用
- Visual Studio Build Tools 和 Windows SDK
- `sccache`
- [`just`](https://github.com/casey/just)

首次配置时，CMake 会通过 CPM 自动获取 SDL3、Dear ImGui、spdlog 和 nlohmann/json，因此需要可用的 Git 和网络连接。后续构建可复用 `CPM_SOURCE_CACHE` 中的依赖缓存。

## 构建与运行

构建并运行 Debug 版本：

```powershell
just run
```

仅构建指定配置：

```powershell
just build
just build release
```

生成可分发的 Release 程序：

```powershell
just install
```

安装结果位于 `dist/FileMonitor.exe`。

不使用 `just` 时，可以直接调用 CMake：

```powershell
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
cmake --install build/release --prefix dist
```

## 使用方法

1. 启动 `FileMonitor.exe`。
2. 点击“设置”，添加需要监控的文件夹。
3. 点击“保存”开始监控。
4. 文件变更会显示在主列表中，并同步写入当天的日志文件。

配置和日志路径相对于程序的当前工作目录。通过 `just run` 启动时，它们位于仓库根目录下的 `data` 文件夹中。

## 常见问题

### 链接器无法写入 FileMonitor.exe

Windows 不允许覆盖正在运行的可执行文件。如果构建出现以下错误：

```text
lld-link: error: failed to write output 'FileMonitor.exe': permission denied
```

请先关闭正在运行的“文件监控”窗口，再重新构建。

### 未找到 PkgConfig 或 LibUSB

这是 SDL 配置阶段针对可选后端的提示，不影响本项目当前使用的 Windows 文件监控和 GPU 界面功能。

## 项目结构

```text
src/
├── core/    文件扫描、系统变更监控、配置和日志
├── ui/      SDL3、Dear ImGui 界面及 GPU 后端
└── main.cpp 程序入口
```

## 许可证

本项目基于 [MIT License](LICENSE) 发布。
