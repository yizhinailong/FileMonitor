#include <SDL3/SDL_main.h>

#include "ui/Application.hpp"

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    file_monitor::ui::Application application;
    return application.Run();
}
