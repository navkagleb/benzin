#pragma once

#include <spieler/math/math.hpp>
#include <spieler/system/event.hpp>
#include <spieler/system/window_event.hpp>
#include <spieler/system/mouse_event.hpp>

namespace sandbox
{

    struct Camera
    {
        DirectX::XMVECTOR EyePosition{ 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR Front{ 0.0f, 0.0f, 1.0f, 1.0f };
        DirectX::XMVECTOR Up{ 0.0f, 1.0f, 0.0f, 1.0f };
        DirectX::XMMATRIX View{ DirectX::XMMatrixIdentity() };

        float FOV{ spieler::math::ToRadians(60.0f) };
        float AspectRatio{ 0.0f };
        float NearPlane{ 0.1f };
        float FarPlane{ 1000.0f };
        DirectX::XMMATRIX Projection{ DirectX::XMMatrixIdentity() };

        DirectX::BoundingFrustum BoundingFrustum;
    };

    class CameraController
    {
    public:
        CameraController() = default;
        CameraController(Camera& camera);

    public:
        void OnEvent(spieler::Event& event);
        void OnUpdate(float dt);
        void OnImGuiRender(float dt);

    private:
        bool OnWindowResized(spieler::WindowResizedEvent& event);
        bool OnMouseButtonPressed(spieler::MouseButtonPressedEvent& event);
        bool OnMouseMoved(spieler::MouseMovedEvent& event);
        bool OnMouseScrolled(spieler::MouseScrolledEvent& event);

        void UpdateView();
        void UpdateProjection();

    private:
        Camera* m_Camera{ nullptr };

        float m_Theta{ 0.0f };
        float m_Phi{ DirectX::XM_PIDIV2 };
        float m_CameraSpeed{ 20.0f };
        float m_MouseSensitivity{ 0.003f };
        float m_MouseWheelSensitivity{ 0.04f };
        DirectX::XMFLOAT2 m_LastMousePosition{ 0.0f, 0.0f };
    };

} // namespace sandbox
