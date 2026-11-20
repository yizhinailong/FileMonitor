-- This file is included from xmake/, so relative paths would resolve against
-- that directory instead of the project root.
local project_dir = os.projectdir()

-- `xmake install` publishes the two front ends side by side in the install
-- root, without the standard bin/ directory, so a published copy shares one
-- data/FileMonitor directory.
local install_dir = path.join(project_dir, "dist")

local function install_binary(target)
    os.mkdir(target:installdir())
    os.vcp(target:targetfile(), target:installdir())
end

local function uninstall_binary(target)
    os.tryrm(path.join(target:installdir(), path.filename(target:targetfile())))
end

-- `xmake clean` also removes the binaries published by `xmake install`.
rule("dist_clean")
    after_clean(function ()
        os.tryrm(install_dir)
    end)

target("FileMonitorCore")
    set_kind("static")
    -- Internal library: built as a dependency, but not installed by default.
    set_default(false)
    -- Public: the frontends import file_monitor.core, so its module interfaces
    -- must be visible in the module graph of every target that depends on this.
    add_files(path.join(project_dir, "src/core/**.cppm"), {public = true})
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
        add_rules("dist_clean")
        add_files(path.join(project_dir, "src/gui/*.cpp"))
        add_files(path.join(project_dir, "app.rc"))
        add_includedirs(path.join(project_dir, "src"))
        add_defines("FILE_MONITOR_VERSION=" .. PROJECT_VERSION)
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
        add_rules("dist_clean")
        add_files(path.join(project_dir, "src/tui/**.cppm"))
        add_files(path.join(project_dir, "src/tui/main.cpp"))
        add_files(path.join(project_dir, "app.rc"))
        add_includedirs(path.join(project_dir, "src"))
        add_defines("FILE_MONITOR_VERSION=" .. PROJECT_VERSION)
        add_packages("ftxui")
        -- copy_to_clipboard() uses the Win32 clipboard API.
        add_syslinks("user32")
        set_installdir(install_dir)
        on_install(install_binary)
        on_uninstall(uninstall_binary)
    target_end()
end
