default:
    @cmake --version
    @ninja --version
    @ccache --version

[windows]
build profile="debug":
    @cmake --preset "windows-{{ profile }}"
    @cmake --build --preset "windows-{{ profile }}"
    @cmake -E copy_if_different "./build/windows-{{ profile }}/compile_commands.json" "./build/compile_commands.json"

[windows]
publish profile="release": (build profile)
    @cmake --install "./build/windows-{{ profile }}" --prefix "dist"

[windows]
run profile="debug": (build profile)
    @./build/windows-{{ profile }}/FileMonitor.exe
