#pragma once

#include "spieler/core/layer.hpp"

namespace spieler
{

    class Camera;

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
        void OnImGuiRender(float dt) override;

    public:
        void SetCamera(Camera* camera);

    private:
        bool m_IsEventsAreBlocked{ true };
        bool m_IsDemoWindowEnabled{ false };
        bool m_IsBottomPanelEnabled{ true };

        Camera* m_Camera{ nullptr };
    };

} // namespace spieler
