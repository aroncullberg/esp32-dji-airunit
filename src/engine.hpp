// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "canvas.hpp"
#include "dji/config.hpp"
#include "dji/status.hpp"
#include "state_store.hpp"

namespace dji::detail {
// Single-worker core; no IDF, locks, allocation, or sensor/provider callbacks.
class Engine {
public:
    void start(const Config& config);
    void feed(const std::uint8_t* data, std::size_t size, std::int64_t now);
    void discontinuity();
    bool submit(const Frame& frame);
    bool prepare(const StateStore& state, std::int64_t now, TxBatch& batch);
    void commit(bool success);
    Status status() const { return status_; }
private:
    struct Reply { std::uint8_t command = 0; bool error = false; };
    void receive(const MspFrame& frame);
    Config config_{};
    MspParser parser_;
    Canvas canvas_;
    Frame desired_;
    bool has_frame_ = false;
    Status status_{};
    std::array<Reply, 16> replies_{};
    unsigned head_ = 0, count_ = 0;
    std::int64_t status_due_ = 0, heartbeat_due_ = 0, display_due_ = 0, repaint_due_ = 0, battery_due_ = 0;
    std::size_t pending_bytes_ = 0;
    bool pending_draw_ = false, pending_status_ = false, pending_arm_ = false;
    unsigned pending_battery_ = 0;
};
} // namespace dji::detail
