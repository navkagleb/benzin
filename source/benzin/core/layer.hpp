#pragma once

#include "benzin/core/tick_timer.hpp"

namespace benzin
{

    class Event;

    class Layer
    {
    public:
        inline static TickTimer s_FrameTimer;

        inline static std::chrono::milliseconds s_UpdateStatsInterval = SecToMS(1.0f);
        inline static bool s_IsUpdateStatsIntervalPassed = false;

    public:
        virtual ~Layer() = default;

    public:
        virtual void OnBeginFrame() {}
        virtual void OnEndFrame() {}

        virtual void OnEvent(Event& event) {}
        virtual void OnUpdate() {}
        virtual void OnRender() {}
        virtual void OnImGuiRender() {}
    };

} // namespace benzin
