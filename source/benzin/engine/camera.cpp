#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/camera.hpp"

#include "benzin/core/logger.hpp"
#include "benzin/core/math.hpp"
#include "benzin/system/input.hpp"

namespace benzin
{

    static void RenderImGuiMatrix4x4(const DirectX::XMMATRIX& matrix)
        {
            const uint32_t matrixSize = 4;

            const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

            if (ImGui::BeginTable("View Matrix", matrixSize, flags))
            {
                for (uint32_t rowIndex = 0; rowIndex < matrixSize; ++rowIndex)
                {
                    ImGui::TableNextRow();

                    const DirectX::XMVECTOR row = matrix.r[rowIndex];

                    for (uint32_t columnIndex = 0; columnIndex < matrixSize; ++columnIndex)
                    {
                        ImGui::TableSetColumnIndex(columnIndex);

                        const float cellValue = *(reinterpret_cast<const float*>(&row) + columnIndex);
                        ImGui::Text("%0.4f", cellValue);
                    }
                }
                ImGui::EndTable();
            }
        }

    // Projection

    DirectX::BoundingFrustum Projection::GetTransformedBoundingFrustum(const DirectX::XMMATRIX& transform) const
    {
        DirectX::BoundingFrustum transformedBoundingFrustum;
        m_BoundingFrustum.Transform(transformedBoundingFrustum, transform);

        return transformedBoundingFrustum;
    }

    void Projection::UpdateMatrix()
    {
        m_ProjectionMatrix = CreateMatrix();
        m_InverseProjectionMatrix = DirectX::XMMatrixInverse(nullptr, m_ProjectionMatrix);

        DirectX::BoundingFrustum::CreateFromMatrix(m_BoundingFrustum, m_ProjectionMatrix);
    };

    // PerspectiveProjection

    PerspectiveProjection::PerspectiveProjection(float fov, float aspectRatio, float nearPlane, float farPlane)
    {
        SetLens(fov, aspectRatio, nearPlane, farPlane);
    }
    
    void PerspectiveProjection::SetFOV(float fov)
    {
        m_FOV = fov;

        UpdateMatrix();
    }

    void PerspectiveProjection::SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;

        UpdateMatrix();
    }

    void PerspectiveProjection::SetLens(float fov, float aspectRatio, float nearPlane, float farPlane)
    {
        m_FOV = fov;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;

        UpdateMatrix();
    }

    DirectX::XMMATRIX PerspectiveProjection::CreateMatrix() const
    {
        return DirectX::XMMatrixPerspectiveFovLH(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);
    }

    // OrthographicProjection

    void OrthographicProjection::SetViewRect(const ViewRect& viewRect)
    {
        m_ViewRect = viewRect;

        UpdateMatrix();
    };

    DirectX::XMMATRIX OrthographicProjection::CreateMatrix() const
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
        UpdateViewMatrix();
    }

    void Camera::SetPosition(const DirectX::XMVECTOR& position)
    {
        m_Position = position;

        UpdateViewMatrix();
    }

    void Camera::SetFrontDirection(const DirectX::XMVECTOR& frontDirection)
    {
        m_FrontDirection = frontDirection;
        DirectX::XMVector3Normalize(m_FrontDirection);

        UpdateRightDirection();
        UpdateViewMatrix();
    }

    void Camera::SetUpDirection(const DirectX::XMVECTOR& upDirection)
    {
        m_UpDirection = upDirection;
        DirectX::XMVector3Normalize(m_UpDirection);

        UpdateRightDirection();
        UpdateViewMatrix();
    }

    const DirectX::XMMATRIX& Camera::GetProjectionMatrix() const
    {
        return m_Projection.GetMatrix();
    }

    const DirectX::XMMATRIX& Camera::GetInverseProjectionMatrix() const
    {
        return m_Projection.GetInverseMatrix();
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const
    {
        return m_ViewMatrix * GetProjectionMatrix();
    }

    DirectX::XMMATRIX Camera::GetInverseViewProjectionMatrix() const
    {
        return DirectX::XMMatrixInverse(nullptr, GetViewProjectionMatrix());
    }

    DirectX::XMMATRIX Camera::GetInverseViewDirectionProjectionMatrix() const
    {
        DirectX::XMMATRIX viewDirectionMatrix = m_ViewMatrix;
        viewDirectionMatrix.r[3] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Removes translation

        const DirectX::XMMATRIX viewDirectionProjectionMatrix = viewDirectionMatrix * GetProjectionMatrix();
        return DirectX::XMMatrixInverse(nullptr, viewDirectionProjectionMatrix);
    }

    void Camera::UpdateRightDirection()
    {
        m_RightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_FrontDirection, m_UpDirection));
    }

    void Camera::UpdateViewMatrix()
    {
        m_ViewMatrix = DirectX::XMMatrixLookToLH(m_Position, m_FrontDirection, m_UpDirection);
        m_InverseViewMatrix = DirectX::XMMatrixInverse(nullptr, m_ViewMatrix);
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

    void FlyCameraController::OnEvent(Event& event)
    {
        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch(&FlyCameraController::OnWindowResized, *this);
        dispatcher.Dispatch(&FlyCameraController::OnMouseMoved, *this);
        dispatcher.Dispatch(&FlyCameraController::OnMouseScrolled, *this);
    }

    void FlyCameraController::OnUpdate(std::chrono::microseconds dt)
    {
        UpdatePitchAndYawIfNeeded();

        const float delta = m_CameraTranslationSpeed * ToFloatMS(dt);
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

    void FlyCameraController::OnImGuiRender()
    {
        ImGui::Begin("FlyCameraController");
        {
            RenderImGuiControllerProperties();
            ImGui::Separator();
            RenderImGuiViewProperties();
            ImGui::Separator();
            RenderImGuiProjectionProperties();
        }
        ImGui::End();
    }

    bool FlyCameraController::OnWindowResized(WindowResizedEvent& event)
    {
        if (auto* perspectiveProjection = GetPerspectiveProjection())
        {
            const float aspectRatio = event.GetAspectRatio();
            perspectiveProjection->SetAspectRatio(aspectRatio);
        }

        return false;
    }

    bool FlyCameraController::OnMouseMoved(MouseMovedEvent& event)
    {
        if (Input::IsMouseButtonPressed(MouseButton::Left))
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

        return false;
    }

    bool FlyCameraController::OnMouseScrolled(MouseScrolledEvent& event)
    {
        static const float minFOV = DirectX::XM_PIDIV4; // 45 degrees
        static const float maxFOV = DirectX::XM_PI * 2.0f / 3.0f; // 120 degrees

        if (auto* perspectiveProjection = GetPerspectiveProjection())
        {
            const float fov = perspectiveProjection->GetFOV() - m_MouseWheelSensitivity * (float)event.GetOffsetX();
            perspectiveProjection->SetFOV(std::clamp(fov, minFOV, maxFOV));
        }

        return false;
    }

    void FlyCameraController::RenderImGuiControllerProperties()
    {
        ImGui::Text("Controller Properties");
        ImGui::SliderFloat("CameraTranslationSpeed", &m_CameraTranslationSpeed, 0.001f, 0.01f);
        ImGui::SliderFloat("MouseSensitivity", &m_MouseSensitivity, 0.001f, 0.007f, "%.3f");
    }

    void FlyCameraController::RenderImGuiViewProperties()
    {
        ImGui::Text("View Properties");

        if (ImGui::DragFloat3("Position", reinterpret_cast<float*>(&m_Camera.m_Position)))
        {
            m_Camera.UpdateViewMatrix();
        }

        if (ImGui::DragFloat3("Front Direction", reinterpret_cast<float*>(&m_Camera.m_FrontDirection)))
        {
            m_Camera.UpdateViewMatrix();
        }

        if (ImGui::DragFloat3("Up Direction", reinterpret_cast<float*>(&m_Camera.m_UpDirection)))
        {
            m_Camera.UpdateViewMatrix();
        }

        if (ImGui::SliderAngle("Pitch (X)", &m_Pitch, -89.0f, 89.0f))
        {
            m_Camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Pitch, m_Yaw));
        }

        if (ImGui::SliderAngle("Yaw (Y)", &m_Yaw, -180.0f, 180.0f))
        {
            m_Camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Pitch, m_Yaw));
        }

        RenderImGuiMatrix4x4(m_Camera.GetViewMatrix());
    }

    void FlyCameraController::RenderImGuiProjectionProperties()
    {
        if (auto* perspectiveProjection = GetPerspectiveProjection())
        {
            ImGui::Text("Projection Properties");

            if (ImGui::SliderAngle("FOV", &perspectiveProjection->m_FOV, 45.0f, 120.0f))
            {
                perspectiveProjection->UpdateMatrix();
            }

            ImGui::Text("AspectRatio: %f", perspectiveProjection->m_AspectRatio);
            ImGui::Text("NearPlane: %f", perspectiveProjection->m_NearPlane);
            ImGui::Text("FarPlane: %f", perspectiveProjection->m_FarPlane);
        }

        RenderImGuiMatrix4x4(m_Camera.GetProjectionMatrix());
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
