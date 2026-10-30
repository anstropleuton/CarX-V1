#pragma once

#include "mcpwm.hpp"
#include "soc/gpio_num.h"

#include <optional>

namespace proximity {
    struct config final {
        gpio_num_t          trigger{};
        mcpwm::channel     *channel{};
        mcpwm::timer const *timer{};
    };

    struct isr_state;

    class proximity final {
    public:
        [[nodiscard]] static auto create(config const &cfg) -> std::optional<proximity>;

        [[nodiscard]] auto measure_cm(config const &cfg) const -> std::optional<float>;

    private:
        proximity() = default;

        [[nodiscard]] static auto capture_callback_(
            mcpwm_cap_channel_handle_t, mcpwm_capture_event_data_t const *event, void *state_ptr) -> bool;

        isr_state *state_{};
    };
}
