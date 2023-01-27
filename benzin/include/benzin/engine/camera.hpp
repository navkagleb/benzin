#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/window_event.hpp"
#include "benzin/system/mouse_event.hpp"

namespace benzin
{

    class Projection
    {
    public:
        const DirectX::XMMATRIX& GetMatrix() const { return m_Projection; }
        const DirectX::XMMATRIX& GetInverseMatrix() const { return m_InverseProjection; }

        const DirectX::BoundingFrustum& GetBoundingFrustum() const { return m_BoundingFrustum; }

    public:
        DirectX::BoundingFrustum GetTransformedBoundingFrustum(const DirectX::XMMATRIX& transform) const;

        void UpdateMatrix();

    protected:
        virtual DirectX::XMMATRIX CreateMatrix() const = 0;

    private:
        DirectX::XMMATRIX m_Projection{ DirectX::XMMatrixIdentity() };
        DirectX::XMMATRIX m_InverseProjection{ DirectX::XMMatrixIdentity() };
        DirectX::BoundingFrustum m_BoundingFrustum;
    };

    class PerspectiveProjection : public Projection
    {
    public:
        friend class FlyCameraController;

    public:
        PerspectiveProjection() = default;
        PerspectiveProjection(float fov, float aspectRatio, float nearPlane, float farPlane);

    public:
        float GetFOV() const { return m_FOV; }
        void SetFOV(float fov);

        float GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(float aspectRatio);

    public:
        void SetLens(float fov, float aspectRatio, float nearPlane, float farPlane);

    private:
        DirectX::XMMATRIX CreateMatrix() const override;

    private:
        float m_FOV{ DirectX::XMConvertToRadians(60.0f) };
        float m_AspectRatio{ 0.0f };
        float m_NearPlane{ 0.1f };
        float m_FarPlane{ 1000.0f };
    };

    class OrthographicProjection : public Projection
    {
    public:
        struct ViewRect
        {
            float LeftPlane{ -1.0f };
            float RightPlane{ 1.0f };
            float BottomPlane{ -1.0f };
            float TopPlane{ 1.0f };
            float NearPlane{ -1.0f };
            float FarPlane{ 1.0f };
        };

    public:
        void SetViewRect(const ViewRect& viewRect);

    private:
        DirectX::XMMATRIX CreateMatrix() const override;

    private:
        ViewRect m_ViewRect;
    };

    class Camera
    {
    public:
        friend class FlyCameraController;

    public:
        Camera();
        Camera(const Projection* projection);

    public:
        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const DirectX::XMVECTOR& GetFrontDirection() const { return m_FrontDirection; }
        void SetFrontDirection(const DirectX::XMVECTOR& frontDirection);

        const DirectX::XMVECTOR& GetUpDirection() const { return m_UpDirection; }
        void SetUpDirection(const DirectX::XMVECTOR& upDirection);

        const DirectX::XMVECTOR& GetRightDirection() const { return m_RightDirection; }

        const Projection* GetProjection() const { return m_Projection; }
        void SetProjection(const Projection* projection);

        const DirectX::XMMATRIX& GetViewMatrix() const { return m_ViewMatrix; }
        const DirectX::XMMATRIX& GetInverseViewMatrix() const { return m_InverseViewMatrix; }

        DirectX::XMMATRIX GetViewProjectionMatrix() const;

    private:
        void UpdateRightDirection();
        void UpdateViewMatrix();

    private:
        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, -4.0f, 1.0f };
        DirectX::XMVECTOR m_FrontDirection{ 0.0f, 0.0f, 1.0f, 1.0f };
        DirectX::XMVECTOR m_UpDirection{ 0.0f, 1.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_RightDirection{ -1.0f, 0.0f, 0.0f, 1.0f };

        DirectX::XMMATRIX m_ViewMatrix{ DirectX::XMMatrixIdentity() };
        DirectX::XMMATRIX m_InverseViewMatrix{ DirectX::XMMatrixIdentity() };

        const Projection* m_Projection{ nullptr };
    };

    class FlyCameraController
    {
    public:
        FlyCameraController(Camera* camera);

    public:
        void SetCamera(Camera* camera);

    public:
        void OnEvent(Event& event);
        void OnUpdate(float dt);
        void OnImGuiRender(float dt);

    private:
        bool OnWindowResized(WindowResizedEvent& event);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
        bool OnMouseMoved(MouseMovedEvent& event);
        bool OnMouseScrolled(MouseScrolledEvent& event);

        void RenderImGuiControllerProperties();
        void RenderImGuiViewProperties();
        void RenderImGuiProjectionProperties();

        DirectX::XMVECTOR GetCameraFrontDirection() const;

        PerspectiveProjection* GetPerspectiveProjection();

    private:
        Camera* m_Camera{ nullptr };

        float m_CameraTranslationSpeed{ 5.0f };
        float m_MouseSensitivity{ 0.003f };
        float m_MouseWheelSensitivity{ 0.04f };

        // For camera front direction
        float m_Theta{ 0.0f };
        float m_Phi{ DirectX::XM_PIDIV2 };
        DirectX::XMFLOAT2 m_LastMousePosition{ 0.0f, 0.0f };
    };

} // namespace benzin
