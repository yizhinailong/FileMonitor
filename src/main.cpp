#include <SDL3/SDL_main.h>

#include "ui/Application.hpp"

auto main(int argc, char* argv[]) -> int {
    return file_monitor::ui::run(argc, argv);
}
