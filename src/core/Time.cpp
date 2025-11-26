// core/Time.cpp
// Implements timing using std::chrono high_resolution_clock.
// Converts microseconds to floating-point seconds for simulation usage.

#include "Time.h"
#include <chrono>

uint64_t Time::s_last = 0;
float Time::s_delta = 0.f;
float Time::s_elapsed = 0.f;

void Time::tick(){
    using namespace std::chrono;
    uint64_t now = duration_cast<microseconds>(high_resolution_clock::now().time_since_epoch()).count();
    if(s_last == 0){
        s_last = now;
        s_delta = 0.f; // first frame no elapsed time.
        return;
    }
    s_delta = (now - s_last) / 1'000'000.0f;
    s_elapsed += s_delta;
    s_last = now;
}

float Time::delta(){
    return s_delta;
}

float Time::elapsed(){
    return s_elapsed;
}
