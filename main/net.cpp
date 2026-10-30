#include "net.hpp"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <utility>

namespace {
    auto setsockopt(int const file, int const level, int const name, int const value) {
        ::setsockopt(file, level, name, &value, sizeof value);
    }

    auto tune(int const file, net::stream_config const &cfg) -> void {
        setsockopt(file, SOL_SOCKET, SO_KEEPALIVE, 1);
        setsockopt(file, IPPROTO_TCP, TCP_KEEPIDLE, cfg.keepalive_idle_s);
        setsockopt(file, IPPROTO_TCP, TCP_KEEPINTVL, cfg.keepalive_interval_s);
        setsockopt(file, IPPROTO_TCP, TCP_KEEPCNT, cfg.keepalive_count);
        setsockopt(file, IPPROTO_TCP, TCP_NODELAY, 1);
    }

    auto send_all(int const file, std::span<std::byte const> data, int const flags) -> bool {
        while (!data.empty()) {
            auto const sent{send(file, data.data(), data.size(), flags)};
            if (sent <= 0)
                return false;
            data = data.subspan(static_cast<std::size_t>(sent));
        }
        return true;
    }
}

auto net::stream::create(std::uint32_t const ip_network_order, std::uint16_t const port, stream_config const &cfg)
    -> std::optional<stream> {
    stream result;
    result.file_ = socket(AF_INET, SOCK_STREAM, 0);
    if (result.file_ < 0)
        return std::nullopt;

    sockaddr_in const address{.sin_family{AF_INET}, .sin_port{htons(port)}, .sin_addr{.s_addr{ip_network_order}}};
    if (connect(result.file_, reinterpret_cast<sockaddr const *>(&address), sizeof address) < 0)
        return std::nullopt;

    tune(result.file_, cfg);
    return result;
}

net::stream::~stream() {
    release_();
}

net::stream::stream(stream &&other) noexcept : file_{std::exchange(other.file_, -1)} {}

auto net::stream::operator=(stream &&other) noexcept -> stream & {
    if (this != &other) {
        release_();
        file_ = std::exchange(other.file_, -1);
    }
    return *this;
}

auto net::stream::send(std::span<std::byte const> const payload) const -> bool {
    if (payload.size() > max_payload_len)
        return false;

    auto const       size{static_cast<std::uint16_t>(payload.size())};
    std::array const header{
        static_cast<std::byte>(size >> 8),
        static_cast<std::byte>(size & 0xFF),
    };

    auto const flags{payload.empty() ? 0 : MSG_MORE};
    return send_all(file_, header, flags) && send_all(file_, payload, 0);
}

auto net::stream::receive() -> std::optional<std::span<std::byte const>> {
    std::array<std::byte, 2> header{};
    if (!receive_exactly_(header))
        return std::nullopt;

    auto const size{(std::to_integer<std::size_t>(header.at(0)) << 8) | std::to_integer<std::size_t>(header.at(1))};
    if (size > max_payload_len)
        return std::nullopt;

    payload_.resize(size);
    if (!receive_exactly_(payload_))
        return std::nullopt;
    return std::span<std::byte const>{payload_};
}

auto net::stream::adopt_(int const file, stream_config const &cfg) -> stream {
    stream result;
    result.file_ = file;
    tune(file, cfg);
    return result;
}

auto net::stream::receive_exactly_(std::span<std::byte> destination) const -> bool {
    while (!destination.empty()) {
        auto const received{recv(file_, destination.data(), destination.size(), 0)};
        if (received <= 0)
            return false;
        destination = destination.subspan(static_cast<std::size_t>(received));
    }
    return true;
}

auto net::stream::release_() -> void {
    if (file_ >= 0)
        close(file_);
    file_ = -1;
}

auto net::listener::create(listener_config const &cfg) -> std::optional<listener> {
    listener result;
    result.file_ = socket(AF_INET, SOCK_STREAM, 0);
    if (result.file_ < 0)
        return std::nullopt;

    setsockopt(result.file_, SOL_SOCKET, SO_REUSEADDR, 1);

    sockaddr_in const address{
        .sin_family{AF_INET},
        .sin_port{htons(cfg.port)},
        .sin_addr{.s_addr{htonl(INADDR_ANY)}},
    };

    if (bind(result.file_, reinterpret_cast<sockaddr const *>(&address), sizeof address) < 0)
        return std::nullopt;
    if (listen(result.file_, cfg.backlog) < 0)
        return std::nullopt;
    return result;
}

net::listener::~listener() {
    release_();
}

net::listener::listener(listener &&other) noexcept : file_{std::exchange(other.file_, -1)} {}

auto net::listener::operator=(listener &&other) noexcept -> listener & {
    if (this != &other) {
        release_();
        file_ = std::exchange(other.file_, -1);
    }
    return *this;
}

auto net::listener::accept(stream_config const &cfg) const -> std::optional<stream> {
    int const file{::accept(file_, nullptr, nullptr)};
    if (file < 0)
        return std::nullopt;
    return stream::adopt_(file, cfg);
}

auto net::listener::release_() -> void {
    if (file_ >= 0)
        close(file_);
    file_ = -1;
}
