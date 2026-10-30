#include "button.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pins.hpp"

constexpr auto delay_us{10'000}; // 10ms

auto button::button::create(config const &cfg) -> std::optional<button> {
    gpio_config_t button_gpio_config{
        .pin_bit_mask{1ULL << cfg.button},
        .mode{GPIO_MODE_INPUT},
    };

    if (gpio_can_output(cfg.button)) {
        if (cfg.active_low)
            button_gpio_config.pull_up_en = GPIO_PULLUP_ENABLE;
        else
            button_gpio_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    } else {
        ESP_LOGW(
            "button", "GPIO%d needs an external pull-%s resistor (<= 40k)", cfg.button, cfg.active_low ? "up" : "down");
    }

    ESP_ERROR_CHECK(gpio_config(&button_gpio_config));

    button btn{};
    btn.last_edge_us_ = -delay_us;
    return btn;
}

auto button::button::update(config const &cfg) -> void {
    auto const now{esp_timer_get_time()};
    auto const down{(gpio_get_level(cfg.button) != 0) != cfg.active_low};

    prev_stable_down_ = stable_down_;

    if (down != was_down_) {
        was_down_     = down;
        last_edge_us_ = now;
    } else if (now - last_edge_us_ >= delay_us) {
        stable_down_ = down;
    }
}

auto button::button::down() const -> bool {
    return stable_down_;
}

auto button::button::up() const -> bool {
    return !stable_down_;
}

auto button::button::pressed() const -> bool {
    return stable_down_ && !prev_stable_down_;
}

auto button::button::released() const -> bool {
    return !stable_down_ && prev_stable_down_;
}
