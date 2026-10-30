#pragma once

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"

namespace adc {
    constexpr auto unit{ADC_UNIT_1};
    constexpr auto atten{ADC_ATTEN_DB_12};
    constexpr auto bitwidth{ADC_BITWIDTH_12};

    [[nodiscard]] auto get_oneshot_handle() -> adc_oneshot_unit_handle_t;

    [[nodiscard]] auto get_fitting_handle() -> adc_cali_handle_t;

    auto setup(adc_channel_t channel) -> void;

    [[nodiscard]] auto measure_mv(adc_channel_t channel) -> int;
}
