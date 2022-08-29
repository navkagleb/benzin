#pragma once

#include "spieler/system/event.hpp"
#include "spieler/system/window_event.hpp"
#include "spieler/system/mouse_event.hpp"

namespace spieler::renderer
{

    class Camera
    {
    public:
        friend class CameraController;

    public:
        Camera();

    public:
        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const DirectX::XMVECTOR& GetFrontDirection() const { return m_FrontDirection; }
        void SetFrontDirection(const DirectX::XMVECTOR& frontDirection);

        const DirectX::XMVECTOR& GetUpDirection() const { return m_UpDirection; }

        const DirectX::XMVECTOR& GetRightDirection() const { return m_RightDirection; }

        float GetFOV() const { return m_FOV; }
        void SetFOV(float fov);

        float GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(float aspectRatio);

        const DirectX::XMMATRIX& GetView() const { return m_View; }
        const DirectX::XMMATRIX& GetInverseView() const { return m_InverseView; }

        const DirectX::XMMATRIX& GetProjection() const { return m_Projection; }
        const DirectX::XMMATRIX& GetInverseProjection() const { return m_InverseProjection; }

        const DirectX::XMMATRIX& GetViewProjection() const { return m_ViewProjection; }

        const DirectX::BoundingFrustum& GetBoundingFrustum() const { return m_BoundingFrustum; }

    public:
        void UpdateView();
        void UpdateProjection();

    private:
        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, -4.0f, 1.0f };
        DirectX::XMVECTOR m_FrontDirection{ 0.0f, 0.0f, 1.0f, 1.0f };
        DirectX::XMVECTOR m_UpDirection{ 0.0f, 1.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_RightDirection{ -1.0f, 0.0f, 0.0f, 1.0f };

        float m_FOV{ DirectX::XMConvertToRadians(60.0f) };
        float m_AspectRatio{ 0.0f };
        float m_NearPlane{ 0.1f };
        float m_FarPlane{ 1000.0f };

        DirectX::XMMATRIX m_View{ DirectX::XMMatrixIdentity() };
        DirectX::XMMATRIX m_InverseView{ DirectX::XMMatrixIdentity() };

        DirectX::XMMATRIX m_Projection{ DirectX::XMMatrixIdentity() };
        DirectX::XMMATRIX m_InverseProjection{ DirectX::XMMatrixIdentity() };

        DirectX::XMMATRIX m_ViewProjection{ DirectX::XMMatrixIdentity() };

        DirectX::BoundingFrustum m_BoundingFrustum;
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

        DirectX::XMVECTOR GetFrontDirection() const;

    private:
        Camera* m_Camera{ nullptr };

        float m_Theta{ 0.0f };
        float m_Phi{ DirectX::XM_PIDIV2 };

        float m_CameraSpeed{ 20.0f };
        float m_MouseSensitivity{ 0.003f };
        float m_MouseWheelSensitivity{ 0.04f };
        DirectX::XMFLOAT2 m_LastMousePosition{ 0.0f, 0.0f };
    };

} // namespace spieler::renderer
