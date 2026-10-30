#pragma once

#include "driver/mcpwm_types.h"
#include "soc/gpio_num.h"

#include <cstdint>
#include <optional>

namespace mcpwm {
    class timer final {
    public:
        mcpwm_cap_timer_handle_t handle{};
        int                      group_id{};

        [[nodiscard]] static auto create(std::uint32_t resolution_hz) -> std::optional<timer>;

        ~timer();
        timer(timer const &) = delete;
        timer(timer &&) noexcept;
        auto operator=(timer const &) -> timer & = delete;
        auto operator=(timer &&) noexcept -> timer &;

        auto enable() -> void;
        auto disable() -> void;
        auto start() -> void;
        auto stop() -> void;

        [[nodiscard]] auto resolution() const -> std::uint32_t;

        [[nodiscard]] auto is_enabled() const -> bool {
            return enabled_;
        }

        [[nodiscard]] auto is_running() const -> bool {
            return running_;
        }

    private:
        timer() = default;

        auto release_() -> void;

        bool enabled_{};
        bool running_{};
    };

    class channel final {
    public:
        mcpwm_cap_channel_handle_t handle{};

        [[nodiscard]] static auto create(gpio_num_t pin, timer const &tmr) -> std::optional<channel>;

        ~channel();
        channel(channel const &) = delete;
        channel(channel &&) noexcept;
        auto operator=(channel const &) -> channel & = delete;
        auto operator=(channel &&) noexcept -> channel &;

        auto enable() -> void;
        auto disable() -> void;

        // Must be called before the first enable(). Callback runs in ISR context.
        auto set_callback(mcpwm_capture_event_cb_t on_cap, void *user_data) const -> void;

        [[nodiscard]] auto is_enabled() const -> bool {
            return enabled_;
        }

        channel() = default;

    private:
        auto release_() -> void;

        bool enabled_{};
    };
}
