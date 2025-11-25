#pragma once

namespace benzin
{

    class PerspectiveCamera
    {
    public:
        friend class FlyCameraTool;

        PerspectiveCamera();

        const auto& GetWorldToView() const { return m_WorldToView; }
        const auto& GetViewToWorld() const { return m_ViewToWorld; }
        const auto& GetViewToClip() const { return m_ViewToClip; }
        const auto& GetClipToView() const { return m_ClipToView; }

        const auto& GetViewFrustumLeft() const { return m_ViewFrustumLeft; }
        const auto& GetViewFrustumRight() const { return m_ViewFrustumRight; }
        const auto& GetViewFrustumBottom() const { return m_ViewFrustumBottom; }
        const auto& GetViewFrustumTop() const { return m_ViewFrustumTop; }
        const auto& GetViewFrustumNear() const { return m_ViewFrustumNear; }
        const auto& GetViewFrustumFar() const { return m_ViewFrustumFar; }

        auto GetUvToViewScale() const { return m_UvToViewScale; }
        auto GetUvToViewBias() const { return m_UvToViewBias; }

        const auto& GetPosition() const { return m_Position; }
        const auto& GetFrontDirection() const { return m_FrontDirection; }
        const auto& GetUpDirection() const { return m_UpDirection; }
        const auto& GetRightDirection() const { return m_RightDirection; }

        auto GetVerticalFovInRadians() const { return m_VerticalFovInRadians; }
        auto GetAspectRatio() const { return m_AspectRatio; }
        auto GetNearPlane() const { return m_NearPlane; }
        auto GetFarPlane() const { return m_FarPlane; }

        DirectX::XMMATRIX GetWorldToClip() const { return m_WorldToView * m_ViewToClip; }
        DirectX::XMMATRIX GetClipToWorld() const { return DirectX::XMMatrixInverse(nullptr, GetWorldToClip()); }

        DirectX::XMMATRIX GetClipToWorldNoTranslation() const;

        float GetPixelToWorldScale(uint32_t height) const;

        void SetPosition(const DirectX::XMVECTOR& position);
        void SetFrontDirection(const DirectX::XMVECTOR& frontDirection);
        void SetUpDirection(const DirectX::XMVECTOR& upDirection);
        void SetLens(float verticalFov, float aspectRatio, float nearPlane);

    private:
        void UpdateRightDirection();
        void UpdateWorldToViewMatrix();
        void UpdateViewToClipMatrix();

        DirectX::XMMATRIX m_WorldToView = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ViewToWorld = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ViewToClip = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_ClipToView = DirectX::XMMatrixIdentity();

        DirectX::XMFLOAT4 m_ViewFrustumLeft = {};
        DirectX::XMFLOAT4 m_ViewFrustumRight = {};
        DirectX::XMFLOAT4 m_ViewFrustumBottom = {};
        DirectX::XMFLOAT4 m_ViewFrustumTop = {};
        DirectX::XMFLOAT4 m_ViewFrustumNear = {};
        DirectX::XMFLOAT4 m_ViewFrustumFar = {};

        DirectX::XMFLOAT2 m_UvToViewScale = {};
        DirectX::XMFLOAT2 m_UvToViewBias = {};

        DirectX::XMVECTOR m_Position = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_FrontDirection = { 0.0f, 0.0f, -1.0f, 1.0f };
        DirectX::XMVECTOR m_UpDirection = { 0.0f, 1.0f, 0.0f, 1.0f };
        DirectX::XMVECTOR m_RightDirection = { 0.0f, 0.0f, 0.0f, 1.0f };

        float m_VerticalFovInRadians = DirectX::XMConvertToRadians(90.0f);
        float m_AspectRatio = 16.0f / 9.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 1000.0f;
    };

    class FlyCameraController
    {
    public:
        friend class FlyCameraTool;

        void SetCamera(PerspectiveCamera& camera);

        void MoveCamera(float dtInMs);
        void RotateCamera(DirectX::XMINT2 mousePosition, DirectX::XMINT2 prevMousePosition);
        void IncrementFov(float direction);

        bool OnRenderViewportResized(uint32_t width, uint32_t height);

    private:
        void UpdatePitchAndYawIfNeeded();

        PerspectiveCamera* m_Camera = nullptr;

        float m_CameraTranslationSpeed = 0.2f;
        float m_MouseSensitivity = 0.003f;
        float m_MouseWheelSensitivity = 0.04f;

        // For camera front direction
        float m_Pitch = 0.0f; // X axis (top-down)
        float m_Yaw = 0.0f; // Y axis (left-right)
    };

}
