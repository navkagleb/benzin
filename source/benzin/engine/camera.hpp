#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/window_event.hpp"
#include "benzin/system/mouse_event.hpp"

namespace benzin
{

    class RenderViewportTool;

    class Projection
    {
    public:
        const auto& GetViewToClipMatrix() const { return m_ViewToClipMatrix; }
        const auto& GetClipToViewMatrix() const { return m_ClipToViewMatrix; }

        const auto& GetViewFrustum() const { return m_ViewFrustum; }

        void UpdateViewToClipMatrix();

    protected:
        virtual DirectX::XMMATRIX CreateViewToClipMatrix() const = 0;

    private:
        DirectX::XMMATRIX m_ViewToClipMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ClipToViewMatrix = DirectX::XMMatrixIdentity();

        DirectX::BoundingFrustum m_ViewFrustum;
    };

    class PerspectiveProjection : public Projection
    {
    public:
        friend class FlyCameraTool;

        PerspectiveProjection() = default;
        PerspectiveProjection(float verticalFovInRadians, float aspectRatio, float nearPlane, float farPlane);

    public:
        float GetVerticalFovInRadians() const { return m_VerticalFovInRadians; }
        void SetVerticalFov(float verticalFovInRadians);

        float GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(float aspectRatio);

        DirectX::XMFLOAT2 GetUvToViewScale() const;
        DirectX::XMFLOAT2 GetUvToViewBias() const;

        float GetPixelToWorldScale(uint32_t height) const;

        void SetLens(float verticalFov, float aspectRatio, float nearPlane, float farPlane);

    private:
        DirectX::XMMATRIX CreateViewToClipMatrix() const override;

    private:
        float m_VerticalFovInRadians = DirectX::XMConvertToRadians(60.0f);
        float m_AspectRatio = 0.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 1000.0f;
    };

    class OrthographicProjection : public Projection
    {
    public:
        struct ViewRect
        {
            float LeftPlane = -1.0f;
            float RightPlane = 1.0f;
            float BottomPlane = -1.0f;
            float TopPlane = 1.0f;
            float NearPlane = -1.0f;
            float FarPlane = 1.0f;
        };

        void SetViewRect(const ViewRect& viewRect);

    private:
        DirectX::XMMATRIX CreateViewToClipMatrix() const override;

    private:
        ViewRect m_ViewRect;
    };

    class Camera
    {
    public:
        friend class FlyCameraTool;
        friend class FlyCameraController;

        explicit Camera(Projection& projection);

    public:
        const auto& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const auto& GetFrontDirection() const { return m_FrontDirection; }
        void SetFrontDirection(const DirectX::XMVECTOR& frontDirection);

        const auto& GetUpDirection() const { return m_UpDirection; }
        void SetUpDirection(const DirectX::XMVECTOR& upDirection);

        const auto& GetRightDirection() const { return m_RightDirection; }

        const auto& GetWorldToViewMatrix() const { return m_WorldToViewMatrix; }
        const auto& GetViewToWorldMatrix() const { return m_ViewToWorldMatrix; }

        const auto& GetWorldFrustum() const { return m_WorldFrustum; }

        const auto& GetViewToClipMatrix() const { return m_Projection.GetViewToClipMatrix(); }
        const auto& GetClipToViewMatrix() const { return m_Projection.GetClipToViewMatrix(); }
        const auto& GetViewFrustum() const { return m_Projection.GetViewFrustum(); }

        DirectX::XMMATRIX GetWorldToClipMatrix() const;
        DirectX::XMMATRIX GetClipToWorldMatrix() const;

        DirectX::XMMATRIX GetClipToWorldNoTranslation() const;

    private:
        void UpdateRightDirection();
        void UpdateWorldToViewMatrix();

    private:
        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_FrontDirection{ 0.0f, 0.0f, -1.0f, 1.0f };
        DirectX::XMVECTOR m_UpDirection{ 0.0f, 1.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_RightDirection{ 0.0f, 0.0f, 0.0f, 1.0f };

        DirectX::XMMATRIX m_WorldToViewMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ViewToWorldMatrix = DirectX::XMMatrixIdentity();

        DirectX::BoundingFrustum m_WorldFrustum;

        Projection& m_Projection;
    };

    class FlyCameraController
    {
    public:
        friend class FlyCameraTool;
        friend class RenderViewportTool;

        explicit FlyCameraController(Camera& camera);

    private:
        void SetCameraTranslationSpeed(float speed) { m_CameraTranslationSpeed = speed; }
        void SetCameraPitchYaw(float pitch, float yaw);

        bool OnRenderViewportResized(uint32_t width, uint32_t height);

        void MoveCamera(std::chrono::microseconds dt);
        void RotateCamera(DirectX::XMINT2 mousePosition, DirectX::XMINT2 prevMousePosition);
        void IncrementFov(float direction);

        PerspectiveProjection* GetPerspectiveProjection();

        void UpdatePitchAndYawIfNeeded();

    private:
        Camera& m_Camera;

        float m_CameraTranslationSpeed = 0.002f;
        float m_MouseSensitivity = 0.003f;
        float m_MouseWheelSensitivity = 0.04f;

        // For camera front direction
        float m_Pitch = 0.0f; // X axis (top-down)
        float m_Yaw = -DirectX::XM_PIDIV2; // Y axis (left-right)
        DirectX::XMFLOAT2 m_LastMousePosition{ 0.0f, 0.0f };
    };

}
