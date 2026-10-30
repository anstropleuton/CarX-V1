#include "mcpwm.hpp"

#include "driver/mcpwm_cap.h"
#include "esp_err.h"

#include <mutex>
#include <ranges>
#include <utility>

namespace {
    std::array<bool, 2> cap_timer_slots{};
    std::mutex          slots_mutex;
}

auto mcpwm::timer::create(std::uint32_t const resolution_hz) -> std::optional<timer> {
    std::scoped_lock lock{slots_mutex};

    for (auto &&[index, slot] : cap_timer_slots | std::views::enumerate) {
        if (!slot) {
            auto const group_id{index};

            mcpwm_capture_timer_config_t const config{
                .group_id{group_id},
                .clk_src{MCPWM_CAPTURE_CLK_SRC_DEFAULT},
                .resolution_hz{resolution_hz},
            };

            mcpwm_cap_timer_handle_t handle{};
            ESP_ERROR_CHECK(mcpwm_new_capture_timer(&config, &handle));

            slot = true;

            timer tmr;
            tmr.handle   = handle;
            tmr.group_id = group_id;

            return tmr;
        }
    }

    return std::nullopt;
}

mcpwm::timer::~timer() {
    release_();
}

mcpwm::timer::timer(timer &&tmr) noexcept
    : handle{std::exchange(tmr.handle, nullptr)}, group_id{std::exchange(tmr.group_id, -1)},
      enabled_{std::exchange(tmr.enabled_, false)}, running_{std::exchange(tmr.running_, false)} {}

auto mcpwm::timer::operator=(timer &&tmr) noexcept -> timer & {
    if (this != &tmr) {
        release_();
        handle   = std::exchange(tmr.handle, nullptr);
        group_id = std::exchange(tmr.group_id, -1);
        enabled_ = std::exchange(tmr.enabled_, false);
        running_ = std::exchange(tmr.running_, false);
    }
    return *this;
}

auto mcpwm::timer::enable() -> void {
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_enable(handle)) == ESP_OK)
        enabled_ = true;
}

auto mcpwm::timer::disable() -> void {
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_disable(handle)) == ESP_OK)
        enabled_ = false;
}

auto mcpwm::timer::start() -> void {
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_start(handle)) == ESP_OK)
        running_ = true;
}

auto mcpwm::timer::stop() -> void {
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_stop(handle)) == ESP_OK)
        running_ = false;
}

auto mcpwm::timer::resolution() const -> std::uint32_t {
    std::uint32_t hz{};
    ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_get_resolution(handle, &hz));
    return hz;
}

auto mcpwm::timer::release_() -> void {
    if (handle == nullptr)
        return;
    std::scoped_lock lock{slots_mutex};

    if (running_)
        ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_stop(handle));
    if (enabled_)
        ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_timer_disable(handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_del_capture_timer(handle));

    cap_timer_slots.at(group_id) = false;

    handle   = nullptr;
    group_id = -1;
    enabled_ = false;
    running_ = false;
}

auto mcpwm::channel::create(gpio_num_t const pin, timer const &tmr) -> std::optional<channel> {
    std::scoped_lock lock{slots_mutex};

    mcpwm_capture_channel_config_t const config{
        .gpio_num{static_cast<int>(pin)},
        .prescale{1                    },
        .flags{
                  .pos_edge{1},
                  .neg_edge{1},
                  },
    };

    mcpwm_cap_channel_handle_t handle{};
    auto const                 err{mcpwm_new_capture_channel(tmr.handle, &config, &handle)};

    if (err == ESP_ERR_NOT_FOUND)
        return std::nullopt;

    ESP_ERROR_CHECK(err);

    channel ch;
    ch.handle = handle;

    return ch;
}

mcpwm::channel::~channel() {
    release_();
}

mcpwm::channel::channel(channel &&ch) noexcept
    : handle{std::exchange(ch.handle, nullptr)}, enabled_{std::exchange(ch.enabled_, false)} {}

auto mcpwm::channel::operator=(channel &&ch) noexcept -> channel & {
    if (this != &ch) {
        release_();
        handle   = std::exchange(ch.handle, nullptr);
        enabled_ = std::exchange(ch.enabled_, false);
    }
    return *this;
}

auto mcpwm::channel::enable() -> void {
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_channel_enable(handle)) == ESP_OK)
        enabled_ = true;
}

auto mcpwm::channel::disable() -> void {
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_channel_disable(handle)) == ESP_OK)
        enabled_ = false;
}

auto mcpwm::channel::set_callback(mcpwm_capture_event_cb_t const on_cap, void *const user_data) const -> void {
    mcpwm_capture_event_callbacks_t const cbs{
        .on_cap{on_cap},
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_channel_register_event_callbacks(handle, &cbs, user_data));
}

auto mcpwm::channel::release_() -> void {
    if (handle == nullptr)
        return;
    std::scoped_lock lock{slots_mutex};

    if (enabled_)
        ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_capture_channel_disable(handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mcpwm_del_capture_channel(handle));

    handle   = nullptr;
    enabled_ = false;
}
