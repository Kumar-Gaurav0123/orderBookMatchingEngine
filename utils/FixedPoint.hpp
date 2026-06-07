#pragma once

#include <cstdint>
#include <cmath>
#include <stdexcept>

class FixedPoint {
public:
    using storage_type = int64_t;

private:
    storage_type value_;
    static constexpr storage_type SCALE = 10000; // 4 decimal places (configurable later)

public:
    FixedPoint() : value_(0) {}
    explicit FixedPoint(double v) : value_(static_cast<storage_type>(std::round(v * SCALE))) {}

    double toDouble() const {
        return static_cast<double>(value_) / SCALE;
    }

    storage_type raw() const noexcept { return value_; }

    // Operators
    FixedPoint operator+(const FixedPoint& other) const { return FixedPoint::fromRaw(value_ + other.value_); }
    FixedPoint operator-(const FixedPoint& other) const { return FixedPoint::fromRaw(value_ - other.value_); }
    bool operator<(const FixedPoint& other) const noexcept { return value_ < other.value_; }
    bool operator>(const FixedPoint& other) const noexcept { return value_ > other.value_; }
    bool operator==(const FixedPoint& other) const noexcept { return value_ == other.value_; }

    static FixedPoint fromRaw(storage_type raw) {
        FixedPoint fp;
        fp.value_ = raw;
        return fp;
    }
};
