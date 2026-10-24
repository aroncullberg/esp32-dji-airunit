// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "msp_codec.hpp"
#include "dji/frame.hpp"

namespace dji::detail {
class Canvas {
public:
    bool active() const { return active_; }
    void invalidate() { invalid_ = true; active_ = false; }
    void begin(const Frame& desired);
    // Mutates optimistic sent state. Caller MUST invalidate on failed TX.
    // Returns true only when a complete frame (including DRAW) was appended.
    bool append(TxBatch& batch);
private:
    Frame target_, sent_;
    bool invalid_ = true, active_ = false, clear_pending_ = false, changed_ = false;
    std::size_t cursor_ = 0;
};
} // namespace dji::detail
