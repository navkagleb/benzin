#pragma once

namespace benzin
{

    class PerspectiveProjection
    {
    public:
        friend class FlyCameraTool;

        PerspectiveProjection();

        const auto& GetViewToClipMatrix() const { return m_ViewToClipMatrix; }
        const auto& GetClipToViewMatrix() const { return m_ClipToViewMatrix; }
        const auto& GetViewFrustum() const { return m_ViewFrustum; }

        auto GetVerticalFovInRadians() const { return m_VerticalFovInRadians; }
        auto GetAspectRatio() const { return m_AspectRatio; }
        auto GetNearPlane() const { return m_NearPlane; }

        DirectX::XMFLOAT2 GetUvToViewScale() const;
        DirectX::XMFLOAT2 GetUvToViewBias() const;

        float GetPixelToWorldScale(uint32_t height) const;

        void SetLens(float verticalFov, float aspectRatio, float nearPlane);

    private:
        void UpdateViewToClipMatrix();

        DirectX::XMMATRIX m_ViewToClipMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ClipToViewMatrix = DirectX::XMMatrixIdentity();
        DirectX::BoundingFrustum m_ViewFrustum;

        float m_VerticalFovInRadians = DirectX::XMConvertToRadians(90.0f);
        float m_AspectRatio = 16.0f / 9.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 1000.0f;
    };

    class Camera
    {
    public:
        friend class FlyCameraTool;

        Camera();

        const auto& GetPosition() const { return m_Position; }
        const auto& GetFrontDirection() const { return m_FrontDirection; }
        const auto& GetUpDirection() const { return m_UpDirection; }
        const auto& GetRightDirection() const { return m_RightDirection; }

        const auto& GetWorldToViewMatrix() const { return m_WorldToViewMatrix; }
        const auto& GetViewToWorldMatrix() const { return m_ViewToWorldMatrix; }

        const auto& GetWorldFrustum() const { return m_WorldFrustum; }

        const auto& GetViewToClipMatrix() const { return m_Projection.GetViewToClipMatrix(); }
        const auto& GetClipToViewMatrix() const { return m_Projection.GetClipToViewMatrix(); }
        const auto& GetViewFrustum() const { return m_Projection.GetViewFrustum(); }

        auto& GetProjection(this auto&& self) { return self.m_Projection; }

        DirectX::XMMATRIX GetWorldToClipMatrix() const { return m_WorldToViewMatrix * GetViewToClipMatrix(); }
        DirectX::XMMATRIX GetClipToWorldMatrix() const { return DirectX::XMMatrixInverse(nullptr, GetWorldToClipMatrix()); }

        DirectX::XMMATRIX GetClipToWorldNoTranslation() const;

        void SetPosition(const DirectX::XMVECTOR& position);
        void SetFrontDirection(const DirectX::XMVECTOR& frontDirection);
        void SetUpDirection(const DirectX::XMVECTOR& upDirection);

    private:
        void UpdateRightDirection();
        void UpdateWorldToViewMatrix();

        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_FrontDirection{ 0.0f, 0.0f, -1.0f, 1.0f };
        DirectX::XMVECTOR m_UpDirection{ 0.0f, 1.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_RightDirection{ 0.0f, 0.0f, 0.0f, 1.0f };

        DirectX::XMMATRIX m_WorldToViewMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ViewToWorldMatrix = DirectX::XMMatrixIdentity();

        DirectX::BoundingFrustum m_WorldFrustum;

        PerspectiveProjection m_Projection;
    };

    class FlyCameraController
    {
    public:
        friend class FlyCameraTool;

        void SetCamera(Camera& camera);

        void MoveCamera(std::chrono::microseconds dt);
        void RotateCamera(DirectX::XMINT2 mousePosition, DirectX::XMINT2 prevMousePosition);
        void IncrementFov(float direction);

        bool OnRenderViewportResized(uint32_t width, uint32_t height);

    private:
        void UpdatePitchAndYawIfNeeded();

        Camera* m_Camera = nullptr;

        float m_CameraTranslationSpeed = 0.002f;
        float m_MouseSensitivity = 0.003f;
        float m_MouseWheelSensitivity = 0.04f;

        // For camera front direction
        float m_Pitch = 0.0f; // X axis (top-down)
        float m_Yaw = 0.0f; // Y axis (left-right)
    };

}
