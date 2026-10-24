// SPDX-License-Identifier: GPL-3.0-only
#include "canvas.hpp"
#include <algorithm>
#include <cstring>

namespace dji::detail {
void Canvas::begin(const Frame& desired) {
    target_ = desired; cursor_ = 0; active_ = true; changed_ = false;
    clear_pending_ = invalid_ || sent_.columns() != target_.columns() || sent_.rows() != target_.rows();
}
bool Canvas::append(TxBatch& batch) {
    if (!active_) return false;
    if (clear_pending_) {
        if (batch.remaining() < 14 || !batch.display(2)) return false;
        sent_.reset(target_.columns(), target_.rows());
        clear_pending_ = false; invalid_ = false; changed_ = true;
    }
    const auto total = target_.columns() * target_.rows();
    while (cursor_ < total) {
        if (target_.cells_[cursor_] == sent_.cells_[cursor_]) { ++cursor_; continue; }
        const auto start = cursor_;
        const auto column = start % target_.columns();
        const auto row = start / target_.columns();
        const auto attributes = target_.cells_[start].attributes;
        // Reserve seven bytes for DRAW; each write has six MSP + four DP bytes.
        if (batch.remaining() < 18) return false;
        const auto limit = std::min<std::size_t>({30, target_.columns() - column,
                                                batch.remaining() - 17});
        std::size_t count = 0;
        while (count < limit && target_.cells_[start + count] != sent_.cells_[start + count] &&
               target_.cells_[start + count].attributes == attributes) ++count;
        std::uint8_t payload[34]{3, static_cast<std::uint8_t>(row),
                                 static_cast<std::uint8_t>(column), attributes};
        for (std::size_t i = 0; i < count; ++i) payload[4 + i] = target_.cells_[start + i].character;
        if (!batch.frame('>', 182, payload, count + 4)) return false;
        std::copy_n(target_.cells_.begin() + start, count, sent_.cells_.begin() + start);
        cursor_ += count; changed_ = true;
    }
    if (changed_ && !batch.display(4)) return false;
    active_ = false;
    return changed_;
}
} // namespace dji::detail
