# FileMonitor

FileMonitor 是一个面向 Windows 的轻量级文件夹监控工具。它通过系统文件变更通知递归监控一个或多个文件夹，并实时展示文件和目录的创建、删除、修改、重命名与移动事件。程序提供桌面图形界面 `FileMonitor.exe` 和终端界面 `FileMonitorTui.exe`，两者监控功能相同，共用同一份配置和日志。

## 功能

- 同时监控多个文件夹及其子目录
- 可配置不监控的文件夹及其子目录
- 展示事件时间、状态、文件大小和绝对路径
- 识别文件与目录的创建、删除、修改、重命名和移动
- 将监控目录保存到程序所在目录的 `data/FileMonitor/config.json`
- 按日期将完整事件记录到程序所在目录的 `data/FileMonitor/YYYY-MM-DD.log`
- 桌面界面支持高 DPI、键盘导航和中文字体
- 终端界面支持键盘和鼠标操作，可以选中一条变更查看完整的路径和时间，并按路径和状态过滤变更，适合在命令行或远程终端中使用

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
4. 文件变更会显示在主列表中，并同步写入当天的日志文件。列表停在最底部时会跟随新的变更，向上滚动查看历史时不会被打断。

### 终端界面

1. 在 Windows Terminal、PowerShell 或命令提示符中启动 `FileMonitorTui.exe`。主界面第一行显示监控状态、文件夹数量和各类变更的数量，最后一行列出可用的按键，也可以用鼠标点击这些按键提示。
2. 按 `s` 打开设置页。“监控文件夹”和“不监控文件夹”两个列表上下排列，用 Tab 在两者之间切换，用 `↑` `↓` 选择文件夹。不存在或已包含在其他监控文件夹中的监控文件夹，以及不在任何监控文件夹中、因而不起作用的不监控文件夹，会在右侧给出提示。
3. 按 `a` 在底部输入要添加到当前列表的文件夹路径，按 Enter 添加，按 Esc 返回列表；相对路径按当前工作目录转换为绝对路径后保存。选中文件夹后按 Delete 移除。终端界面不提供系统的选择文件夹对话框。
4. 按 Ctrl+S 保存并开始监控。按 Esc 取消，有未保存的修改时需要再按一次 Esc 才会放弃。列表、输入行和底部的按键提示也可以用鼠标点击。
5. 主列表用 `↑` `↓`、PgUp、PgDn、Home、End、鼠标滚轮或鼠标点击选择变更，列表下方显示选中变更的完整时间、大小和路径，删除事件还显示文件的创建时间；按 Enter 隐藏或显示这部分详情。选中最后一行时列表跟随新的变更；选中其他行后不再跟随，底部提示期间新增了多少条变更，按 End 回到最新一行。超过 1000 条变更后最旧的变更会被移除，选中的变更和可见的行不会因此移动。列表中的时间只显示时分秒，过长的路径会省略中间部分；终端不足 80 列时隐藏大小列。按 `q` 或 Ctrl+C 退出。
6. 按 `/` 输入路径中的一段文字过滤列表，英文字母不区分大小写，按 Enter 确定，按 Esc 清除。按 `1` 到 `5` 或点击第一行的数量，可以显示或隐藏创建、删除、修改、重命名和移动事件。按 `0` 或 Esc 清除全部过滤。过滤只影响列表，日志仍记录全部变更。

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
