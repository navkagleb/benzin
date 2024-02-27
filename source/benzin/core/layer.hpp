#pragma once

#include "benzin/core/tick_timer.hpp"

namespace benzin
{

    class Event;

    class Layer
    {
    public:
        inline static TickTimer s_FrameTimer;

        inline static const std::chrono::microseconds s_UpdateStatsInterval = SecToUS(1.0f);
        inline static bool s_IsUpdateStatsIntervalPassed = false;

    public:
        virtual ~Layer() = default;

    public:
        virtual void OnEvent(Event& event) {}
        virtual void OnUpdate() {}
        virtual void OnRender() {}
        virtual void OnImGuiRender() {}

        virtual void OnResize(uint32_t width, uint32_t height) {};
    };

} // namespace benzin
