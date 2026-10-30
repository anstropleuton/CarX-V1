#include "battery.hpp"

#include "adc.hpp"
#include "pins.hpp"
#include "soc_table.hpp"

#include <array>
#include <cmath>
#include <ranges>

constexpr auto samples{4};

namespace {
    auto adc_to_battery(float const volt) {
        return 8.833F * volt - 6.05F;
    }
}

auto battery::battery::create(config const &cfg) -> std::optional<battery> {
    for (gpio_num_t const probe : cfg.probes)
        adc::setup(adc_from_gpio(probe).value());

    battery ind{};
    ind.socs_.resize(cfg.probes.size());
    return ind;
}

auto battery::battery::update(config const &cfg) -> void {
    float min_soc{std::numeric_limits<float>::max()};

    for (auto const &[index, probe] : cfg.probes | std::views::enumerate) {
        int sampled{};

        for ([[maybe_unused]] auto _ : std::views::iota(0, samples))
            sampled += adc::measure_mv(adc_from_gpio(probe).value());

        auto const volt{adc_to_battery(static_cast<float>(sampled) / 1000.0F / samples)};

        auto const soc{volt_to_soc_(volt)};
        socs_.at(index) = soc;

        min_soc = std::min(min_soc, soc);
    }
}

auto battery::battery::get_soc(std::size_t const index) -> float {
    return socs_.at(index);
}

auto battery::battery::volt_to_soc_(float const volt) -> float {
    if (volt <= empty_v)
        return 0.0F;

    if (volt >= full_v)
        return 1.0F;

    auto const index{(volt - empty_v) / (full_v - empty_v) * (soc_table.size() - 1)};

    auto const lower{static_cast<int>(index)};
    auto const frac{index - static_cast<float>(lower)};

    return std::lerp(soc_table.at(lower), soc_table.at(lower + 1), frac);
}
