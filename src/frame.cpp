// SPDX-License-Identifier: GPL-3.0-only
#include "dji/frame.hpp"
#include <algorithm>

namespace dji {
namespace {
bool encode(Glyph glyph, Cell& cell) {
    if (glyph.index > 1023 || (glyph.index & 255) == 0) return false;
    cell.character = static_cast<std::uint8_t>(glyph.index);
    cell.attributes = static_cast<std::uint8_t>((glyph.index >> 8) | (glyph.blink ? 0x40 : 0));
    return true;
}
}
Frame::Frame(unsigned columns, unsigned rows) { reset(columns, rows); }
void Frame::reset(unsigned columns, unsigned rows) {
    columns_ = rows_ = 0;
    if (columns && columns <= max_columns && rows && rows <= max_rows) {
        columns_ = columns; rows_ = rows;
    }
    owner_ = nullptr; session_ = 0;
    cells_.fill(Cell{});
}
bool Frame::clear(Glyph fill) {
    Cell cell;
    if (!valid() || !encode(fill, cell)) return false;
    cells_.fill(cell);
    return true;
}
bool Frame::glyph(int x, int y, Glyph value) {
    Cell cell;
    if (!encode(value, cell) || x < 0 || y < 0 || unsigned(x) >= columns_ || unsigned(y) >= rows_)
        return false;
    cells_[unsigned(y) * columns_ + unsigned(x)] = cell;
    return true;
}
bool Frame::text(int x, int y, std::string_view value, TextStyle style) {
    if (style.font > 3 || y < 0 || unsigned(y) >= rows_) return false;
    if (value.empty()) return true;
    const auto skip = x < 0 ? static_cast<std::uint64_t>(-std::int64_t(x)) : 0;
    if (skip >= value.size() || (x >= 0 && unsigned(x) >= columns_)) return false;
    const unsigned start = x < 0 ? 0 : unsigned(x);
    const auto count = std::min<std::size_t>(columns_ - start, value.size() - skip);
    bool complete = skip == 0 && count == value.size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto code = static_cast<unsigned char>(value[skip + i]);
        if (code < 32 || code > 126) { complete = false; continue; }
        const auto index = static_cast<std::uint16_t>(unsigned(style.font) * 256 + code);
        glyph(int(start + i), y, Glyph{index, style.blink});
    }
    return complete;
}
Cell Frame::at(unsigned x, unsigned y) const {
    return x < columns_ && y < rows_ ? cells_[y * columns_ + x] : Cell{};
}
} // namespace dji
