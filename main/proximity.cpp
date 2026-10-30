#include "proximity.hpp"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mcpwm.hpp"
#include "utilities.hpp"

#include <atomic>
#include <limits>

struct proximity::isr_state {
    std::atomic<TaskHandle_t>                 waiter;
    std::atomic<std::optional<std::uint32_t>> rise_tick;
};

constexpr auto echo_timeout_ms{50};
constexpr auto speed_of_sound_cm_us{0.0348268F}; // 28 deg C

namespace {
    inplace_vector<proximity::isr_state, 4> isr_states;
}

auto proximity::proximity::create(config const &cfg) -> std::optional<proximity> {
    if (isr_states.size() == 4)
        return std::nullopt; // NOLINT(*-static-accessed-through-instance)

    gpio_config_t const trigger_config{
        .pin_bit_mask{1ULL << cfg.trigger},
        .mode{GPIO_MODE_OUTPUT},
    };
    ESP_ERROR_CHECK(gpio_config(&trigger_config));
    ESP_ERROR_CHECK(gpio_set_level(cfg.trigger, 0));

    proximity prox;
    prox.state_ = &isr_states.emplace_back();

    cfg.channel->set_callback(capture_callback_, prox.state_);

    return prox;
}

auto proximity::proximity::measure_cm(config const &cfg) const -> std::optional<float> {
    auto const resolution{cfg.timer->resolution()};
    if (resolution == 0)
        return std::nullopt;

    auto const cm_per_tick{speed_of_sound_cm_us * 1'000'000.0F / static_cast<float>(resolution) / 2.0F};

    std::uint32_t pulse_ticks{};

    xTaskNotifyWait(0, std::numeric_limits<std::uint32_t>::max(), nullptr, 0);

    state_->rise_tick.store(std::nullopt);
    state_->waiter.store(xTaskGetCurrentTaskHandle());

    cfg.channel->enable();
    if (!cfg.channel->is_enabled())
        return std::nullopt;

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(cfg.trigger, 0));
    esp_rom_delay_us(2);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(cfg.trigger, 1));
    esp_rom_delay_us(10);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(cfg.trigger, 0));

    bool const received{static_cast<bool>(
        xTaskNotifyWait(0, std::numeric_limits<std::uint32_t>::max(), &pulse_ticks, pdMS_TO_TICKS(echo_timeout_ms)))};

    cfg.channel->disable();

    state_->rise_tick.store(std::nullopt);
    state_->waiter.store(nullptr);

    if (!received)
        return std::nullopt;

    return static_cast<float>(pulse_ticks) * cm_per_tick;
}

IRAM_ATTR auto proximity::proximity::capture_callback_(
    mcpwm_cap_channel_handle_t, mcpwm_capture_event_data_t const *const event, void *const state_ptr) -> bool {
    auto *const state{static_cast<isr_state *>(state_ptr)};

    if (event->cap_edge == MCPWM_CAP_EDGE_POS) {
        state->rise_tick.store(event->cap_value);
        return false;
    }

    auto const rise{state->rise_tick.exchange(std::nullopt)};
    if (!rise)
        return false;

    if (auto const *const waiter{state->waiter.load()}; waiter == nullptr)
        return false;

    BaseType_t hp_task_woken{};
    xTaskNotifyFromISR(state->waiter, event->cap_value - *rise, eSetValueWithOverwrite, &hp_task_woken);

    return static_cast<bool>(hp_task_woken);
}
