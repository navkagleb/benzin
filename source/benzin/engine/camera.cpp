#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/camera.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/system/input.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    // PerspectiveCamera

    PerspectiveCamera::PerspectiveCamera()
    {
        UpdateRightDirection();
        UpdateWorldToViewMatrix();
        UpdateViewToClipMatrix();
    }

    DirectX::XMMATRIX PerspectiveCamera::GetClipToWorldNoTranslation() const
    {
        DirectX::XMMATRIX worldToViewMatrix = m_WorldToViewMatrix;
        worldToViewMatrix.r[3] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Removes translation

        const DirectX::XMMATRIX worldToClipMatrix = worldToViewMatrix * GetViewToClipMatrix();
        return DirectX::XMMatrixInverse(nullptr, worldToClipMatrix);
    }

    DirectX::XMFLOAT2 PerspectiveCamera::GetUvToViewScale() const
    {
        // Ref: NRD Sample - https://github.com/NVIDIA-RTX/NRD-Sample

        DirectX::XMFLOAT2 scale = {};
        scale.x = m_ViewFrustum.RightSlope - m_ViewFrustum.LeftSlope;
        scale.y = m_ViewFrustum.BottomSlope - m_ViewFrustum.TopSlope;

        return scale;
    }

    DirectX::XMFLOAT2 PerspectiveCamera::GetUvToViewBias() const
    {
        // Ref: NRD Sample - https://github.com/NVIDIA-RTX/NRD-Sample

        DirectX::XMFLOAT2 bias = {};
        bias.x = m_ViewFrustum.LeftSlope;
        bias.y = m_ViewFrustum.TopSlope;

        return bias;
    }

    float PerspectiveCamera::GetPixelToWorldScale(uint32_t height) const
    {
        // viewToClip[1][1] = 1.0f / std::tan(0.5f * verticalFov)

        const float verticalProjectionScale = DirectX::XMVectorGetByIndex(GetViewToClipMatrix().r[1], 1);
        const float pixelToWorldScale = 1.0f / (0.5f * (float)height * verticalProjectionScale);

        return pixelToWorldScale;
    }

    void PerspectiveCamera::SetPosition(const DirectX::XMVECTOR& position)
    {
        m_Position = position;

        UpdateWorldToViewMatrix();
    }

    void PerspectiveCamera::SetFrontDirection(const DirectX::XMVECTOR& frontDirection)
    {
        m_FrontDirection = frontDirection;
        DirectX::XMVector3Normalize(m_FrontDirection);

        UpdateRightDirection();
        UpdateWorldToViewMatrix();
    }

    void PerspectiveCamera::SetUpDirection(const DirectX::XMVECTOR& upDirection)
    {
        m_UpDirection = upDirection;
        DirectX::XMVector3Normalize(m_UpDirection);

        UpdateRightDirection();
        UpdateWorldToViewMatrix();
    }

    void PerspectiveCamera::SetLens(float verticalFovInRadians, float aspectRatio, float nearPlane)
    {
        m_VerticalFovInRadians = verticalFovInRadians;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;

        UpdateViewToClipMatrix();
    }

    void PerspectiveCamera::UpdateRightDirection()
    {
        m_RightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_FrontDirection, m_UpDirection));
    }

    void PerspectiveCamera::UpdateWorldToViewMatrix()
    {
        m_WorldToViewMatrix = DirectX::XMMatrixLookToLH(m_Position, m_FrontDirection, m_UpDirection);
        m_ViewToWorldMatrix = DirectX::XMMatrixInverse(nullptr, m_WorldToViewMatrix);

        m_ViewFrustum.Transform(m_WorldFrustum, m_ViewToWorldMatrix);
    }

    void PerspectiveCamera::UpdateViewToClipMatrix()
    {
        const float yScale = 1.0f / std::tan(m_VerticalFovInRadians * 0.5f);
        const float xScale = yScale / m_AspectRatio;

        m_ViewToClipMatrix = DirectX::XMMATRIX
        {
            xScale, 0.0f,   0.0f,        0.0f,
            0.0f,   yScale, 0.0f,        0.0f,
            0.0f,   0.0f,   0.0f,        1.0f,
            0.0f,   0.0f,   m_NearPlane, 0.0f
        };

        m_ClipToViewMatrix = DirectX::XMMatrixInverse(nullptr, m_ViewToClipMatrix);

        // TODO: duplication
        const DirectX::XMMATRIX viewToClipMatrixForFrustum = DirectX::XMMatrixPerspectiveFovLH(
            m_VerticalFovInRadians,
            m_AspectRatio,
            m_NearPlane,
            m_FarPlane);

        DirectX::BoundingFrustum::CreateFromMatrix(m_ViewFrustum, viewToClipMatrixForFrustum);
    }

    // CameraController

    void FlyCameraController::SetCamera(PerspectiveCamera& camera)
    {
        m_Camera = &camera;
    }

    bool FlyCameraController::OnRenderViewportResized(uint32_t width, uint32_t height)
    {
        if (m_Camera == nullptr)
            return false;

        const float aspectRatio = (float)width / height;
        m_Camera->SetLens(m_Camera->GetVerticalFovInRadians(), aspectRatio, m_Camera->GetNearPlane());

        return false;
    }

    void FlyCameraController::MoveCamera(std::chrono::microseconds dt)
    {
        if (m_Camera == nullptr)
            return;

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
        const DirectX::XMVECTOR& position = m_Camera->GetPosition();

        DirectX::XMVECTOR updatedPosition = DirectX::XMVectorZero();

        // Front / Back
        {
            const DirectX::XMVECTOR& frontDirection = m_Camera->GetFrontDirection();

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
            const DirectX::XMVECTOR& rightDirection = m_Camera->GetRightDirection();

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
            const DirectX::XMVECTOR& upDirection = m_Camera->GetUpDirection();

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
            m_Camera->SetPosition(updatedPosition);
        }
    }

    void FlyCameraController::RotateCamera(DirectX::XMINT2 mousePosition, DirectX::XMINT2 prevMousePosition)
    {
        if (m_Camera == nullptr)
            return;

        const auto deltaX = (float)(mousePosition.x - prevMousePosition.x);
        const auto deltaY = (float)(mousePosition.y - prevMousePosition.y);

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

        m_Camera->SetFrontDirection(GetDirectionFromPitchYaw(m_Pitch, m_Yaw));
    }

    void FlyCameraController::IncrementFov(float direction)
    {
        if (m_Camera == nullptr)
            return;

        constexpr float minVerticalFovInRadians = DirectX::XMConvertToRadians(45.0f);
        constexpr float maxVerticalFovInRadians = DirectX::XMConvertToRadians(120.0f);

        float verticalFov = m_Camera->GetVerticalFovInRadians() - m_MouseWheelSensitivity * direction;
        verticalFov = std::clamp(verticalFov, minVerticalFovInRadians, maxVerticalFovInRadians);

        m_Camera->SetLens(verticalFov, m_Camera->GetAspectRatio(), m_Camera->GetNearPlane());
    }

    void FlyCameraController::UpdatePitchAndYawIfNeeded()
    {
        const DirectX::XMVECTOR eplison = DirectX::XMVectorReplicate(1e-4f);
        const DirectX::XMVECTOR& frontDirection = m_Camera->GetFrontDirection();

        if (DirectX::XMVector3NearEqual(frontDirection, GetDirectionFromPitchYaw(m_Pitch, m_Yaw), eplison))
            return;

        const auto [pitch, yaw] = GetPitchYawFromDirection(frontDirection);
        m_Pitch = pitch;
        m_Yaw = yaw;
    }

}
