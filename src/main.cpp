#include <SDL3/SDL_main.h>

#include "Application.hpp"

int main(int argc, char* argv[]) {
    return file_monitor::ui::run(argc, argv);
}
