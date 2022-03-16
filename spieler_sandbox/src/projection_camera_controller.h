#pragma once

#include <DirectXMath.h>

#include <spieler/window/event.h>
#include <spieler/window/window_event.h>
#include <spieler/window/mouse_event.h>

namespace Sandbox
{

    struct ProjectionCamera
    {
        // View props
        DirectX::XMVECTOR   EyePosition{ DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f) };
        DirectX::XMVECTOR   Front{ DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f) };
        DirectX::XMVECTOR   UpDirection{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f) };
        DirectX::XMMATRIX   View{ DirectX::XMMatrixIdentity() };

        // Projection props
        float               Fov{ DirectX::XM_PIDIV2 };
        float               AspectRatio{ 0.0f };
        float               NearPlane{ 0.1f };
        float               FarPlane{ 1000.0f };
        DirectX::XMMATRIX   Projection{ DirectX::XMMatrixIdentity() };
    };

    class ProjectionCameraController
    {
    public:
        ProjectionCameraController(float fov, float aspectRatio);

    public:
        const ProjectionCamera& GetCamera() const { return m_Camera; }

    public:
        void OnEvent(Spieler::Event& event);
        void OnUpdate(float dt);
        void OnImGuiRender(float dt);

    private:
        bool OnWindowResized(Spieler::WindowResizedEvent& event);
        bool OnMouseButtonPressed(Spieler::MouseButtonPressedEvent& event);
        bool OnMouseMoved(Spieler::MouseMovedEvent& event);
        bool OnMouseScrolled(Spieler::MouseScrolledEvent& event);

        void UpdateView();
        void UpdateProjection();

    private:
        ProjectionCamera    m_Camera;

        float               m_Pitch{ 0.0f };        // X axis
        float               m_Yaw{ -90.0f };        // Y axis
        float               m_Roll{ 0.0f };         // Z axis

        float               m_CameraSpeed{ 100.0f };
        float               m_MouseSensitivity{ 0.2f };

        DirectX::XMFLOAT2   m_LastMousePosition{ 0.0f, 0.0f };
    };

} // namespace Sandbox