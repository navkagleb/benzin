#pragma once

namespace benzin
{

    std::chrono::microseconds SecToUS(float sec);
    std::chrono::microseconds MSToUS(float ms);
    std::chrono::milliseconds SecToMS(float sec);

    std::chrono::milliseconds ToMS(std::chrono::microseconds us);
    std::chrono::microseconds ToUS(std::chrono::nanoseconds ns);

    float ToFloatMS(std::chrono::microseconds us);
    float ToFloatSec(std::chrono::microseconds us);

} // namespace benzin
