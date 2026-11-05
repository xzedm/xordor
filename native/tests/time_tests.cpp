#include "core/Time.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <initializer_list>

using namespace Engine;

TEST_SUITE("time") {

TEST_CASE("exactly one step of time gives one fixed update") {
    FixedTimestep ts(0.01);
    CHECK(ts.advance(0.01) == 1);
    CHECK(ts.alpha() == doctest::Approx(0.0).epsilon(1e-9));
}

TEST_CASE("short frames add up until a step is due") {
    FixedTimestep ts(0.01);
    CHECK(ts.advance(0.004) == 0);
    CHECK(ts.alpha() == doctest::Approx(0.4));
    CHECK(ts.advance(0.004) == 0);
    CHECK(ts.advance(0.004) == 1); // 0.012 in total: one step, 0.002 left over
    CHECK(ts.alpha() == doctest::Approx(0.2));
}

TEST_CASE("a long frame runs several steps and keeps the remainder") {
    FixedTimestep ts(0.01);
    CHECK(ts.advance(0.035) == 3);
    CHECK(ts.alpha() == doctest::Approx(0.5));
    CHECK(ts.totalSteps() == 3);
}

TEST_CASE("the per-frame step limit prevents the spiral of death") {
    FixedTimestep ts(0.01, /*maxStepsPerFrame=*/4, /*maxFrameSeconds=*/1.0);
    CHECK(ts.advance(0.105) == 4);  // would be 10 steps
    CHECK(ts.droppedSteps() == 6);  // the rest is thrown away...
    CHECK(ts.alpha() < 1.0);        // ...so the next frame starts fresh
    CHECK(ts.advance(0.01) == 1);
}

TEST_CASE("very long frames are capped") {
    FixedTimestep ts(0.01, /*maxStepsPerFrame=*/100, /*maxFrameSeconds=*/0.05);
    CHECK(ts.advance(3.0) == 5); // counted as 0.05 s, not 3 s
}

TEST_CASE("negative frame time is ignored") {
    FixedTimestep ts(0.01);
    CHECK(ts.advance(-1.0) == 0);
    CHECK(ts.alpha() == doctest::Approx(0.0));
}

TEST_CASE("changing the step keeps alpha") {
    FixedTimestep ts(0.01);
    ts.advance(0.005);
    ts.setStepSeconds(0.1);
    CHECK(ts.alpha() == doctest::Approx(0.5));
    CHECK(ts.advance(0.05) == 1);
}

TEST_CASE("the same real time gives the same steps however it is sliced") {
    // The point of a fixed step: 60 frames of 1/60 s and 30 frames of 1/30 s
    // both simulate exactly one second.
    FixedTimestep fast(1.0 / 60.0);
    FixedTimestep slow(1.0 / 60.0);
    int fastSteps = 0;
    int slowSteps = 0;
    for (int i = 0; i < 60; ++i) {
        fastSteps += fast.advance(1.0 / 60.0);
    }
    for (int i = 0; i < 30; ++i) {
        slowSteps += slow.advance(1.0 / 30.0);
    }
    // Rounding can leave the very last step one frame early or late.
    CHECK(std::abs(fastSteps - slowSteps) <= 1);
    CHECK(fastSteps >= 59);
}

TEST_CASE("rounding error never loses a step") {
    // 0.1 and 0.01 are not exact in binary. Without a tolerance, time worth
    // exactly N steps sometimes produced N - 1 (this test caught that bug).
    for (const double step : {0.1, 0.01, 1.0 / 60.0, 1.0 / 30.0}) {
        FixedTimestep ts(step, /*maxStepsPerFrame=*/100, /*maxFrameSeconds=*/10.0);
        CHECK(ts.advance(step * 5.0) == 5);
        int total = 0;
        for (int i = 0; i < 1000; ++i) {
            total += ts.advance(step);
        }
        CHECK(total == 1000);
    }
}

TEST_CASE("the clock only moves forward") {
    Clock clock;
    const double a = clock.elapsedSeconds();
    const double b = clock.elapsedSeconds();
    CHECK(b >= a);
    CHECK(clock.tick() >= 0.0);
}

} // TEST_SUITE
