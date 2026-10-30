// clang-format off
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
// clang-format on

#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "pins.hpp"

namespace {
    // ...
}

extern "C" [[noreturn]] auto app_main() -> void {
    // Heart beat
    constexpr gpio_config_t heart_beat_led{
        .pin_bit_mask{1ULL << p2},
        .mode{GPIO_MODE_OUTPUT},
    };
    gpio_config(&heart_beat_led);

    while (true) {
        gpio_set_level(p2, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(p2, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
