#pragma once

#include <string>
#include <string_view>

namespace file_monitor::ui::utils {

    class Button final {
    public:
        explicit Button(std::string_view label);

        auto Render(float width = 0.0F, float height = 0.0F) const -> bool;

    private:
        std::string m_label;
    };

} // namespace file_monitor::ui::utils
