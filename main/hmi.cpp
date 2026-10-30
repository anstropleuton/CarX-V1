#include "hmi.hpp"

#include "esp_timer.h"

#include <algorithm>
#include <array>
#include <ranges>

constexpr std::int64_t loading_period_us{2'000'000};

constexpr auto hue_saturation{1.0F};
constexpr auto hue_value{0.5F};
constexpr auto value_baseline{0.5F};
constexpr auto blink_threshold{0.38197F};

// Alternating on/off durations, starting with on
constexpr std::array<std::uint16_t, 1> blink_one_ms{300};
constexpr std::array<std::uint16_t, 3> blink_two_ms{200, 200, 200};
constexpr std::array<std::uint16_t, 5> blink_three_ms{150, 150, 150, 150, 150};
constexpr std::array<std::uint16_t, 1> blink_one_long_ms{500};

namespace {
    auto blink_pattern(hmi::blink_code const code) -> std::span<std::uint16_t const> {
        switch (code) {
            case hmi::blink_code::one:
                return blink_one_ms;
            case hmi::blink_code::two:
                return blink_two_ms;
            case hmi::blink_code::three:
                return blink_three_ms;
            case hmi::blink_code::one_long:
                return blink_one_long_ms;
        }
        return {};
    }

    // hue is in [0, 1)
    auto hsv_to_rgb(float const hue, float const saturation, float const value) -> hmi::rgb {
        auto const sector{hue * 6.0F};
        auto const index{static_cast<int>(sector)};
        auto const frac{sector - static_cast<float>(index)};

        auto const p{value * (1.0F - saturation)};
        auto const q{value * (1.0F - saturation * frac)};
        auto const t{value * (1.0F - saturation * (1.0F - frac))};

        switch (index % 6) {
            case 0:
                return {value, t, p};
            case 1:
                return {q, value, p};
            case 2:
                return {p, value, t};
            case 3:
                return {p, q, value};
            case 4:
                return {t, p, value};
            default:
                return {value, p, q};
        }
    }
}

auto hmi::hmi::create(config const &cfg) -> std::optional<hmi> {
    for (auto const &in : {cfg.primary, cfg.secondary, cfg.tertiary})
        if (in.btn == nullptr)
            return std::nullopt;

    if (cfg.gs_red == nullptr || cfg.gs_green == nullptr || cfg.gs_blue == nullptr || cfg.is_red == nullptr)
        return std::nullopt;

    if (cfg.pages.empty())
        return std::nullopt;

    if (!std::ranges::all_of(cfg.pages, [](page const &pg) { return static_cast<bool>(pg.color); }))
        return std::nullopt;

    return hmi{};
}

auto hmi::hmi::paused() const -> bool {
    return paused_;
}

auto hmi::hmi::current_menu() const -> menu {
    return menu_;
}

auto hmi::hmi::current_input_method() const -> input_method {
    return method_;
}

auto hmi::hmi::update(config const &cfg) -> void {
    cfg.primary.btn->update(cfg.primary.cfg);
    cfg.secondary.btn->update(cfg.secondary.cfg);
    cfg.tertiary.btn->update(cfg.tertiary.cfg);

    now_us_ = esp_timer_get_time();

    if (menu_ == menu::event_mode) {
        // All input is ignored while the event is busy
        if (auto const &act{cfg.actions.at(action_)}; !act.busy || !act.busy())
            finish_action_(cfg);
    } else {
        poll_tertiary_(cfg);
        poll_lists_(cfg);
    }

    render_(cfg);
}

auto hmi::hmi::repeat_step_(button::button const &btn, std::optional<std::int64_t> &next_us, std::int64_t const now_us)
    -> bool {
    if (btn.pressed()) {
        next_us = now_us + long_press_us;
        return true;
    }

    if (btn.up()) {
        next_us.reset();
        return false;
    }

    if (!next_us || now_us < *next_us)
        return false;

    *next_us += repeat_period_us;
    return true;
}

auto hmi::hmi::poll_tertiary_(config const &cfg) -> void {
    auto const &btn{*cfg.tertiary.btn};

    if (btn.pressed()) {
        tertiary_down_us_  = now_us_;
        tertiary_consumed_ = false;

        // Second press inside the window: the pending back becomes a root
        if (back_deadline_us_) {
            back_deadline_us_.reset();
            tertiary_consumed_ = true;
            root_();
        }
    } else if (btn.released()) {
        tertiary_down_us_.reset();
        if (!tertiary_consumed_)
            back_deadline_us_ = now_us_ + double_press_us;
    } else if (btn.down() &&
               tertiary_down_us_ &&
               !tertiary_consumed_ &&
               now_us_ - *tertiary_down_us_ >= long_press_us) {
        tertiary_consumed_ = true;
        toggle_method_();
    }

    // A single press only becomes back once it can no longer turn into a double press
    if (back_deadline_us_ && now_us_ >= *back_deadline_us_) {
        back_deadline_us_.reset();
        back_(cfg);
    }
}

auto hmi::hmi::poll_lists_(config const &cfg) -> void {
    auto const primary_pressed{cfg.primary.btn->pressed()};
    auto const primary_step{repeat_step_(*cfg.primary.btn, primary_next_us_, now_us_)};
    auto const secondary_step{repeat_step_(*cfg.secondary.btn, secondary_next_us_, now_us_)};

    switch (method_) {
        case input_method::normal:
            if (secondary_step)
                move_(cfg, 1, true);
            else if (primary_pressed)
                commit_(cfg);
            break;
        case input_method::adjustment:
            if (primary_step)
                move_(cfg, 1, false);
            if (secondary_step)
                move_(cfg, -1, false);
            break;
    }
}

auto hmi::hmi::move_(config const &cfg, int const direction, bool const wrap) -> void {
    auto const total{total_(cfg)};
    if (total == 0)
        return;

    auto const index{index_()};

    if (wrap) {
        set_index_((index + 1) % total);
        blink_(index + 1 == total ? blink_code::two : blink_code::one);
        return;
    }

    if (direction > 0 ? index + 1 >= total : index == 0) {
        blink_(blink_code::two);
        return;
    }

    set_index_(direction > 0 ? index + 1 : index - 1);
    blink_(blink_code::one);
}

auto hmi::hmi::commit_(config const &cfg) -> void {
    switch (menu_) {
        case menu::home:
            if (cfg.actions.empty())
                return;
            menu_ = menu::action_selection;
            blink_(blink_code::one);
            break;
        case menu::action_selection:
            enter_action_(cfg);
            blink_(blink_code::one);
            break;
        case menu::value_mode:
            if (auto const &act{cfg.actions.at(action_)}; act.apply)
                act.apply(value_);
            finish_action_(cfg);
            blink_(blink_code::one);
            break;
        case menu::event_mode:
            break;
        case menu::reports:
            menu_ = menu::action_selection;
            blink_(blink_code::one);
            break;
    }
}

auto hmi::hmi::back_(config const &cfg) -> void {
    switch (menu_) {
        case menu::home:
            if (page_ != 0)
                page_ = 0;
            else
                paused_ = !paused_;
            blink_(blink_code::three);
            break;
        case menu::action_selection:
            menu_ = menu::home;
            blink_(blink_code::three);
            break;
        case menu::value_mode:
            // Changes were never applied, so leaving is the revert
            menu_ = menu::action_selection;
            blink_(blink_code::three);
            break;
        case menu::event_mode:
            break;
        case menu::reports:
            enter_action_(cfg);
            blink_(blink_code::three);
            break;
    }
}

auto hmi::hmi::root_() -> void {
    menu_ = menu::home;
    page_ = 0;
    blink_(blink_code::three);
}

auto hmi::hmi::toggle_method_() -> void {
    method_ = method_ == input_method::normal ? input_method::adjustment : input_method::normal;
    blink_(blink_code::one_long);
}

auto hmi::hmi::enter_action_(config const &cfg) -> void {
    auto const &act{cfg.actions.at(action_)};

    switch (act.kind) {
        case action_kind::value:
            if (act.steps == 0)
                return;
            value_ = act.current ? std::min(act.current(), act.steps - 1) : 0;
            menu_  = menu::value_mode;
            break;
        case action_kind::event:
            run_event_(cfg);
            break;
    }
}

auto hmi::hmi::run_event_(config const &cfg) -> void {
    // Gesture state would go stale while input is ignored
    tertiary_down_us_.reset();
    back_deadline_us_.reset();
    primary_next_us_.reset();
    secondary_next_us_.reset();
    tertiary_consumed_ = true;

    if (auto const &act{cfg.actions.at(action_)}; act.start)
        act.start();
    menu_ = menu::event_mode;
}

auto hmi::hmi::finish_action_(config const &cfg) -> void {
    auto const &act{cfg.actions.at(action_)};

    report_ = 0;
    menu_   = act.report_count && act.report_count() > 0 ? menu::reports : menu::action_selection;
}

auto hmi::hmi::total_(config const &cfg) const -> std::size_t {
    switch (menu_) {
        case menu::home:
            return cfg.pages.size();
        case menu::action_selection:
            return cfg.actions.size();
        case menu::value_mode:
            return cfg.actions.at(action_).steps;
        case menu::event_mode:
            return 0;
        case menu::reports: {
            auto const &act{cfg.actions.at(action_)};
            return act.report_count ? act.report_count() : 0;
        }
    }
    return 0;
}

auto hmi::hmi::index_() const -> std::size_t {
    switch (menu_) {
        case menu::home:
            return page_;
        case menu::action_selection:
            return action_;
        case menu::value_mode:
            return value_;
        case menu::event_mode:
            return 0;
        case menu::reports:
            return report_;
    }
    return 0;
}

auto hmi::hmi::set_index_(std::size_t const index) -> void {
    switch (menu_) {
        case menu::home:
            page_ = index;
            break;
        case menu::action_selection:
            action_ = index;
            break;
        case menu::value_mode:
            value_ = index;
            break;
        case menu::event_mode:
            break;
        case menu::reports:
            report_ = index;
            break;
    }
}

auto hmi::hmi::blink_(blink_code const code) -> void {
    blink_pattern_  = blink_pattern(code);
    blink_start_us_ = now_us_;
}

auto hmi::hmi::blink_level_(float const baseline) const -> float {
    auto const elapsed{now_us_ - blink_start_us_};

    // The pulse contrasts with the baseline; the gaps between pulses take the opposite extreme
    auto const on_level{baseline < blink_threshold ? 1.0F : 0.0F};

    std::int64_t edge_us{};
    for (auto const &[index, duration_ms] : blink_pattern_ | std::views::enumerate) {
        edge_us += static_cast<std::int64_t>(duration_ms) * 1'000;
        if (elapsed < edge_us)
            return index % 2 == 0 ? on_level : 1.0F - on_level;
    }

    return baseline;
}

auto hmi::hmi::hue_display_(std::size_t const index, std::size_t const total) -> rgb {
    if (total == 0)
        return {};

    return hsv_to_rgb(static_cast<float>(index) / static_cast<float>(total), hue_saturation, hue_value);
}

auto hmi::hmi::r_display_(std::size_t const index, std::size_t const total) -> float {
    if (total == 0)
        return 0.0F;

    return static_cast<float>(index + 1) / static_cast<float>(total);
}

auto hmi::hmi::render_(config const &cfg) const -> void {
    auto const total{total_(cfg)};
    auto const index{index_()};

    rgb   gs{};
    float baseline{};

    switch (menu_) {
        case menu::home:
            gs       = cfg.pages.at(page_).color();
            baseline = r_display_(index, total);
            break;
        case menu::action_selection:
            gs       = hue_display_(index, total);
            baseline = r_display_(index, total);
            break;
        case menu::value_mode:
            gs       = hue_display_(index, total);
            baseline = value_baseline;
            break;
        case menu::event_mode: {
            // Spinning hue as the default loading indicator
            auto const hue_phase{
                static_cast<float>(now_us_ % loading_period_us) / static_cast<float>(loading_period_us)};
            gs       = hsv_to_rgb(hue_phase, hue_saturation, hue_value);
            baseline = value_baseline;
            break;
        }
        case menu::reports: {
            auto const &act{cfg.actions.at(action_)};
            gs       = act.report_color ? act.report_color(index) : hue_display_(index, total);
            baseline = r_display_(index, total);
            break;
        }
    }

    cfg.gs_red->set_duty_gamma(gs.r);
    cfg.gs_green->set_duty_gamma(gs.g);
    cfg.gs_blue->set_duty_gamma(gs.b);
    cfg.is_red->set_duty_gamma(blink_level_(baseline));
}
