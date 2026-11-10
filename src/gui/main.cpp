#include <SDL3/SDL_main.h>

#include "gui/Application.hpp"

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    file_monitor::gui::Application application;
    return application.Run();
}
