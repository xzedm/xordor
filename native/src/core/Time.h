#pragma once
// Time: a real-time clock, and the fixed-timestep accumulator.
//
// The accumulator idea ("Fix Your Timestep!", Glenn Fiedler, 2004):
//
//   real time arrives in uneven frames:   |--16.9ms--|---33.1ms---|-8.2ms-|
//   simulation consumes fixed steps:      |16.7|16.7|16.7|16.7|16.7|...
//
// Each frame, the real time that passed is added to an "accumulator". While the
// accumulator holds at least one step, we run one fixed update and subtract a
// step. What is left over (less than one step) carries into the next frame.
// The leftover divided by the step size is "alpha": how far we are between the
// last simulated state and the next one, used to interpolate when rendering.

#include <chrono>

namespace Engine {

// Measures real elapsed time with a steady (never-jumping) clock.
// system_clock can jump when the user or NTP changes the time; steady_clock
// only moves forward, which is what timing code needs.
class Clock {
public:
    Clock();
    // Seconds since the clock was created.
    double elapsedSeconds() const;
    // Seconds since the previous tick() (or since creation for the first call).
    double tick();

private:
    using SteadyClock = std::chrono::steady_clock;
    SteadyClock::time_point start_;
    SteadyClock::time_point last_;
};

class FixedTimestep {
public:
    // stepSeconds: length of one fixed update (1/60 for 60 Hz).
    // maxStepsPerFrame: upper limit of fixed updates in one frame. Without it,
    //   a slow frame causes many updates, which make the next frame slower
    //   still (the "spiral of death"). Past the limit the simulation runs
    //   slower than real time instead of freezing the program.
    // maxFrameSeconds: frames longer than this (a breakpoint, dragging the
    //   window) count as this long, so the game does not try to catch up on them.
    explicit FixedTimestep(double stepSeconds = 1.0 / 60.0, int maxStepsPerFrame = 8, double maxFrameSeconds = 0.25);

    // Adds one frame of real time. Returns how many fixed updates to run now.
    int advance(double frameSeconds);

    // Fraction of a step left in the accumulator, in [0, 1).
    double alpha() const;

    double stepSeconds() const { return step_; }
    // Changing the step keeps the leftover time, scaled to the new step.
    void setStepSeconds(double stepSeconds);

    long long totalSteps() const { return totalSteps_; }
    // Steps skipped because of maxStepsPerFrame, so the app can report slowdowns.
    long long droppedSteps() const { return droppedSteps_; }

private:
    double step_;
    int maxStepsPerFrame_;
    double maxFrameSeconds_;
    double accumulator_ = 0.0;
    long long totalSteps_ = 0;
    long long droppedSteps_ = 0;
};

} // namespace Engine
