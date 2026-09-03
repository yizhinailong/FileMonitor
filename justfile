default:
    just --list

[windows]
build profile="debug":
    cmake --preset "windows-{{ profile }}"
    cmake --build --preset "windows-{{ profile }}"
    cmake -E copy_if_different "./build/windows-{{ profile }}/compile_commands.json" "./build/compile_commands.json"

[windows]
install profile="release": (build profile)
    cmake --install "./build/windows-{{ profile }}" --prefix "dist"

[windows]
run profile="debug": (build profile)
    ./build/windows-{{ profile }}/FileMonitor.exe

[linux]
build profile="debug":
    cmake --preset "linux-{{ profile }}"
    cmake --build --preset "linux-{{ profile }}"
    cmake -E copy_if_different "./build/linux-{{ profile }}/compile_commands.json" "./build/compile_commands.json"

[linux]
install profile="release": (build profile)
    cmake --install "./build/linux-{{ profile }}" --prefix "dist"

[linux]
run profile="debug": (build profile)
    ./build/linux-{{ profile }}/FileMonitor
