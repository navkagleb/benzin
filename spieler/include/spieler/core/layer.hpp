#pragma once

namespace spieler
{

    class Event;

    class Layer
    {
    public:
        virtual ~Layer() = default;

    public:
        virtual bool OnAttach() { return true; }
        virtual bool OnDetach() { return true; }

        virtual void OnEvent(Event& event) {}
        virtual void OnUpdate(float dt) {}
        virtual bool OnRender(float dt) { return true; }
        virtual void OnImGuiRender(float dt) {}

    protected:
        std::string m_Name;
    };

} // namespace spieler