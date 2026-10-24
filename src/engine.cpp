// SPDX-License-Identifier: GPL-3.0-only
#include "engine.hpp"
#include "dji_protocol.hpp"

namespace dji::detail {
void Engine::start(const Config& config) {
    // Avoid a multi-kilobyte temporary on the application's startup stack.
    parser_.reset(); parser_.errors = 0; canvas_.invalidate();
    status_ = {}; head_ = count_ = 0;
    status_due_ = heartbeat_due_ = display_due_ = repaint_due_ = battery_due_ = 0;
    pending_bytes_ = 0; pending_draw_ = pending_status_ = pending_arm_ = false;
    pending_battery_ = 0;
    config_ = config;
    status_.columns = config.osd.columns; status_.rows = config.osd.rows;
    desired_.reset(status_.columns, status_.rows); has_frame_ = false;
}
bool Engine::submit(const Frame& frame) {
    if (!config_.osd.enabled || !frame.valid() || frame.columns() != status_.columns ||
        frame.rows() != status_.rows) return false;
    desired_ = frame; has_frame_ = true;
    return true;
}
void Engine::discontinuity() {
    parser_.reset(); ++status_.rx_errors;
}
void Engine::feed(const std::uint8_t* data, std::size_t size, std::int64_t now) {
    status_.rx_bytes += size;
    if (size) status_.last_rx_us = now;
    MspFrame frame;
    for (std::size_t i = 0; i < size; ++i) {
        if (parser_.feed(data[i], now, frame)) {
            ++status_.rx_frames;
            status_.last_rx_command = frame.command;
            receive(frame);
        }
    }
    status_.parser_errors = parser_.errors;
}
void Engine::receive(const MspFrame& frame) {
    if (frame.direction != '<') return; // don't reply to replies/errors/echo
    Reply reply{frame.command, false};
    switch (frame.command) {
    case 101: case 150: case 189:
        reply.error = frame.size != 0;
        break;
    case 130:
        ++status_.battery_requests;
        reply.error = !config_.native_battery || frame.size != 0;
        break;
    case 188:
        if (frame.size != 2 || frame.payload[0] < 1 || frame.payload[0] > Frame::max_columns ||
            frame.payload[1] < 1 || frame.payload[1] > Frame::max_rows) {
            reply.error = true;
        } else {
            if (status_.columns != frame.payload[0] || status_.rows != frame.payload[1]) {
                has_frame_ = false;
                desired_.reset(frame.payload[0], frame.payload[1]);
            }
            status_.columns = frame.payload[0]; status_.rows = frame.payload[1];
            status_.canvas_announced = true;
            canvas_.invalidate(); display_due_ = 0;
        }
        break;
    default: reply.error = true; break;
    }
    if (count_ == replies_.size()) { ++status_.dropped_requests; return; }
    replies_[(head_ + count_) % replies_.size()] = reply; ++count_;
}
bool Engine::prepare(const StateStore& state, std::int64_t now, TxBatch& batch) {
    batch.size = 0; pending_draw_ = false; pending_status_ = false; pending_battery_ = 0;
    status_.native_battery_ready = config_.native_battery &&
        native_battery_ready(state, now, config_.telemetry_ttl_ms);
    const bool armed = state.armed.fresh(now, config_.armed_ttl_ms) && state.armed.value;
    // Status cannot be crowded out by peer requests or a display repaint.
    if (now >= status_due_ || armed != status_.reported_armed) {
        append_status(batch, 101, armed); append_status(batch, 150, armed);
        pending_status_ = true; pending_arm_ = armed; status_due_ = now + 100000;
    }
    if (config_.osd.enabled && now >= heartbeat_due_) {
        batch.display(0); heartbeat_due_ = now + 500000;
    }
    // Bounded request work even under a continuous incoming stream.
    for (unsigned i = 0; i < 4 && count_; ++i) {
        const auto reply = replies_[head_]; head_ = (head_ + 1) % replies_.size(); --count_;
        if (reply.error) batch.frame('!', reply.command, nullptr, 0);
        else if (reply.command == 101 || reply.command == 150) {
            append_status(batch, reply.command, armed);
            pending_status_ = true; pending_arm_ = armed;
        } else if (reply.command == 130) {
            if (status_.native_battery_ready) {
                append_battery(batch, state); ++pending_battery_;
            } else batch.frame('!', 130, nullptr, 0);
        } else if (reply.command == 189) {
            const std::uint8_t dimensions[]{status_.columns, status_.rows};
            batch.frame('>', 189, dimensions, 2);
        } else batch.frame('>', reply.command, nullptr, 0);
    }
    // Independent of DisplayPort/custom readout switches. Poll replies above
    // use current samples too; a periodic push also covers a quiet peer.
    if (status_.native_battery_ready && now >= battery_due_) {
        if (!pending_battery_) { append_battery(batch, state); ++pending_battery_; }
        battery_due_ = now + 200000; // 5 Hz
    }
    if (config_.osd.enabled && has_frame_) {
        if (!canvas_.active() && now >= display_due_) {
            if (now >= repaint_due_) {
                canvas_.invalidate();
                repaint_due_ = now + std::int64_t(config_.osd.full_repaint_ms) * 1000;
            }
            canvas_.begin(desired_);
            display_due_ = now + std::int64_t(config_.osd.refresh_ms) * 1000;
        }
        pending_draw_ = canvas_.append(batch);
    }
    pending_bytes_ = batch.size;
    return batch.size != 0;
}
void Engine::commit(bool success) {
    if (!pending_bytes_) return;
    if (success) {
        status_.tx_bytes += pending_bytes_;
        if (pending_draw_) ++status_.display_frames;
        if (pending_status_) status_.reported_armed = pending_arm_;
        status_.battery_tx_frames += pending_battery_;
    } else {
        ++status_.tx_errors; status_.last_error = ESP_FAIL;
        canvas_.invalidate(); display_due_ = status_due_ = heartbeat_due_ = battery_due_ = 0;
    }
    pending_bytes_ = 0;
}
} // namespace dji::detail
