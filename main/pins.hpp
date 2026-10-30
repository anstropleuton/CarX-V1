#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "hal/dac_types.h"

#include <optional>

constexpr auto cmd_11 = GPIO_NUM_11;
constexpr auto sd3_10 = GPIO_NUM_10;
constexpr auto sd2_9  = GPIO_NUM_9;
constexpr auto p13    = GPIO_NUM_13;
constexpr auto p12    = GPIO_NUM_12;
constexpr auto p14    = GPIO_NUM_14;
constexpr auto p27    = GPIO_NUM_27;
constexpr auto p26    = GPIO_NUM_26;
constexpr auto p25    = GPIO_NUM_25;
constexpr auto p33    = GPIO_NUM_33;
constexpr auto p32    = GPIO_NUM_32;
constexpr auto p35    = GPIO_NUM_35;
constexpr auto p34    = GPIO_NUM_34;
constexpr auto svn_39 = GPIO_NUM_39;
constexpr auto svp_36 = GPIO_NUM_36;
constexpr auto clk_6  = GPIO_NUM_6;
constexpr auto sd0_7  = GPIO_NUM_7;
constexpr auto sd1_8  = GPIO_NUM_8;
constexpr auto p15    = GPIO_NUM_15;
constexpr auto p2     = GPIO_NUM_2;
constexpr auto p0     = GPIO_NUM_0;
constexpr auto p4     = GPIO_NUM_4;
constexpr auto p16    = GPIO_NUM_16;
constexpr auto p17    = GPIO_NUM_17;
constexpr auto p5     = GPIO_NUM_5;
constexpr auto p18    = GPIO_NUM_18;
constexpr auto p19    = GPIO_NUM_19;
constexpr auto p21    = GPIO_NUM_21;
constexpr auto rx_3   = GPIO_NUM_3;
constexpr auto tx_1   = GPIO_NUM_1;
constexpr auto p22    = GPIO_NUM_22;
constexpr auto p23    = GPIO_NUM_23;

constexpr auto vp_adc_0  = ADC_CHANNEL_0;
constexpr auto vn_adc_3  = ADC_CHANNEL_3;
constexpr auto p32_adc_4 = ADC_CHANNEL_4;
constexpr auto p33_adc_5 = ADC_CHANNEL_5;
constexpr auto p34_adc_6 = ADC_CHANNEL_6;
constexpr auto p35_adc_7 = ADC_CHANNEL_7;

constexpr auto p25_dac_0 = DAC_CHAN_0;
constexpr auto p26_dac_1 = DAC_CHAN_1;

[[nodiscard]]
constexpr auto adc_from_gpio(gpio_num_t const gpio) -> std::optional<adc_channel_t> {
    switch (gpio) {
        case svp_36:
            return vp_adc_0;
        case svn_39:
            return vn_adc_3;
        case p32:
            return p32_adc_4;
        case p33:
            return p33_adc_5;
        case p34:
            return p34_adc_6;
        case p35:
            return p35_adc_7;
        default:
            return std::nullopt;
    }
}

[[nodiscard]]
constexpr auto dac_from_gpio(gpio_num_t const gpio) -> std::optional<dac_channel_t> {
    switch (gpio) {
        case p25:
            return p25_dac_0;
        case p26:
            return p26_dac_1;
        default:
            return std::nullopt;
    }
}

[[nodiscard]]
constexpr auto gpio_can_output(gpio_num_t const gpio) { // Excludes input-only & special
    switch (gpio) {
        case svp_36:
        case svn_39:
        case p34:
        case p35:
        case cmd_11:
        case sd3_10:
        case sd2_9:
        case clk_6:
        case sd0_7:
        case sd1_8:
        case rx_3:
        case tx_1:
            return false;
        default:
            return true;
    }
}

[[nodiscard]]
constexpr auto gpio_can_input(gpio_num_t const gpio) { // Excludes strapping (treated as output-only) & special
    switch (gpio) {
        // p0 is not excluded, since it can be used as button
        case p2:
        case p5:
        case p12:
        case p15:
        case cmd_11:
        case sd3_10:
        case sd2_9:
        case clk_6:
        case sd0_7:
        case sd1_8:
        case rx_3:
        case tx_1:
            return false;
        default:
            return true;
    }
}
