#include "core/Time.h"

#include "core/Assert.h"

#include <algorithm>

namespace Engine {

Clock::Clock() : start_(SteadyClock::now()), last_(start_) {}

double Clock::elapsedSeconds() const {
    return std::chrono::duration<double>(SteadyClock::now() - start_).count();
}

double Clock::tick() {
    const auto now = SteadyClock::now();
    const double seconds = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    return seconds;
}

FixedTimestep::FixedTimestep(double stepSeconds, int maxStepsPerFrame, double maxFrameSeconds)
    : step_(stepSeconds), maxStepsPerFrame_(maxStepsPerFrame), maxFrameSeconds_(maxFrameSeconds) {
    ENGINE_ASSERT(stepSeconds > 0.0, "the fixed step must be positive");
    ENGINE_ASSERT(maxStepsPerFrame >= 1, "at least one step per frame must be allowed");
}

int FixedTimestep::advance(double frameSeconds) {
    // Negative time can't happen with a steady clock, but a caller could pass it.
    frameSeconds = std::clamp(frameSeconds, 0.0, maxFrameSeconds_);
    accumulator_ += frameSeconds;

    // Most step sizes can't be stored exactly in binary (0.01 is really
    // 0.01000000000000000020816...). After a few subtractions, "exactly five
    // steps of time" can leave 0.0099999999 in the accumulator instead of 0.01,
    // and a strict >= would run only four steps. A tolerance of a millionth of
    // a step counts "a whole step minus rounding error" as a whole step.
    const double tolerance = step_ * 1e-6;

    int steps = 0;
    while (accumulator_ + tolerance >= step_ && steps < maxStepsPerFrame_) {
        accumulator_ -= step_;
        ++steps;
    }
    // The tolerance can take the accumulator a hair below zero.
    accumulator_ = std::max(accumulator_, 0.0);

    // Still more than a step behind after the limit: throw the extra time away.
    // Keeping it would make every following frame hit the limit too.
    if (accumulator_ + tolerance >= step_) {
        const auto extra = static_cast<long long>((accumulator_ + tolerance) / step_);
        droppedSteps_ += extra;
        accumulator_ = std::max(accumulator_ - static_cast<double>(extra) * step_, 0.0);
    }
    totalSteps_ += steps;
    return steps;
}

double FixedTimestep::alpha() const {
    return std::clamp(accumulator_ / step_, 0.0, 1.0);
}

void FixedTimestep::setStepSeconds(double stepSeconds) {
    ENGINE_ASSERT(stepSeconds > 0.0, "the fixed step must be positive");
    // Keep alpha the same, so rendering does not jump when the rate changes.
    accumulator_ = alpha() * stepSeconds;
    step_ = stepSeconds;
}

} // namespace Engine
