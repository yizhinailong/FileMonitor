-- FileMonitor build description for xmake.
--
-- Project-wide settings live here; the options, packages, targets and tasks
-- are in the xmake/ directory.

local project_version = "0.6.0"
-- Included scripts cannot see the locals of this file.
PROJECT_VERSION = project_version

set_xmakever("3.1.2")
set_project("FileMonitor")
set_version(project_version)
set_defaultmode("debug")
set_languages("cxx23")
set_encodings("utf-8")
set_toolchains("clang-cl")
set_runtimes(is_mode("debug") and "MTd" or "MT")
set_policy("build.c++.modules", true)

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })

includes("xmake/options.lua")
includes("xmake/packages.lua")
includes("xmake/targets.lua")
includes("xmake/tasks.lua")
