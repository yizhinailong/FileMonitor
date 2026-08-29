default:
    just --list

build profile="debug":
    cmake --preset "{{profile}}"
    cmake --build --preset "{{profile}}"
    cmake -E copy_if_different "./build/{{profile}}/compile_commands.json" "./build/compile_commands.json"

install profile="release": (build profile)
    cmake --install "./build/{{profile}}" --prefix "./dist"

run profile="debug": (build profile)
    ./build/{{profile}}/FileMonitor.exe
