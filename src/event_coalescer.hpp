#pragma once

#include <string_view>

namespace deltaforceclipper {

constexpr int eventPriority(std::string_view event)
{
    if (event == "penta_kill") return 6;
    if (event == "quadra_kill") return 5;
    if (event == "multikill") return 4;
    if (event == "triple_kill") return 3;
    if (event == "double_kill") return 2;
    if (event == "kill") return 1;
    return 0;
}

constexpr bool promotesEvent(std::string_view current, std::string_view candidate)
{
    return eventPriority(candidate) > eventPriority(current);
}

}
