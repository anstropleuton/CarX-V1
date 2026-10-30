#include "adc.hpp"

#include <mutex>

namespace {
    adc_oneshot_unit_handle_t oneshot_handle{};
    adc_cali_handle_t         fitting_handle{};
    std::mutex                adc_mutex;
    std::once_flag            adc_init_flag;

    auto adc_initialize() {
        call_once(adc_init_flag, [] {
            constexpr adc_oneshot_unit_init_cfg_t oneshot_config{
                .unit_id{adc::unit},
                .clk_src{ADC_RTC_CLK_SRC_DEFAULT},
            };

            ESP_ERROR_CHECK(adc_oneshot_new_unit(&oneshot_config, &oneshot_handle));

            constexpr adc_cali_line_fitting_config_t fitting_config{
                .unit_id{adc::unit},
                .atten{adc::atten},
                .bitwidth{adc::bitwidth},
            };
            ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&fitting_config, &fitting_handle));
        });
    }
}

auto adc::get_oneshot_handle() -> adc_oneshot_unit_handle_t {
    adc_initialize();

    return oneshot_handle;
}

auto adc::get_fitting_handle() -> adc_cali_handle_t {
    adc_initialize();

    return fitting_handle;
}

auto adc::setup(adc_channel_t const channel) -> void {
    adc_initialize();

    constexpr adc_oneshot_chan_cfg_t ch_config{
        .atten{atten},
        .bitwidth{bitwidth},
    };

    std::scoped_lock lock{adc_mutex};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc::get_oneshot_handle(), channel, &ch_config));
}

auto adc::measure_mv(adc_channel_t const channel) -> int {
    std::scoped_lock lock{adc_mutex};

    int raw_read{};
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_oneshot_read(adc::get_oneshot_handle(), channel, &raw_read));

    int volt_read{};
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_cali_raw_to_voltage(adc::get_fitting_handle(), raw_read, &volt_read));

    return volt_read;
}
