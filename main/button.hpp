#pragma once

#include <cstdint>
#include <optional>

#include "soc/gpio_num.h"

namespace button {
    struct config final {
        gpio_num_t button{};
        bool       active_low{};
    };

    class button final {
    public:
        [[nodiscard]] static auto create(config const &cfg) -> std::optional<button>;

        auto update(config const &cfg) -> void;

        [[nodiscard]] auto down() const -> bool;
        [[nodiscard]] auto up() const -> bool;
        [[nodiscard]] auto pressed() const -> bool;
        [[nodiscard]] auto released() const -> bool;

    private:
        std::int64_t last_edge_us_{};
        bool         was_down_{};
        bool         stable_down_{};
        bool         prev_stable_down_{};
    };
}
