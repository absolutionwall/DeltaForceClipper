#include "event_coalescer.hpp"
#include "clip_window.hpp"

#include <cassert>

int main()
{
    using deltaforceclipper::promotesEvent;

    assert(promotesEvent("double_kill", "triple_kill"));
    assert(promotesEvent("double_kill", "multikill"));
    assert(promotesEvent("multikill", "quadra_kill"));
    assert(promotesEvent("quadra_kill", "penta_kill"));
    assert(!promotesEvent("penta_kill", "quadra_kill"));
    assert(!promotesEvent("triple_kill", "double_kill"));
    assert(!promotesEvent("multikill", "kill"));
    assert(!promotesEvent("double_kill", "double_kill"));

    using deltaforceclipper::requestedClipWindowMs;
    assert(requestedClipWindowMs(1000, 3000, 1000, 2000) == 5000);
    assert(requestedClipWindowMs(1000, 1000, 1000, 2000) == 3000);
    assert(requestedClipWindowMs(3000, 1000, 1000, 2000) == -1);
    assert(requestedClipWindowMs(1000, 3000, -1, 2000) == -1);
}
