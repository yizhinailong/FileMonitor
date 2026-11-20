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
rule("dist_clean", function()
    after_clean(function()
        os.tryrm(install_dir)
    end)
end)

target("FileMonitorCore", function()
    set_kind("static")
    -- Internal library: built as a dependency, but not installed by default.
    set_default(false)
    -- Public: the frontends import file_monitor.core, so its module interfaces
    -- must be visible in the module graph of every target that depends on this.
    add_files(path.join(project_dir, "src/core/**.cppm"), { public = true })
    -- Public: the module interface of nlohmann_json is part of this target's
    -- module graph, so targets that depend on FileMonitorCore also need the
    -- package's include directories to scan that interface.
    add_packages("nlohmann_json", "spdlog", { public = true })
    -- show_in_explorer() calls ShellExecuteW; the MSVC linker's default
    -- libraries are not part of xmake's clang-cl link line.
    add_syslinks("shell32", { public = true })
end)

-- The two front ends share the install, module and version settings; only the
-- sources, packages, system libraries and linker flags differ.
local function define_frontend(opt)
    target(opt.name, function()
        set_kind("binary")
        add_deps("FileMonitorCore")
        add_rules("dist_clean")
        for _, file in ipairs(opt.files) do
            add_files(path.join(project_dir, file))
        end
        add_includedirs(path.join(project_dir, "src"))
        add_defines("FILE_MONITOR_VERSION=" .. PROJECT_VERSION)
        add_packages(table.unpack(opt.packages))
        for _, link in ipairs(opt.syslinks or {}) do
            add_syslinks(link)
        end
        for _, flag in ipairs(opt.ldflags or {}) do
            add_ldflags(flag, { force = true })
        end
        set_installdir(install_dir)
        on_install(install_binary)
        on_uninstall(uninstall_binary)
    end)
end

if has_config("gui") then
    define_frontend({
        name = "FileMonitor",
        files = { "src/gui/*.cpp", "app.rc" },
        packages = { "imgui", "libsdl3" },
        syslinks = { "opengl32" },
        ldflags = { "/subsystem:windows" },
    })
end

if has_config("tui") then
    define_frontend({
        name = "FileMonitorTui",
        files = { "src/tui/**.cppm", "src/tui/main.cpp", "app.rc" },
        packages = { "ftxui" },
        -- copy_to_clipboard() uses the Win32 clipboard API.
        syslinks = { "user32" },
    })
end
