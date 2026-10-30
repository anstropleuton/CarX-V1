#pragma once

#include "hal/ledc_types.h"
#include "soc/gpio_num.h"

#include <cstdint>
#include <optional>

namespace ledc {
    constexpr auto gamma_duty_res{LEDC_TIMER_16_BIT};

    class timer final {
    public:
        ledc_mode_t                 speed_mode{};
        std::optional<ledc_timer_t> timer_num;

        [[nodiscard]] static auto create(ledc_mode_t speed_mode, std::uint32_t frequency, ledc_timer_bit_t duty_res)
            -> std::optional<timer>;

        ~timer();
        timer(timer const &) = delete;
        timer(timer &&) noexcept;
        auto operator=(timer const &) -> timer & = delete;
        auto operator=(timer &&) noexcept -> timer &;

        auto set_frequency(std::uint32_t frequency) const -> void;

    private:
        timer() = default;

        auto release_() -> void;
    };

    class channel final {
    public:
        ledc_mode_t                   speed_mode{};
        std::optional<ledc_channel_t> channel_num;

        [[nodiscard]] static auto create(gpio_num_t pin, std::uint32_t duty, timer const &tmr)
            -> std::optional<channel>;

        ~channel();
        channel(channel const &) = delete;
        channel(channel &&) noexcept;
        auto operator=(channel const &) -> channel & = delete;
        auto operator=(channel &&) noexcept -> channel &;

        auto set_duty(std::uint32_t duty) const -> void;

        // Uses gamma_duty_res bits for timer
        auto set_duty_gamma(float value) const -> void;

    private:
        channel() = default;

        auto release_() -> void;
    };
}
