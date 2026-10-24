// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dji/frame.hpp"

namespace dji {
class AirUnit;
// A borrowed view. Never retain it beyond its AirUnit's lifetime.
class Osd {
public:
    // Blank frame at the latest known canvas dimensions. Layout belongs to the
    // application. Commit explicitly; discarding a frame changes nothing.
    Frame begin_frame() const;
private:
    friend class AirUnit;
    explicit Osd(AirUnit& owner) : owner_(owner) {}
    AirUnit& owner_;
};
} // namespace dji
