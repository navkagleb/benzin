#pragma once

#include "spieler/core/layer.hpp"

namespace spieler
{

    class ImGuiLayer final : public Layer
    {
    public:
        static void Begin();
        static void End();

    public:
        bool IsEventsAreBlocked() const { return m_IsEventsAreBlocked; }
        void SetIsEventsAreBlocked(bool isEventsAreBlocked) { m_IsEventsAreBlocked = isEventsAreBlocked; }

    public:
        bool OnAttach() override;
        bool OnDetach() override;

        void OnEvent(Event& event) override;

    private:
        bool m_IsEventsAreBlocked{ true };
    };

} // namespace spieler
