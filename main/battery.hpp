#pragma once

#include "soc/gpio_num.h"
#include "utilities.hpp"

namespace battery {
    constexpr auto full_v{8.4F};
    constexpr auto empty_v{6.0F};

    struct config final {
        inplace_vector<gpio_num_t, 4> probes;
    };

    class battery final {
    public:
        [[nodiscard]] static auto create(config const &cfg) -> std::optional<battery>;

        auto update(config const &cfg) -> void;

        auto get_soc(std::size_t index) -> float;

    private:
        inplace_vector<float, 4> socs_;
        bool                     low_{};
        bool                     blink_on_{};

        [[nodiscard]] static auto volt_to_soc_(float volt) -> float;
    };
}
