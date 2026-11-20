-- Local nlohmann/json package.
--
-- xmake-repo only tracks release tags, and the `import nlohmann.json;` module
-- interface lives on the develop branch, so pin the same commit that the
-- previous CMake build used. Adapted from xmake-repo's nlohmann_json package
-- (xmake-repo is Apache-2.0).
package("nlohmann_json")
    set_kind("library", {headeronly = true})
    set_homepage("https://nlohmann.github.io/json/")
    set_description("JSON for Modern C++")
    set_license("MIT")

    add_urls("https://github.com/nlohmann/json.git", {alias = "git"})
    add_versions("git:3.12.0-modules", "5bad6c518bf6663b69f505511a19c98c91f77c3a")

    add_includedirs("include")

    on_install(function (package)
        io.writefile("xmake.lua", [[
            target("nlohmann_json")
                set_kind("moduleonly")
                set_languages("cxx20")
                set_policy("build.c++.modules.std", false)
                add_headerfiles("include/(nlohmann/**.hpp)")
                add_includedirs("include")
                add_files("src/modules/json.cppm", {public = true})
        ]])
        import("package.tools.xmake").install(package)
    end)
