// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include "esp_err.h"

namespace dji {
class AirUnit;
namespace detail { class Canvas; class Engine; }
struct Glyph {
    std::uint16_t index = ' '; // 0..1023: four font pages; low byte 0 unsupported
    bool blink = false;
};
struct TextStyle {
    std::uint8_t font = 0;
    bool blink = false;
};
struct Cell {
    std::uint8_t character = ' ', attributes = 0;
    bool operator==(Cell other) const {
        return character == other.character && attributes == other.attributes;
    }
    bool operator!=(Cell other) const { return !(*this == other); }
};
// Fixed-storage drawing buffer. Drawing never touches a UART or holds an owner
// lock. Only a frame returned by osd().begin_frame() can commit itself.
class Frame {
public:
    static constexpr unsigned max_columns = 60, max_rows = 22;
    explicit Frame(unsigned columns = 53, unsigned rows = 20);
    unsigned columns() const { return columns_; }
    unsigned rows() const { return rows_; }
    bool valid() const { return columns_ && rows_; }
    bool clear(Glyph fill = {});
    bool glyph(int x, int y, Glyph value);
    // ASCII, not UTF-8. Clips at the canvas edges. False means invalid input or
    // clipping; in-bounds valid characters are still drawn.
    bool text(int x, int y, std::string_view value, TextStyle style = {});
    Cell at(unsigned x, unsigned y) const;
    // Copies a complete snapshot to the owner, without waiting on UART. No
    // implicit commit on destruction. Owner must outlive frames used to commit.
    esp_err_t commit() const;
private:
    friend class AirUnit;
    friend class detail::Canvas;
    friend class detail::Engine;
    void reset(unsigned columns, unsigned rows);
    std::array<Cell, max_columns * max_rows> cells_{};
    unsigned columns_ = 0, rows_ = 0;
    AirUnit* owner_ = nullptr;
    std::uint64_t session_ = 0;
};
} // namespace dji
