#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace file_monitor::ui::utils {

    class List final {
    public:
        explicit List(std::string_view identifier, std::vector<std::string> items = {});

        auto SetItems(std::vector<std::string> items) -> void;
        auto AddItem(std::string_view item) -> void;
        auto Clear() -> void;

        [[nodiscard]] auto ItemCount() const -> std::size_t;
        [[nodiscard]] auto SelectedIndex() const -> std::optional<std::size_t>;

        auto Render(float width = 0.0F, float height = 0.0F) -> bool;

    private:
        std::string                m_identifier;
        std::vector<std::string>   m_items;
        std::optional<std::size_t> m_selected_index;
    };

} // namespace file_monitor::ui::utils
