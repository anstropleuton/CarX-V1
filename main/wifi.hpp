#pragma once

#include "esp_netif_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace wifi {
    constexpr std::size_t max_ssid_len{32};
    constexpr std::size_t max_password_len{63};

    struct ap_config final {
        std::string_view ssid;
        std::string_view password; // Empty means an open network, otherwise 8 to 63 characters
        std::uint8_t     channel{1};
        std::uint8_t     max_connections{1};
    };

    struct sta_config final {
        std::string_view ssid;
        std::string_view password; // Empty means an open network
    };

    // Needs nvs::setup() first
    auto setup() -> void;

    auto ap_start(ap_config const &cfg) -> void;

    auto ap_stop() -> void;

    // Keeps retrying in the background
    auto sta_connect(sta_config const &cfg) -> void;

    // Blocks until the station has an IP or the timeout passes
    [[nodiscard]] auto sta_wait(std::uint32_t timeout_ms) -> bool;

    auto sta_disconnect() -> void;

    [[nodiscard]] auto sta_connected() -> bool;

    [[nodiscard]] auto sta_ip_info() -> std::optional<esp_netif_ip_info_t>;

    [[nodiscard]] auto sta_rssi_dbm() -> std::optional<int>;
}
