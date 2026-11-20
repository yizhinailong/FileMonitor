-- Some dependencies live in the local package repository under
-- xmake/xmake-repo/: nlohmann/json (pinned to a develop commit that ships the
-- module interface), Dear ImGui v1.93.0 (newer than the version xmake-repo
-- lists), and FTXUI (whose module port in xmake-repo misses sources added in
-- v7.0.3).

add_repositories("filemonitor-repo xmake/xmake-repo", { rootdir = os.projectdir() })

add_requires("spdlog v1.17.0", { configs = { header_only = false } })
add_requires("nlohmann_json 3.12.0-modules")

if has_config("gui") then
    -- The GUI only creates a window and an OpenGL context. Leave the audio,
    -- joystick, camera, GPU and renderer subsystems out of the static library;
    -- the local libsdl3 package exposes those switches.
    add_requires("libsdl3 3.4.20", {
        configs = {
            shared = false,
            audio = false,
            joystick = false,
            haptic = false,
            hidapi = false,
            sensor = false,
            camera = false,
            gpu = false,
            render = false,
            power = false,
            tray = false,
            vulkan = false,
            opengles = false,
            xinput = false,
            offscreen = false,
            dummyvideo = false,
        },
    })
    add_requires("imgui v1.93.0", { configs = { shared = false, sdl3 = true, opengl3 = true } })
end

if has_config("tui") then
    add_requires("ftxui v7.0.3", { configs = { modules = true } })
end
