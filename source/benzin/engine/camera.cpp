#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/camera.hpp"

#include "benzin/utility/time_utils.hpp"
#include "benzin/tools/render_viewport_tool.hpp"
#include "benzin/system/input.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/core/engine_math.hpp"

namespace benzin
{

    // Projection

    DirectX::BoundingFrustum Projection::GetTransformedBoundingFrustum(const DirectX::XMMATRIX& transform) const
    {
        DirectX::BoundingFrustum transformedBoundingFrustum;
        m_BoundingFrustum.Transform(transformedBoundingFrustum, transform);

        return transformedBoundingFrustum;
    }

    DirectX::XMFLOAT4 Projection::GetPackedFrustumPlaneSlopes() const
    {
        // Can be used to reconstruct ViewPosition from ViewDepth and Uv coordinates
        // Usage:
        //   viewPos.xy = (packedSlopes.zw * uv + packedSlopes.xy) * viewDepth
        //   viewPos.z = viewDepth

        DirectX::XMFLOAT4 packedSlopes;
        packedSlopes.x = -m_BoundingFrustum.LeftSlope;
        packedSlopes.y = -m_BoundingFrustum.TopSlope;
        packedSlopes.z = m_BoundingFrustum.LeftSlope - m_BoundingFrustum.RightSlope;
        packedSlopes.w = m_BoundingFrustum.TopSlope - m_BoundingFrustum.BottomSlope;

        return packedSlopes;
    }

    void Projection::UpdateViewToClipMatrix()
    {
        m_ViewToClipMatrix = CreateViewToClipMatrix();
        m_InvViewToClipMatrix = DirectX::XMMatrixInverse(nullptr, m_ViewToClipMatrix);

        DirectX::BoundingFrustum::CreateFromMatrix(m_BoundingFrustum, m_ViewToClipMatrix);
    };

    // PerspectiveProjection

    PerspectiveProjection::PerspectiveProjection(float verticalFovInRadians, float aspectRatio, float nearPlane, float farPlane)
    {
        SetLens(verticalFovInRadians, aspectRatio, nearPlane, farPlane);
    }
    
    void PerspectiveProjection::SetVerticalFov(float verticalFovInRadians)
    {
        m_VerticalFovInRadians = verticalFovInRadians;

        UpdateViewToClipMatrix();
    }

    void PerspectiveProjection::SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;

        UpdateViewToClipMatrix();
    }

    void PerspectiveProjection::SetLens(float verticalFovInRadians, float aspectRatio, float nearPlane, float farPlane)
    {
        m_VerticalFovInRadians = verticalFovInRadians;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;

        UpdateViewToClipMatrix();
    }

    DirectX::XMMATRIX PerspectiveProjection::CreateViewToClipMatrix() const
    {
        return DirectX::XMMatrixPerspectiveFovLH(m_VerticalFovInRadians, m_AspectRatio, m_NearPlane, m_FarPlane);
    }

    // OrthographicProjection

    void OrthographicProjection::SetViewRect(const ViewRect& viewRect)
    {
        m_ViewRect = viewRect;

        UpdateViewToClipMatrix();
    };

    DirectX::XMMATRIX OrthographicProjection::CreateViewToClipMatrix() const
    {
        return DirectX::XMMatrixOrthographicOffCenterLH(
            m_ViewRect.LeftPlane,
            m_ViewRect.RightPlane,
            m_ViewRect.BottomPlane,
            m_ViewRect.TopPlane,
            m_ViewRect.NearPlane,
            m_ViewRect.FarPlane
        );
    }

    // Camera

    Camera::Camera(Projection& projection)
        : m_Projection{ projection }
    {
        UpdateRightDirection();
        UpdateWorldToViewMatrix();
    }

    void Camera::SetPosition(const DirectX::XMVECTOR& position)
    {
        m_Position = position;

        UpdateWorldToViewMatrix();
    }

    void Camera::SetFrontDirection(const DirectX::XMVECTOR& frontDirection)
    {
        m_FrontDirection = frontDirection;
        DirectX::XMVector3Normalize(m_FrontDirection);

        UpdateRightDirection();
        UpdateWorldToViewMatrix();
    }

    void Camera::SetUpDirection(const DirectX::XMVECTOR& upDirection)
    {
        m_UpDirection = upDirection;
        DirectX::XMVector3Normalize(m_UpDirection);

        UpdateRightDirection();
        UpdateWorldToViewMatrix();
    }

    const DirectX::XMMATRIX& Camera::GetViewToClipMatrix() const
    {
        return m_Projection.GetViewToClipMatrix();
    }

    const DirectX::XMMATRIX& Camera::GetInvViewToClipMatrix() const
    {
        return m_Projection.GetInvViewToClipMatrix();
    }

    DirectX::XMMATRIX Camera::GetWorldToClipMatrix() const
    {
        return m_WorldToViewMatrix * GetViewToClipMatrix();
    }

    DirectX::XMMATRIX Camera::GetInvWorldToClipMatrix() const
    {
        return DirectX::XMMatrixInverse(nullptr, GetWorldToClipMatrix());
    }

    DirectX::XMMATRIX Camera::GetInvDirectionalWorldToClipMatrix() const
    {
        DirectX::XMMATRIX directionWorldToViewMatrix = m_WorldToViewMatrix;
        directionWorldToViewMatrix.r[3] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Removes translation

        const DirectX::XMMATRIX directionalWorldToClipMatrix = directionWorldToViewMatrix * GetViewToClipMatrix();
        return DirectX::XMMatrixInverse(nullptr, directionalWorldToClipMatrix);
    }

    void Camera::UpdateRightDirection()
    {
        m_RightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_FrontDirection, m_UpDirection));
    }

    void Camera::UpdateWorldToViewMatrix()
    {
        m_WorldToViewMatrix = DirectX::XMMatrixLookToLH(m_Position, m_FrontDirection, m_UpDirection);
        m_WorldToViewMatrixForNormals = GetMatrixForNormals(m_WorldToViewMatrix);
        m_InvWorldToViewMatrix = DirectX::XMMatrixInverse(nullptr, m_WorldToViewMatrix);
    }

    // CameraController

    FlyCameraController::FlyCameraController(Camera& camera)
        : m_Camera{ camera }
    {
        m_Camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Pitch, m_Yaw));
    }

    void FlyCameraController::SetCameraPitchYaw(float pitch, float yaw)
    {
        m_Pitch = DirectX::XMConvertToRadians(pitch);
        m_Yaw = DirectX::XMConvertToRadians(yaw);

        m_Camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Pitch, m_Yaw));
    }

    void FlyCameraController::OnUpdate(std::chrono::microseconds dt)
    {
        UpdatePitchAndYawIfNeeded();

        float translationSpeedFactor = 1.0f;
        if (Input::IsKeyPressed(KeyCode::Shift))
        {
            translationSpeedFactor = 2.0f;
        }
        else if (Input::IsKeyPressed(KeyCode::Control))
        {
            translationSpeedFactor = 0.3f;
        }


        const float delta = m_CameraTranslationSpeed * translationSpeedFactor * ToFloatMs(dt);
        const auto& position = m_Camera.GetPosition();

        DirectX::XMVECTOR updatedPosition = DirectX::XMVectorZero();

        // Front / Back
        {
            const auto& frontDirection = m_Camera.GetFrontDirection();

            if (Input::IsKeyPressed(KeyCode::W))
            {
                updatedPosition = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(frontDirection, delta));
            }
            else if (Input::IsKeyPressed(KeyCode::S))
            {
                updatedPosition = DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(frontDirection, delta));
            }
        }

        // Left / Right
        {
            const auto& rightDirection = m_Camera.GetRightDirection();

            if (Input::IsKeyPressed(KeyCode::A))
            {
                updatedPosition = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(rightDirection, delta));
            }
            else if (Input::IsKeyPressed(KeyCode::D))
            {
                updatedPosition = DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(rightDirection, delta));
            }
        }

        // Up / Down
        {
            const auto& upDirection = m_Camera.GetUpDirection();

            if (Input::IsKeyPressed(KeyCode::Space))
            {
                updatedPosition = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(upDirection, delta));
            }
            else if (Input::IsKeyPressed(KeyCode::C))
            {
                updatedPosition = DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(upDirection, delta));
            }
        }

        if (!DirectX::XMVector4Equal(updatedPosition, DirectX::XMVectorZero()))
        {
            m_Camera.SetPosition(updatedPosition);
        }
    }

    bool FlyCameraController::OnRenderViewportResized(uint32_t width, uint32_t height)
    {
        if (auto* perspectiveProjection = GetPerspectiveProjection())
        {
            const float aspectRatio = (float)width / height;
            perspectiveProjection->SetAspectRatio(aspectRatio);
        }

        return false;
    }

    bool FlyCameraController::OnMouseMoved(const MouseMovedEvent& event)
    {
        if (Input::IsMouseButtonPressed(MouseButton::Right))
        {
            const float deltaX = event.GetX<float>() - m_LastMousePosition.x;
            const float deltaY = event.GetY<float>() - m_LastMousePosition.y;

            m_Pitch += m_MouseSensitivity * deltaY;
            m_Yaw += m_MouseSensitivity * deltaX;

            // Clamp the up down view
            m_Pitch = std::clamp(m_Pitch, -DirectX::XM_PIDIV2 + 0.01f, DirectX::XM_PIDIV2 - 0.01f);

            // 360 rotation
            if (m_Yaw > DirectX::XM_PI)
            {
                m_Yaw = -DirectX::XM_PI;
            }

            if (m_Yaw < -DirectX::XM_PI)
            {
                m_Yaw = DirectX::XM_PI;
            }

            m_Camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Pitch, m_Yaw));
        }

        m_LastMousePosition.x = event.GetX<float>();
        m_LastMousePosition.y = event.GetY<float>();

        return true;
    }

    bool FlyCameraController::OnMouseScrolled(const MouseScrolledEvent& event)
    {
        static const float minVerticalFovInRadians = DirectX::XMConvertToRadians(45.0f);
        static const float maxVerticalFovInRadians = DirectX::XMConvertToRadians(120.0f);

        if (auto* perspectiveProjection = GetPerspectiveProjection())
        {
            const float verticalFov = perspectiveProjection->GetVerticalFovInRadians() - m_MouseWheelSensitivity * (float)event.GetOffsetX();
            perspectiveProjection->SetVerticalFov(std::clamp(verticalFov, minVerticalFovInRadians, maxVerticalFovInRadians));
        }

        return true;
    }

    PerspectiveProjection* FlyCameraController::GetPerspectiveProjection()
    {
        const auto* perspectiveProjection = dynamic_cast<const PerspectiveProjection*>(&m_Camera.GetProjection());

        if (!perspectiveProjection)
        {
            BenzinWarning("Projection isn't Perspective! FlyCameraController supports only PerspectiveProjection");
            return nullptr;
        }

        return const_cast<PerspectiveProjection*>(perspectiveProjection);
    }

    void FlyCameraController::UpdatePitchAndYawIfNeeded()
    {
        static constexpr DirectX::XMVECTOR eplison3{ 0.0001f, 0.0001f, 0.0001f };

        const auto& frontDirection = m_Camera.GetFrontDirection();

        if (!DirectX::XMVector3NearEqual(frontDirection, GetDirectionFromPitchYaw(m_Pitch, m_Yaw), eplison3))
        {
            const auto [pitch, yaw] = GetPitchYawFromDirection(frontDirection);

            m_Pitch = pitch;
            m_Yaw = yaw;
        }
    }

} // namespace benzin
