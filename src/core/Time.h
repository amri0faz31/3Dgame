// core/Time.h
// Responsibility: Frame-to-frame timing. tick() samples current timestamp,
// computes delta seconds since last frame, and stores for queries.
// Usage pattern: call Time::tick() once per loop before updates; retrieve delta via delta().

#pragma once
#include <cstdint>

class Time {
public:
    // tick(): sample clock and compute seconds elapsed since prior tick.
    static void tick();
    // delta(): last computed frame time in seconds.
    static float delta();
    // elapsed(): total seconds since first tick.
    static float elapsed();
private:
    static uint64_t s_last; // previous timestamp (microseconds).
    static float s_delta;   // cached delta seconds.
    static float s_elapsed; // total elapsed seconds.
};
