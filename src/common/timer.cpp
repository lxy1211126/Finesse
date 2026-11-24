#include "common/timer.h"

namespace finesse {

Timer::Timer() : start_(Clock::now()) {}

double Timer::elapsed_ms() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - start_)
        .count();
}

}  // namespace finesse
