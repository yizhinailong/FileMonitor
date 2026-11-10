# FileMonitor

FileMonitor 是一个面向 Windows 的轻量级文件夹监控工具。它通过系统文件变更通知递归监控一个或多个文件夹，并实时展示文件和目录的创建、删除、修改、重命名与移动事件。程序提供桌面图形界面 `FileMonitor.exe` 和终端界面 `FileMonitorTui.exe`，两者功能一致。

## 功能

- 同时监控多个文件夹及其子目录
- 可配置不监控的文件夹及其子目录
- 展示事件时间、状态、文件大小和绝对路径
- 识别文件与目录的创建、删除、修改、重命名和移动
- 将监控目录保存到程序所在目录的 `data/FileMonitor/config.json`
- 按日期将完整事件记录到程序所在目录的 `data/FileMonitor/YYYY-MM-DD.log`
- 桌面界面支持高 DPI、键盘导航和中文字体
- 终端界面支持键盘和鼠标操作，适合在命令行或远程终端中使用

## 环境要求

- Windows 10 或 Windows 11（x64）
- CMake 4.3 或更高版本
- Ninja
- LLVM/Clang，且 `clang-cl` 可用
- Visual Studio Build Tools 和 Windows SDK
- `ccache`
- [`just`](https://github.com/casey/just)

首次配置时，CMake 会通过 CPM 自动获取 SDL3、Dear ImGui、FTXUI、spdlog 和 nlohmann/json，因此需要可用的 Git 和网络连接。后续构建可复用 `CPM_SOURCE_CACHE` 中的依赖缓存。

## 构建与运行

构建并运行 Debug 版本的桌面界面或终端界面：

```powershell
just run
just run-tui
```

仅构建指定配置：

```powershell
just build
just build release
```

生成可分发的 Release 程序：

```powershell
just publish
```

发布结果位于 `dist/FileMonitor.exe` 和 `dist/FileMonitorTui.exe`。

清理构建目录 `build/` 和发布目录 `dist/`：

```powershell
just clean
```

清理后下次构建会重新配置并完整编译，依赖仍从 `CPM_SOURCE_CACHE` 缓存中读取。

不使用 `just` 时，可以直接调用 CMake：

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug

cmake --preset windows-release
cmake --build --preset windows-release
cmake --install build/windows-release --prefix dist
```

## 使用方法

### 桌面界面

1. 启动 `FileMonitor.exe`。
2. 点击“设置”，在对应标签页中添加监控文件夹和不监控文件夹。
3. 点击“保存”开始监控。
4. 文件变更会显示在主列表中，并同步写入当天的日志文件。

### 终端界面

1. 在 Windows Terminal、PowerShell 或命令提示符中启动 `FileMonitorTui.exe`。
2. 按 `s` 或选中“设置”后按 Enter 打开设置页。焦点位于顶部标签栏时，用 `←` `→` 切换“监控文件夹”和“不监控文件夹”标签页。
3. 输入文件夹路径后按 Enter 或“添加”；相对路径按当前工作目录转换为绝对路径后保存。在列表中选中文件夹后按 Delete 或“删除”移除。终端界面不提供系统的选择文件夹对话框。
4. 选择“保存”开始监控，按 Esc 或“取消”放弃修改。用 Tab 或方向键在控件之间移动焦点。
5. 主列表用 `↑` `↓`、PgUp、PgDn、Home、End 或鼠标滚轮滚动，用 `←` `→` 横向滚动路径列；有新变更时自动滚动到最新一行。时间列只显示变更发生的时间，删除事件的创建时间记录在日志中。按 `q` 或 Ctrl+C 退出。

监控、配置或日志出错时，两个界面都会在主界面顶部显示错误信息。

配置和日志保存在程序所在目录的 `data/FileMonitor` 文件夹中，与启动时的工作目录无关；同一目录下的 `FileMonitor.exe` 和 `FileMonitorTui.exe` 共用这份配置和日志。该文件夹始终不会被监控，避免写日志时又产生新的变更事件。通过 `just run` 或 `just run-tui` 启动时，它位于 `build/windows-debug/data/FileMonitor`，执行 `just clean` 会一并删除。

## 常见问题

### 链接器无法写入 FileMonitor.exe

Windows 不允许覆盖正在运行的可执行文件。如果构建出现以下错误：

```text
lld-link: error: failed to write output 'FileMonitor.exe': permission denied
```

请先关闭正在运行的“文件监控”窗口或终端界面，再重新构建。

### 未找到 PkgConfig

这是 SDL 配置阶段针对可选后端的提示，不影响本项目当前使用的 Windows 文件监控和 OpenGL 界面功能。

## 项目结构

```text
src/
├── core/ 文件扫描、系统变更监控、配置、日志和监控会话
├── gui/  SDL3、Dear ImGui 桌面界面及 OpenGL3 后端
└── tui/  FTXUI 终端界面
```

## 许可证

本项目基于 [MIT License](LICENSE) 发布。
