#pragma once

namespace benzin
{

    std::chrono::microseconds SecToUs(float sec);
    std::chrono::microseconds MsToUs(float ms);
    std::chrono::milliseconds SecToMs(float sec);

    std::chrono::milliseconds ToMs(std::chrono::microseconds us);
    std::chrono::microseconds ToUs(std::chrono::nanoseconds ns);

    float ToFloatMs(std::chrono::microseconds us);
    float ToFloatSec(std::chrono::microseconds us);

} // namespace benzin
