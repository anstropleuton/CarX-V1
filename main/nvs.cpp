#include "nvs.hpp"

#include "esp_err.h"
#include "nvs_flash.h"

auto nvs::setup() -> void {
    if (auto const result{nvs_flash_init()};
        result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(result);
    }
}
