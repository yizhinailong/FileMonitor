#pragma once

#include <string>
#include <string_view>

namespace file_monitor::ui::utils {

    class Text final {
    public:
        explicit Text(std::string_view content);

        auto SetContent(std::string_view content) -> void;
        auto Render() const -> void;

    private:
        std::string m_content;
    };

} // namespace file_monitor::ui::utils
