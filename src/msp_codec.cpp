// SPDX-License-Identifier: GPL-3.0-only
#include "msp_codec.hpp"

namespace dji::detail {
bool TxBatch::frame(char direction, std::uint8_t command, const std::uint8_t* data,
                    std::size_t length) {
    if (length > 255 || remaining() < length + 6 || (length && !data) ||
        (direction != '<' && direction != '>' && direction != '!')) return false;
    bytes[size++] = '$'; bytes[size++] = 'M'; bytes[size++] = direction;
    bytes[size++] = static_cast<std::uint8_t>(length); bytes[size++] = command;
    auto checksum = static_cast<std::uint8_t>(length ^ command);
    for (std::size_t i = 0; i < length; ++i) {
        bytes[size++] = data[i]; checksum ^= data[i];
    }
    bytes[size++] = checksum;
    return true;
}
bool MspParser::feed(std::uint8_t byte, std::int64_t now, MspFrame& output) {
    if (state_ && (now < last_byte_us_ || now - last_byte_us_ > 50000)) {
        ++errors;
        reset();
    }
    last_byte_us_ = now;
    switch (state_) {
    case 0: state_ = byte == '$' ? 1 : 0; break;
    case 1: state_ = byte == 'M' ? 2 : (byte == '$' ? 1 : 0); break;
    case 2:
        if (byte == '<' || byte == '>' || byte == '!') {
            frame_.direction = static_cast<char>(byte); state_ = 3;
        } else { ++errors; state_ = byte == '$' ? 1 : 0; }
        break;
    case 3: frame_.size = checksum_ = byte; index_ = 0; state_ = 4; break;
    case 4:
        frame_.command = byte; checksum_ ^= byte;
        state_ = frame_.size ? 5 : 6;
        break;
    case 5:
        frame_.payload[index_++] = byte; checksum_ ^= byte;
        if (index_ == frame_.size) state_ = 6;
        break;
    case 6:
        reset();
        if (checksum_ == byte) { output = frame_; return true; }
        ++errors;
        state_ = byte == '$' ? 1 : 0;
        break;
    }
    return false;
}
} // namespace dji::detail
