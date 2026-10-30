#include "motor.hpp"

auto motor::motor::create(config const &cfg) -> std::optional<motor> {
    if (cfg.forward == nullptr || cfg.reverse == nullptr)
        return std::nullopt;

    cfg.forward->set_duty(0);
    cfg.reverse->set_duty(0);

    return motor{};
}

auto motor::motor::get_dir() const -> direction {
    return dir_;
}

auto motor::motor::set_dir(direction const &dir, config const &cfg) -> void {
    if (dir_ == dir)
        return;

    set_driver_(dir, cfg);

    dir_ = dir;
}

auto motor::motor::set_driver_(direction const dir, config const &cfg) -> void {
    switch (dir) {
        case direction::neutral:
            cfg.forward->set_duty(0);
            cfg.reverse->set_duty(0);
            break;
        case direction::forward:
            cfg.forward->set_duty(cfg.pwm_duty);
            cfg.reverse->set_duty(0);
            break;
        case direction::reverse:
            cfg.forward->set_duty(0);
            cfg.reverse->set_duty(cfg.pwm_duty);
            break;
    }
}