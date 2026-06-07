#pragma once
#include <chrono>

class PerfTimer {
public:
    PerfTimer() { reset(); }

    void reset() {
        start_ = Clock::now();
    }

    long long ns() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
    }

    double ms() const { return ns() / 1'000'000.0; }

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_;
};
