-- FileMonitor build description for xmake.
--
-- Some dependencies live in the local package repository under thirdparty/:
-- nlohmann/json (pinned to a develop commit that ships the module interface),
-- Dear ImGui v1.93.0 (newer than the version xmake-repo lists), and FTXUI
-- (whose module port in xmake-repo misses sources added in v7.0.3).

local project_version = "0.6.0"

set_xmakever("3.1.2")
set_project("FileMonitor")
set_version(project_version)
set_languages("cxx23")
set_encodings("utf-8")
set_toolchains("clang-cl")
set_runtimes(is_mode("debug") and "MTd" or "MT")
set_policy("build.c++.modules", true)

add_repositories("filemonitor-repo thirdparty/xmake-repo", {rootdir = os.scriptdir()})

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

option("gui")
    set_default(true)
    set_showmenu(true)
    set_description("Build the SDL3 and Dear ImGui desktop application")
option_end()

option("tui")
    set_default(true)
    set_showmenu(true)
    set_description("Build the FTXUI terminal application")
option_end()

add_requires("spdlog v1.17.0", {configs = {header_only = false}})
add_requires("nlohmann_json 3.12.0-modules")

if has_config("gui") then
    add_requires("libsdl3 3.4.20", {configs = {shared = false}})
    add_requires("imgui v1.93.0", {configs = {shared = false, sdl3 = true, opengl3 = true}})
end

if has_config("tui") then
    add_requires("ftxui v7.0.3", {configs = {modules = true}})
end

-- `xmake install` publishes the two front ends side by side in the install
-- root, without the standard bin/ directory, so a published copy shares one
-- data/FileMonitor directory.
local install_dir = "dist"

local function install_binary(target)
    os.mkdir(target:installdir())
    os.vcp(target:targetfile(), target:installdir())
end

local function uninstall_binary(target)
    os.tryrm(path.join(target:installdir(), path.filename(target:targetfile())))
end

target("FileMonitorCore")
    set_kind("static")
    -- Internal library: built as a dependency, but not installed by default.
    set_default(false)
    -- Public: the frontends import file_monitor.core, so its module interfaces
    -- must be visible in the module graph of every target that depends on this.
    add_files("src/core/**.cppm", {public = true})
    -- Public: the module interface of nlohmann_json is part of this target's
    -- module graph, so targets that depend on FileMonitorCore also need the
    -- package's include directories to scan that interface.
    add_packages("nlohmann_json", "spdlog", {public = true})
    -- show_in_explorer() calls ShellExecuteW; the MSVC linker's default
    -- libraries are not part of xmake's clang-cl link line.
    add_syslinks("shell32", {public = true})
target_end()

if has_config("gui") then
    target("FileMonitor")
        set_kind("binary")
        add_deps("FileMonitorCore")
        add_files("src/gui/*.cpp", "app.rc")
        add_includedirs("src")
        add_defines("FILE_MONITOR_VERSION=" .. project_version)
        add_packages("imgui", "libsdl3")
        add_syslinks("opengl32")
        add_ldflags("/subsystem:windows", {force = true})
        set_installdir(install_dir)
        on_install(install_binary)
        on_uninstall(uninstall_binary)
    target_end()
end

if has_config("tui") then
    target("FileMonitorTui")
        set_kind("binary")
        add_deps("FileMonitorCore")
        add_files("src/tui/**.cppm", "src/tui/main.cpp", "app.rc")
        add_includedirs("src")
        add_defines("FILE_MONITOR_VERSION=" .. project_version)
        add_packages("ftxui")
        -- copy_to_clipboard() uses the Win32 clipboard API.
        add_syslinks("user32")
        set_installdir(install_dir)
        on_install(install_binary)
        on_uninstall(uninstall_binary)
    target_end()
end
