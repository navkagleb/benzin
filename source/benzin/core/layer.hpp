#pragma once

namespace benzin
{

    class Event;

    class Layer
    {
    public:
        virtual ~Layer() = default;

    public:
        virtual void OnEvent(Event& event) {}
        virtual void OnUpdate(float dt) {}
        virtual void OnRender() {}
        virtual void OnImGuiRender() {}
    };

} // namespace benzin
