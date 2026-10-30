#pragma once

#include "utilities.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace net {
    constexpr std::size_t max_payload_len{1024};

    static_assert(max_payload_len <= std::numeric_limits<std::uint16_t>::max());

    struct stream_config final {
        // Detects a vanished peer after idle_s + interval_s * count seconds
        std::uint8_t keepalive_idle_s{5};
        std::uint8_t keepalive_interval_s{5};
        std::uint8_t keepalive_count{3};
    };

    struct listener_config final {
        std::uint16_t port{};
        int           backlog{1};
    };

    class listener;

    class stream final {
    public:
        // Blocks until connected. The address is in network byte order, as lwIP reports it
        [[nodiscard]] static auto create(std::uint32_t ip_network_order, std::uint16_t port, stream_config const &cfg)
            -> std::optional<stream>;

        ~stream();
        stream(stream const &) = delete;
        stream(stream &&) noexcept;
        auto operator=(stream const &) -> stream & = delete;
        auto operator=(stream &&) noexcept -> stream &;

        // False if the payload exceeds max_payload_len or the peer is gone. Not thread-safe
        [[nodiscard]] auto send(std::span<std::byte const> payload) const -> bool;

        // Blocks until a whole frame arrived. Empty once the peer is gone, or after a frame
        // larger than max_payload_len, since the stream can't be resynchronized then.
        // The view stays valid until the next call or a move
        [[nodiscard]] auto receive() -> std::optional<std::span<std::byte const>>;

    private:
        stream() = default;

        [[nodiscard]] static auto adopt_(int file, stream_config const &cfg) -> stream;

        [[nodiscard]] auto receive_exactly_(std::span<std::byte> destination) const -> bool;

        auto release_() -> void;

        friend class listener;

        int                                        file_{-1};
        inplace_vector<std::byte, max_payload_len> payload_;
    };

    class listener final {
    public:
        [[nodiscard]] static auto create(listener_config const &cfg) -> std::optional<listener>;

        ~listener();
        listener(listener const &) = delete;
        listener(listener &&) noexcept;
        auto operator=(listener const &) -> listener & = delete;
        auto operator=(listener &&) noexcept -> listener &;

        // Blocks until a client connects
        [[nodiscard]] auto accept(stream_config const &cfg) const -> std::optional<stream>;

    private:
        listener() = default;

        auto release_() -> void;

        int file_{-1};
    };
}
