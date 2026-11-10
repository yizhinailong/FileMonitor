#include "tui/Application.hpp"

auto main() -> int {
    file_monitor::tui::Application application;
    return application.Run();
}
