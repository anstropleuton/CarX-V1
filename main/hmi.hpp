#pragma once

#include "button.hpp"
#include "ledc.hpp"
#include "utilities.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace hmi {
    constexpr std::int64_t long_press_us{400'000};
    constexpr std::int64_t double_press_us{400'000};
    constexpr std::int64_t repeat_period_us{250'000}; // 4 steps/s
    constexpr auto         max_update_period_ms{10};  // Buttons are polled inside update
    constexpr std::size_t  max_pages{8};
    constexpr std::size_t  max_actions{8};

    struct rgb final {
        float r{};
        float g{};
        float b{};
    };

    enum class menu : std::uint8_t { home, action_selection, value_mode, event_mode, reports };

    enum class input_method : std::uint8_t { normal, adjustment };

    enum class action_kind : std::uint8_t { value, event };

    enum class blink_code : std::uint8_t { one, two, three, one_long };

    struct input final {
        button::button *btn{};
        button::config  cfg{};
    };

    // A status page on the home menu. The color is shown on the GSLED as is.
    struct page final {
        inplace_function<auto()->rgb> color;
    };

    struct action final {
        action_kind kind{};

        // Value mode: `steps` selectable values, `current` is the applied index, `apply` is called on commit only
        std::size_t                               steps{};
        inplace_function<auto()->std::size_t>     current;
        inplace_function<auto(std::size_t)->void> apply;

        // Event mode: `start` is called on entry, the HMI stays busy-waiting until `busy` returns false
        inplace_function<auto()->void> start;
        inplace_function<auto()->bool> busy;

        // Optional results, shown on the reports menu after the action finishes
        inplace_function<auto()->std::size_t>    report_count;
        inplace_function<auto(std::size_t)->rgb> report_color;
    };

    struct config final {
        input primary;
        input secondary;
        input tertiary;

        ledc::channel const *gs_red{};
        ledc::channel const *gs_green{};
        ledc::channel const *gs_blue{};
        ledc::channel const *is_red{};

        inplace_vector<page, max_pages>     pages;
        inplace_vector<action, max_actions> actions;
    };

    class hmi final {
    public:
        [[nodiscard]] static auto create(config const &cfg) -> std::optional<hmi>;

        // Call from a single task, at least every `max_update_period_ms`
        auto update(config const &cfg) -> void;

        // Back on page 0 toggles this; the rest of the system is expected to honor it
        [[nodiscard]] auto paused() const -> bool;
        [[nodiscard]] auto current_menu() const -> menu;
        [[nodiscard]] auto current_input_method() const -> input_method;

    private:
        hmi() = default;

        [[nodiscard]] static auto repeat_step_(
            button::button const &btn, std::optional<std::int64_t> &next_us, std::int64_t now_us) -> bool;
        [[nodiscard]] static auto hue_display_(std::size_t index, std::size_t total) -> rgb;
        [[nodiscard]] static auto r_display_(std::size_t index, std::size_t total) -> float;

        [[nodiscard]] auto total_(config const &cfg) const -> std::size_t;
        [[nodiscard]] auto index_() const -> std::size_t;
        [[nodiscard]] auto blink_level_(float baseline) const -> float;

        auto set_index_(std::size_t index) -> void;
        auto blink_(blink_code code) -> void;
        auto poll_tertiary_(config const &cfg) -> void;
        auto poll_lists_(config const &cfg) -> void;
        auto move_(config const &cfg, int direction, bool wrap) -> void;
        auto commit_(config const &cfg) -> void;
        auto back_(config const &cfg) -> void;
        auto root_() -> void;
        auto toggle_method_() -> void;
        auto enter_action_(config const &cfg) -> void;
        auto run_event_(config const &cfg) -> void;
        auto finish_action_(config const &cfg) -> void;
        auto render_(config const &cfg) const -> void;

        menu         menu_{menu::home};
        input_method method_{input_method::normal};
        std::size_t  page_{};
        std::size_t  action_{};
        std::size_t  value_{};
        std::size_t  report_{};
        bool         paused_{};
        std::int64_t now_us_{};

        std::optional<std::int64_t> primary_next_us_;
        std::optional<std::int64_t> secondary_next_us_;
        std::optional<std::int64_t> tertiary_down_us_;
        std::optional<std::int64_t> back_deadline_us_;
        bool                        tertiary_consumed_{};

        std::span<std::uint16_t const> blink_pattern_;
        std::int64_t                   blink_start_us_{};
    };
}
