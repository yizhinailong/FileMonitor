default:
    just --list

build profile="debug":
    cmake --preset "{{profile}}"
    cmake --build --preset "{{profile}}"

run profile="debug": (build profile)
    ./build/{{profile}}/FileMonitor.exe
