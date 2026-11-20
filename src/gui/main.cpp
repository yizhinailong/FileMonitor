#include <SDL3/SDL_main.h>

#include "gui/Application.hpp"

int main(int /*argc*/, char* /*argv*/[])
{
    file_monitor::gui::Application application;
    return application.Run();
}
