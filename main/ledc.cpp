#include "ledc.hpp"

#include "driver/ledc.h"
#include "esp_log.h"
#include "gamma_table.hpp"

#include <cmath>
#include <mutex>
#include <ranges>

namespace {
    std::mutex          slots_mutex;
    std::array<bool, 4> hs_timer_slots{};
    std::array<bool, 4> ls_timer_slots{};
    std::array<bool, 4> hs_channel_slots{};
    std::array<bool, 4> ls_channel_slots{};
}

auto ledc::timer::create(ledc_mode_t const speed_mode, std::uint32_t const frequency, ledc_timer_bit_t const duty_res)
    -> std::optional<timer> {
    std::scoped_lock lock{slots_mutex};

    for (auto &timer_slots{speed_mode == LEDC_HIGH_SPEED_MODE ? hs_timer_slots : ls_timer_slots};
        auto &&[index, slot] : timer_slots | std::views::enumerate) {
        if (!slot) {
            auto const timer_num{static_cast<ledc_timer_t>(index)};

            ledc_timer_config_t const config{
                .speed_mode{speed_mode},
                .duty_resolution{duty_res},
                .timer_num{timer_num},
                .freq_hz{frequency},
            };

            ESP_ERROR_CHECK(ledc_timer_config(&config));

            slot = true;

            timer timer;
            timer.speed_mode = speed_mode;
            timer.timer_num  = timer_num;

            return timer;
        }
    }

    return std::nullopt;
}

ledc::timer::~timer() {
    release_();
}

ledc::timer::timer(timer &&tmr) noexcept : speed_mode{tmr.speed_mode}, timer_num{tmr.timer_num} {
    tmr.timer_num.reset();
}

auto ledc::timer::operator=(timer &&tmr) noexcept -> timer & {
    if (this != &tmr) {
        release_();
        speed_mode = tmr.speed_mode;
        timer_num  = tmr.timer_num;
        tmr.timer_num.reset();
    }
    return *this;
}

auto ledc::timer::set_frequency(std::uint32_t const frequency) const -> void {
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_freq(speed_mode, timer_num.value(), frequency));
}

auto ledc::timer::release_() -> void {
    if (!timer_num)
        return;
    std::scoped_lock lock{slots_mutex};

    auto &timer_slots{speed_mode == LEDC_HIGH_SPEED_MODE ? hs_timer_slots : ls_timer_slots};
    timer_slots.at(static_cast<std::uint8_t>(timer_num.value())) = false;
    timer_num.reset();
}

auto ledc::channel::create(gpio_num_t const pin, std::uint32_t const duty, timer const &tmr) -> std::optional<channel> {
    std::scoped_lock lock{slots_mutex};

    for (auto &channel_slots{tmr.speed_mode == LEDC_HIGH_SPEED_MODE ? hs_channel_slots : ls_channel_slots};
        auto &&[index, slot] : channel_slots | std::views::enumerate) {
        if (!slot) {
            auto const channel_num{static_cast<ledc_channel_t>(index)};

            ledc_channel_config_t const config{
                .gpio_num{pin},
                .speed_mode{tmr.speed_mode},
                .channel{channel_num},
                .timer_sel{tmr.timer_num.value()},
                .duty{duty},
            };

            ESP_ERROR_CHECK(ledc_channel_config(&config));

            slot = true;

            channel channel;
            channel.speed_mode  = tmr.speed_mode;
            channel.channel_num = channel_num;

            return channel;
        }
    }

    return std::nullopt;
}

ledc::channel::~channel() {
    release_();
}

ledc::channel::channel(channel &&ch) noexcept : speed_mode{ch.speed_mode}, channel_num{ch.channel_num} {
    ch.channel_num.reset();
}

auto ledc::channel::operator=(channel &&ch) noexcept -> channel & {
    if (this != &ch) {
        release_();
        speed_mode  = ch.speed_mode;
        channel_num = ch.channel_num;
        ch.channel_num.reset();
    }
    return *this;
}

auto ledc::channel::set_duty(std::uint32_t const duty) const -> void {
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(speed_mode, channel_num.value(), duty));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(speed_mode, channel_num.value()));
}

auto ledc::channel::set_duty_gamma(float const value) const -> void {
    if (value <= 0.0F) {
        set_duty(gamma_table.front());
        return;
    }

    if (value >= 1.0F) {
        set_duty(gamma_table.back());
        return;
    }

    auto const index{value * (gamma_table.size() - 1)};

    auto const lower{static_cast<int>(index)};
    auto const frac{index - static_cast<float>(lower)};

    set_duty(static_cast<std::uint32_t>(std::lerp(gamma_table.at(lower), gamma_table.at(lower + 1), frac)));
}

auto ledc::channel::release_() -> void {
    if (!channel_num)
        return;
    std::scoped_lock lock{slots_mutex};

    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_stop(speed_mode, channel_num.value(), 0)); // see #2
    auto &channel_slots{speed_mode == LEDC_HIGH_SPEED_MODE ? hs_channel_slots : ls_channel_slots};
    channel_slots.at(static_cast<std::uint8_t>(channel_num.value())) = false;
    channel_num.reset();
}
