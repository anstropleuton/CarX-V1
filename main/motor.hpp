#pragma once

#include "hal/ledc_types.h"
#include "ledc.hpp"

#include <optional>

namespace motor {
    constexpr auto speed_mode{LEDC_LOW_SPEED_MODE};
    constexpr auto duty_res{LEDC_TIMER_8_BIT};

    enum class direction : std::uint8_t { neutral, forward, reverse };

    struct config final {
        std::uint8_t pwm_duty{};

        ledc::channel const *forward{};
        ledc::channel const *reverse{};
    };

    class motor final {
    public:
        [[nodiscard]] static auto create(config const &cfg) -> std::optional<motor>;

        [[nodiscard]] auto get_dir() const -> direction;

        auto set_dir(direction const &dir, config const &cfg) -> void;

    private:
        direction dir_{};

        static auto set_driver_(direction dir, config const &cfg) -> void;
    };
}