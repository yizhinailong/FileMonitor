-- `xmake install` uses the configured mode, which is debug during development.
-- This task always publishes the release binaries, whatever mode is configured.
task("publish", function()
    set_menu({
        usage = "xmake publish",
        description = "Configure, build and install the release binaries into dist.",
    })
    on_run(function()
        local xmake = os.programfile()
        os.execv(xmake, { "f", "-m", "release", "-y" })
        os.execv(xmake, { "-y" })
        os.execv(xmake, { "install", "-y" })
    end)
end)
