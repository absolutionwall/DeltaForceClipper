#pragma once

#include <cstdint>

namespace deltaforceclipper {

constexpr std::int64_t requestedClipWindowMs(std::int64_t firstEventMs,
                                             std::int64_t lastEventMs,
                                             std::int64_t preEventMs,
                                             std::int64_t postEventMs)
{
    if (lastEventMs < firstEventMs || preEventMs < 0 || postEventMs < 0) {
        return -1;
    }
    return (lastEventMs - firstEventMs) + preEventMs + postEventMs;
}

}
