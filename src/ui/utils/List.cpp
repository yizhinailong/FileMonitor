#include "List.hpp"

#include <utility>

#include <imgui.h>

namespace file_monitor::ui::utils {

    List::List(std::string_view identifier, std::vector<std::string> items)
        : m_identifier{ identifier }, m_items{ std::move(items) } {
    }

    auto List::SetItems(std::vector<std::string> items) -> void {
        m_items          = std::move(items);
        m_selected_index = std::nullopt;
    }

    auto List::AddItem(std::string_view item) -> void {
        m_items.emplace_back(item);
    }

    auto List::Clear() -> void {
        m_items.clear();
        m_selected_index = std::nullopt;
    }

    auto List::ItemCount() const -> std::size_t {
        return m_items.size();
    }

    auto List::SelectedIndex() const -> std::optional<std::size_t> {
        return m_selected_index;
    }

    auto List::Render(float width, float height) -> bool {
        if (!ImGui::BeginListBox(m_identifier.c_str(), { width, height })) {
            return false;
        }

        auto             selection_changed{ false };
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_items.size()));
        while (clipper.Step()) {
            for (auto item_index{ clipper.DisplayStart }; item_index < clipper.DisplayEnd;
                 ++item_index) {
                auto const index{ static_cast<std::size_t>(item_index) };
                ImGui::PushID(item_index);

                auto is_selected{ m_selected_index == index };
                if (ImGui::Selectable(m_items[index].c_str(), is_selected)) {
                    selection_changed = !is_selected;
                    m_selected_index  = index;
                    is_selected       = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::PopID();
            }
        }

        ImGui::EndListBox();
        return selection_changed;
    }

} // namespace file_monitor::ui::utils
