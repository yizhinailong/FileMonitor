export module file_monitor.tui:change_list;

import std;
import file_monitor.core;
import ftxui;
import :text_layout;
import :widgets;

namespace file_monitor::tui {

    namespace {

        constexpr auto COLUMN_GAP{ 2 };
        constexpr auto TIME_WIDTH{ 12 };  // "12:34:56.789"
        constexpr auto STATUS_WIDTH{ 6 }; // "重命名"
        constexpr auto SIZE_WIDTH{ 7 };   // "1023 KB"
        // The size column is hidden below this table width, which is that of
        // an 80-column terminal.
        constexpr auto SIZE_COLUMN_MIN_WIDTH{ 78 };
        constexpr auto DETAIL_LABEL_WIDTH{ 8 };
        constexpr auto WHEEL_SCROLL_STEP{ std::ptrdiff_t{ 3 } };

        auto fixed_width(int width) -> ftxui::Decorator {
            return ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
        }

        auto gap() -> ftxui::Element {
            return ftxui::text(std::string(COLUMN_GAP, ' '));
        }

        auto show_size_column(int width) -> bool {
            return width >= SIZE_COLUMN_MIN_WIDTH;
        }

        auto path_column_width(int width) -> int {
            auto fixed_columns_width{ TIME_WIDTH + STATUS_WIDTH + 2 * COLUMN_GAP };
            if (show_size_column(width)) {
                fixed_columns_width += SIZE_WIDTH + COLUMN_GAP;
            }
            return std::max(width - fixed_columns_width, 0);
        }

        auto make_row(
            ftxui::Element time,
            ftxui::Element status,
            ftxui::Element size,
            ftxui::Element path,
            int width
        ) -> ftxui::Element {
            ftxui::Elements cells{
                std::move(time) | fixed_width(TIME_WIDTH + COLUMN_GAP),
                std::move(status) | fixed_width(STATUS_WIDTH + COLUMN_GAP),
            };
            if (show_size_column(width)) {
                cells.push_back(std::move(size) | fixed_width(SIZE_WIDTH + COLUMN_GAP));
            }
            cells.push_back(std::move(path) | ftxui::xflex);
            return ftxui::hbox(std::move(cells));
        }

        auto change_row(core::FileChange const& change, int width) -> ftxui::Element {
            return make_row(
                ftxui::text(std::string{ core::file_change_time_of_day(change) }),
                file_change_status_element(change.status),
                ftxui::text(change.size),
                ftxui::text(change_path_text(change, path_column_width(width))),
                width
            );
        }

        auto size_description(core::FileChange const& change) -> std::string {
            // Directories have no size.
            if (change.size == "-") {
                return "文件夹";
            }
            return std::format("大小 {}", change.size);
        }

        auto append_labeled_lines(
            ftxui::Elements& lines,
            std::string_view label,
            std::string_view text,
            int width
        ) -> void {
            auto first_line{ true };
            for (auto& line : wrap_text(text, width - DETAIL_LABEL_WIDTH)) {
                auto label_text{ ftxui::text(first_line ? std::string{ label } : std::string{}) };
                lines.push_back(
                    ftxui::hbox({
                        std::move(label_text) | ftxui::dim | fixed_width(DETAIL_LABEL_WIDTH),
                        ftxui::text(std::move(line)),
                    })
                );
                first_line = false;
            }
        }

    } // namespace

    // The change table of the main view and the details of its selected
    // change. While the list follows new changes, the latest one is selected.
    // The selection and the first visible row are kept by sequence number, so
    // they stay on the same changes when the session drops older ones.
    export class ChangeList final {
    public:
        explicit ChangeList(core::MonitorSession const& session) : m_session{ session } {}

        auto Render(int width) -> ftxui::Element {
            refresh();

            auto header{
                make_row(
                    ftxui::text("时间"),
                    ftxui::text("状态"),
                    ftxui::text("大小"),
                    ftxui::text("路径"),
                    width
                ) |
                ftxui::bold
            };

            auto body{ ftxui::text(std::string{ emptyText() }) | ftxui::dim };
            if (!m_rows.empty()) {
                // The body box is only known after layout, so render enough rows
                // to fill the terminal and let the frame clip them; this keeps the
                // view correct right after the terminal is resized. While
                // following, the focused last row keeps the frame at the bottom.
                auto const rendered_row_count{
                    std::max(
                        visibleRowCount(),
                        static_cast<std::size_t>(std::max(ftxui::Terminal::Size().dimy, 0))
                    )
                };
                auto const first_row{
                    m_follow_latest ?
                        m_rows.size() - std::min(rendered_row_count, m_rows.size()) :
                        m_first_visible_row
                };
                auto const last_row{ std::min(m_rows.size(), first_row + rendered_row_count) };

                auto const changes{ m_session.Changes() };
                ftxui::Elements rows;
                rows.reserve(last_row - first_row);
                for (auto row{ first_row }; row < last_row; ++row) {
                    auto element{ change_row(changes[m_rows[row]], width) };
                    if (row == m_selected_row) {
                        element |= ftxui::inverted;
                        if (m_follow_latest) {
                            element |= ftxui::focus;
                        }
                    }
                    rows.push_back(std::move(element));
                }
                body = ftxui::vbox(std::move(rows));
            }

            return ftxui::vbox({
                std::move(header),
                ftxui::separatorLight(),
                std::move(body) | ftxui::yframe | ftxui::flex | ftxui::reflect(m_body_box),
            });
        }

        // The details of the selected change below a separator, or an empty
        // element when nothing is selected or the details are hidden. Call
        // after Render().
        auto RenderDetails(int width) const -> ftxui::Element {
            auto const* selected_change{ selectedChange() };
            if (!m_show_details || selected_change == nullptr) {
                return ftxui::emptyElement();
            }

            auto const& change{ *selected_change };
            ftxui::Elements lines{
                ftxui::separatorLight(),
                ftxui::hbox({
                    file_change_status_element(change.status) | ftxui::bold,
                    gap(),
                    ftxui::text(std::string{ core::file_change_event_time(change) }),
                    gap(),
                    ftxui::text(size_description(change)),
                }),
            };
            if (auto const creation_time{ core::file_change_creation_time(change) }; !creation_time.empty()) {
                append_labeled_lines(lines, "创建于", creation_time, width);
            }
            if (change.previous_absolute_path.empty()) {
                append_labeled_lines(lines, "路径", change.absolute_path, width);
            } else {
                append_labeled_lines(lines, "原路径", change.previous_absolute_path, width);
                append_labeled_lines(lines, "新路径", change.absolute_path, width);
            }
            return ftxui::vbox(std::move(lines));
        }

        auto OnEvent(ftxui::Event event) -> bool {
            refresh();
            auto const page_row_count{
                static_cast<std::ptrdiff_t>(std::max(visibleRowCount(), std::size_t{ 1 }))
            };
            if (event == ftxui::Event::ArrowUp) {
                moveSelection(-1);
            } else if (event == ftxui::Event::ArrowDown) {
                moveSelection(1);
            } else if (event == ftxui::Event::PageUp) {
                moveSelection(-page_row_count);
            } else if (event == ftxui::Event::PageDown) {
                moveSelection(page_row_count);
            } else if (event == ftxui::Event::Home) {
                selectRow(0);
            } else if (event == ftxui::Event::End) {
                FollowLatest();
            } else if (event == ftxui::Event::Return) {
                ToggleDetails();
            } else if (!event.is_mouse()) {
                return false;
            } else {
                auto const& mouse{ event.mouse() };
                auto const clicked{
                    mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed
                };
                if (mouse.button == ftxui::Mouse::WheelUp) {
                    moveSelection(-WHEEL_SCROLL_STEP);
                } else if (mouse.button == ftxui::Mouse::WheelDown) {
                    moveSelection(WHEEL_SCROLL_STEP);
                } else if (clicked && m_body_box.Contain(mouse.x, mouse.y)) {
                    auto const row{
                        m_first_visible_row + static_cast<std::size_t>(mouse.y - m_body_box.y_min)
                    };
                    if (row < m_rows.size()) {
                        selectRow(row);
                    }
                } else {
                    return false;
                }
            }
            return true;
        }

        auto FollowLatest() -> void {
            m_follow_latest = true;
        }

        auto ToggleDetails() -> void {
            m_show_details = !m_show_details;
        }

        // Filters, see core::ChangeFilter.
        auto ToggleStatus(core::FileChangeStatus status) -> void {
            m_filter.ToggleStatus(status);
        }

        [[nodiscard]]
        auto ShowsStatus(core::FileChangeStatus status) const -> bool {
            return m_filter.ShowsStatus(status);
        }

        auto SetPathFilter(std::string_view text) -> void {
            m_filter.SetPathText(text);
        }

        auto ClearFilters() -> void {
            m_filter.Clear();
        }

        [[nodiscard]]
        auto IsFiltered() const -> bool {
            return m_filter.IsActive();
        }

        // The selected change, or nullptr when no change matches. Brings the
        // list up to date with the session first.
        [[nodiscard]]
        auto SelectedChange() -> core::FileChange const* {
            refresh();
            return selectedChange();
        }

        // The queries below describe the list as of the last Render().
        [[nodiscard]]
        auto FollowsLatest() const -> bool {
            return m_follow_latest;
        }

        // Matching changes added since the list stopped following new
        // changes.
        [[nodiscard]]
        auto NewChangeCount() const -> std::size_t {
            if (m_follow_latest) {
                return 0;
            }
            auto const first_unseen{ std::ranges::lower_bound(m_rows, changeIndex(m_unseen_sequence)) };
            return static_cast<std::size_t>(m_rows.end() - first_unseen);
        }

        // 1-based position of the selected change, or 0 when no change
        // matches.
        [[nodiscard]]
        auto SelectedPosition() const -> std::size_t {
            return m_rows.empty() ? 0 : m_selected_row + 1;
        }

        // The number of changes that match the filters.
        [[nodiscard]]
        auto RowCount() const -> std::size_t {
            return m_rows.size();
        }

    private:
        core::MonitorSession const& m_session;
        // Indices into m_session.Changes() of the changes that match the
        // filters, oldest first.
        std::vector<std::size_t> m_rows;
        std::size_t m_selected_row{ 0 };
        std::size_t m_first_visible_row{ 0 };
        std::uint64_t m_selected_sequence{ 0 };
        std::uint64_t m_first_visible_sequence{ 0 };
        // The sequence number after the last change of the last refresh.
        std::uint64_t m_end_sequence{ 0 };
        // The first sequence number added after the list stopped following.
        std::uint64_t m_unseen_sequence{ 0 };
        bool m_follow_latest{ true };
        bool m_show_details{ true };
        core::ChangeFilter m_filter;
        ftxui::Box m_body_box{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };

        auto refresh() -> void {
            auto const changes{ m_session.Changes() };
            auto const first_sequence{ m_session.FirstChangeSequence() };
            // None of the changes of the last refresh is left, for example because
            // saving the settings cleared the list: follow the new changes.
            if (first_sequence >= m_end_sequence) {
                m_follow_latest = true;
            }
            m_end_sequence = first_sequence + changes.size();

            m_rows = std::views::iota(std::size_t{ 0 }, changes.size()) |
                std::views::filter([&](std::size_t index) { return m_filter.Matches(changes[index]); }) |
                std::ranges::to<std::vector<std::size_t>>();
            if (m_rows.empty()) {
                m_selected_row = 0;
                m_first_visible_row = 0;
                return;
            }

            m_selected_row = m_follow_latest ? m_rows.size() - 1 : rowAtOrAfter(m_selected_sequence);
            m_selected_sequence = rowSequence(m_selected_row);
            m_first_visible_row = rowAtOrAfter(m_first_visible_sequence);
            scrollToSelection();
        }

        auto selectRow(std::size_t row) -> void {
            if (m_rows.empty()) {
                return;
            }
            auto const last_row{ m_rows.size() - 1 };
            m_selected_row = std::min(row, last_row);
            m_selected_sequence = rowSequence(m_selected_row);

            auto const follow_latest{ m_selected_row == last_row };
            if (m_follow_latest && !follow_latest) {
                m_unseen_sequence = m_end_sequence;
            }
            m_follow_latest = follow_latest;
            scrollToSelection();
        }

        auto moveSelection(std::ptrdiff_t row_count) -> void {
            if (m_rows.empty()) {
                return;
            }
            auto const row{
                std::clamp(
                    static_cast<std::ptrdiff_t>(m_selected_row) + row_count,
                    std::ptrdiff_t{ 0 },
                    static_cast<std::ptrdiff_t>(m_rows.size() - 1)
                )
            };
            selectRow(static_cast<std::size_t>(row));
        }

        auto scrollToSelection() -> void {
            auto const visible_row_count{ std::max(visibleRowCount(), std::size_t{ 1 }) };
            auto const last_first_row{
                m_rows.size() > visible_row_count ? m_rows.size() - visible_row_count : 0
            };
            auto first_row{ m_follow_latest ? last_first_row : m_first_visible_row };
            if (m_selected_row < first_row) {
                first_row = m_selected_row;
            } else if (m_selected_row >= first_row + visible_row_count) {
                first_row = m_selected_row + 1 - visible_row_count;
            }
            // Do not leave rows empty below the last change.
            m_first_visible_row = std::min(first_row, last_first_row);
            m_first_visible_sequence = rowSequence(m_first_visible_row);
        }

        [[nodiscard]]
        auto selectedChange() const -> core::FileChange const* {
            if (m_rows.empty()) {
                return nullptr;
            }
            return &m_session.Changes()[m_rows[m_selected_row]];
        }

        [[nodiscard]]
        auto visibleRowCount() const -> std::size_t {
            if (m_body_box.IsEmpty()) {
                return static_cast<std::size_t>(std::max(ftxui::Terminal::Size().dimy, 0));
            }
            return static_cast<std::size_t>(m_body_box.y_max - m_body_box.y_min + 1);
        }

        [[nodiscard]]
        auto rowSequence(std::size_t row) const -> std::uint64_t {
            return m_session.FirstChangeSequence() + m_rows[row];
        }

        [[nodiscard]]
        auto changeIndex(std::uint64_t sequence) const -> std::size_t {
            auto const first_sequence{ m_session.FirstChangeSequence() };
            return sequence > first_sequence ? static_cast<std::size_t>(sequence - first_sequence) : 0;
        }

        [[nodiscard]]
        auto rowAtOrAfter(std::uint64_t sequence) const -> std::size_t {
            auto const row{ std::ranges::lower_bound(m_rows, changeIndex(sequence)) - m_rows.begin() };
            return std::min(static_cast<std::size_t>(row), m_rows.size() - 1);
        }

        [[nodiscard]]
        auto emptyText() const -> std::string_view {
            if (m_session.CurrentConfiguration().directories.empty()) {
                return "尚未添加监控文件夹，按 s 打开设置添加";
            }
            if (!m_session.Changes().empty()) {
                return "没有符合过滤条件的变更，按 0 清除过滤";
            }
            return "正在等待文件变更";
        }
    };

} // namespace file_monitor::tui
