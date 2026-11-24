#pragma once

#include <chrono>

namespace finesse {

class Timer {
public:
    using Clock = std::chrono::steady_clock;

    Timer();
    double elapsed_ms() const;

private:
    Clock::time_point start_;
};

}  // namespace finesse
