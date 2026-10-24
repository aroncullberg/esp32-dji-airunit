// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dji::detail {
struct MspFrame {
    char direction = '<';
    std::uint8_t command = 0, size = 0;
    std::array<std::uint8_t, 255> payload{};
};
// One bounded output batch, smaller than the IDF TX ring. Nothing allocates.
struct TxBatch {
    std::array<std::uint8_t, 384> bytes{};
    std::size_t size = 0;
    std::size_t remaining() const { return bytes.size() - size; }
    bool frame(char direction, std::uint8_t command, const std::uint8_t* data,
               std::size_t length);
    bool display(std::uint8_t subcommand) { return frame('>', 182, &subcommand, 1); }
};
class MspParser {
public:
    bool feed(std::uint8_t byte, std::int64_t now, MspFrame& output);
    void reset() { state_ = 0; index_ = 0; }
    std::uint64_t errors = 0;
private:
    MspFrame frame_{};
    unsigned state_ = 0, index_ = 0;
    std::uint8_t checksum_ = 0;
    std::int64_t last_byte_us_ = 0;
};
} // namespace dji::detail
