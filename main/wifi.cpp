#include "wifi.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <atomic>
#include <stdexcept>

namespace {
    esp_netif_t      *sta_netif{};
    std::atomic<bool> got_ip{};
    std::atomic<bool> want_sta{};
    bool              ap_on{};
    bool              sta_on{};
    bool              started{};

    auto event_handler(void *, esp_event_base_t const base, std::int32_t const id, void *) -> void {
        if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
            got_ip = false;
            if (want_sta)
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
        } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
            got_ip = true;
        }
    }

    auto apply_mode() -> void {
        auto const mode{static_cast<wifi_mode_t>(
            (ap_on ? WIFI_MODE_AP : WIFI_MODE_NULL) | (sta_on ? WIFI_MODE_STA : WIFI_MODE_NULL))};
        ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    }

    auto start_driver() -> void {
        if (started)
            return;
        ESP_ERROR_CHECK(esp_wifi_start());
        started = true;
    }
}

auto wifi::setup() -> void {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t const init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

auto wifi::ap_start(ap_config const &cfg) -> void {
    wifi_config_t native{};
    std::ranges::copy(cfg.ssid, native.ap.ssid);
    std::ranges::copy(cfg.password, native.ap.password);
    native.ap.ssid_len       = static_cast<std::uint8_t>(cfg.ssid.size());
    native.ap.channel        = cfg.channel;
    native.ap.authmode       = cfg.password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    native.ap.max_connection = cfg.max_connections;

    ap_on = true;
    apply_mode();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &native));
    start_driver();
}

auto wifi::ap_stop() -> void {
    ap_on = false;
    apply_mode();
}

auto wifi::sta_connect(sta_config const &cfg) -> void {
    wifi_config_t native{};
    std::ranges::copy(cfg.ssid, native.sta.ssid);
    std::ranges::copy(cfg.password, native.sta.password);

    want_sta = false;
    if (got_ip)
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    got_ip = false;

    sta_on = true;
    apply_mode();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &native));
    start_driver();

    want_sta = true;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
}

auto wifi::sta_wait(std::uint32_t const timeout_ms) -> bool {
    auto const deadline_us{esp_timer_get_time() + std::int64_t{timeout_ms} * 1'000};
    while (!got_ip && esp_timer_get_time() < deadline_us)
        vTaskDelay(pdMS_TO_TICKS(50));
    return got_ip;
}

auto wifi::sta_disconnect() -> void {
    want_sta = false;
    got_ip   = false;
    sta_on   = false;
    apply_mode();
}

auto wifi::sta_connected() -> bool {
    return got_ip;
}

auto wifi::sta_ip_info() -> std::optional<esp_netif_ip_info_t> {
    esp_netif_ip_info_t info{};
    if (!got_ip || esp_netif_get_ip_info(sta_netif, &info) != ESP_OK)
        return std::nullopt;
    return info;
}

auto wifi::sta_rssi_dbm() -> std::optional<int> {
    wifi_ap_record_t ap;
    if (!got_ip || esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
        return std::nullopt;
    return ap.rssi;
}
