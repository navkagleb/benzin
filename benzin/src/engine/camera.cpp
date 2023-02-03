#include "benzin/config/bootstrap.hpp"

#include "benzin/engine/camera.hpp"

#include <third_party/imgui/imgui.h>

#include "benzin/system/input.hpp"
#include "benzin/system/event_dispatcher.hpp"

namespace benzin
{

    DirectX::BoundingFrustum Projection::GetTransformedBoundingFrustum(const DirectX::XMMATRIX& transform) const
    {
        DirectX::BoundingFrustum transformedBoundingFrustum;
        m_BoundingFrustum.Transform(transformedBoundingFrustum, transform);

        return transformedBoundingFrustum;
    }

    void Projection::UpdateMatrix()
    {
        m_Projection = CreateMatrix();

        DirectX::XMVECTOR projectionDeterminant = DirectX::XMMatrixDeterminant(m_Projection);
        m_InverseProjection = DirectX::XMMatrixInverse(&projectionDeterminant, m_Projection);

        DirectX::BoundingFrustum::CreateFromMatrix(m_BoundingFrustum, m_Projection);
    };

    //////////////////////////////////////////////////////////////////////////
    /// PerspectiveProjection
    //////////////////////////////////////////////////////////////////////////
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

    //////////////////////////////////////////////////////////////////////////
    /// OrthographicProjection
    //////////////////////////////////////////////////////////////////////////
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

    //////////////////////////////////////////////////////////////////////////
    /// Camera
    //////////////////////////////////////////////////////////////////////////
    Camera::Camera()
    {
        UpdateViewMatrix();
    }

    Camera::Camera(const Projection* projection)
    {
        SetProjection(projection);
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

    void Camera::SetProjection(const Projection* projection)
    {
        BENZIN_ASSERT(projection);

        m_Projection = projection;
    }

    const DirectX::XMMATRIX& Camera::GetProjectionMatrix() const
    {
        BENZIN_ASSERT(m_Projection);

        return m_Projection->GetMatrix();
    }

    const DirectX::XMMATRIX& Camera::GetInverseProjectionMatrix() const
    {
        BENZIN_ASSERT(m_Projection);

        return m_Projection->GetInverseMatrix();
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const
    {
        BENZIN_ASSERT(m_Projection);

        return m_ViewMatrix * m_Projection->GetMatrix();
    }

    void Camera::UpdateRightDirection()
    {
        m_RightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_FrontDirection, m_UpDirection));
    }

    void Camera::UpdateViewMatrix()
    {
        m_ViewMatrix = DirectX::XMMatrixLookAtLH(m_Position, DirectX::XMVectorAdd(m_Position, m_FrontDirection), m_UpDirection);

        DirectX::XMVECTOR viewDeterminant = DirectX::XMMatrixDeterminant(m_ViewMatrix);
        m_InverseViewMatrix = DirectX::XMMatrixInverse(&viewDeterminant, m_ViewMatrix);
    }

    //////////////////////////////////////////////////////////////////////////
    /// CameraController
    //////////////////////////////////////////////////////////////////////////
    FlyCameraController::FlyCameraController(Camera* camera)
        : m_Camera{ camera }
    {}

    void FlyCameraController::SetCamera(Camera* camera)
    {
        BENZIN_ASSERT(camera);

        m_Camera = camera;
    }

    void FlyCameraController::OnEvent(Event& event)
    {
        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<WindowResizedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnMouseButtonPressed));
        dispatcher.Dispatch<MouseMovedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnMouseMoved));
        dispatcher.Dispatch<MouseScrolledEvent>(BENZIN_BIND_EVENT_CALLBACK(OnMouseScrolled));
    }

    void FlyCameraController::OnUpdate(float dt)
    {
        const float delta = m_CameraTranslationSpeed * dt;
        const auto& position = m_Camera->GetPosition();

        DirectX::XMVECTOR updatedPosition = DirectX::XMVectorZero();

        // Front / Back
        {
            const auto& frontDirection = m_Camera->GetFrontDirection();

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
            const auto& rightDirection = m_Camera->GetRightDirection();

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
            const auto& upDirection = m_Camera->GetUpDirection();

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

    void FlyCameraController::OnImGuiRender(float dt)
    {
        BENZIN_ASSERT(m_Camera);

        ImGui::Begin("Camera Controller");
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
            const float aspectRatio = event.GetWidth<float>() / event.GetHeight<float>();
            perspectiveProjection->SetAspectRatio(aspectRatio);
        }

        return false;
    }

    bool FlyCameraController::OnMouseButtonPressed(MouseButtonPressedEvent& event)
    {
        m_LastMousePosition.x = event.GetX<float>();
        m_LastMousePosition.y = event.GetY<float>();

        return false;
    }

    bool FlyCameraController::OnMouseMoved(MouseMovedEvent& event)
    {
        BENZIN_ASSERT(m_Camera);

        if (Input::IsMouseButtonPressed(MouseButton::Left))
        {
            const float deltaX = event.GetX<float>() - m_LastMousePosition.x;
            const float deltaY = event.GetY<float>() - m_LastMousePosition.y;

            m_Theta -= m_MouseSensitivity * deltaX;
            m_Phi -= m_MouseSensitivity * deltaY;

            m_Phi = std::clamp(m_Phi, 0.01f, DirectX::XM_PI - 0.01f);

            if (m_Theta > DirectX::XM_PI)
            {
                m_Theta = -DirectX::XM_PI;
            }

            if (m_Theta < -DirectX::XM_PI)
            {
                m_Theta = DirectX::XM_PI;
            }

            m_Camera->SetFrontDirection(GetCameraFrontDirection());
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
            const float fov = perspectiveProjection->GetFOV() - m_MouseWheelSensitivity * static_cast<float>(event.GetOffsetX());
            perspectiveProjection->SetFOV(std::clamp(fov, minFOV, maxFOV));
        }

        return false;
    }

    void FlyCameraController::RenderImGuiControllerProperties()
    {
        ImGui::Text("Controller Properties");
        ImGui::SliderFloat("CameraTranslationSpeed", &m_CameraTranslationSpeed, 5.0f, 100.0f);
        ImGui::SliderFloat("MouseSensitivity", &m_MouseSensitivity, 0.001f, 0.007f, "%.3f");
    }

    void FlyCameraController::RenderImGuiViewProperties()
    {
        ImGui::Text("View Properties");

        if (ImGui::DragFloat3("Position", reinterpret_cast<float*>(&m_Camera->m_Position)))
        {
            m_Camera->UpdateViewMatrix();
        }

        if (ImGui::DragFloat3("Front Direction", reinterpret_cast<float*>(&m_Camera->m_FrontDirection)))
        {
            m_Camera->UpdateViewMatrix();
        }

        if (ImGui::DragFloat3("Up Direction", reinterpret_cast<float*>(&m_Camera->m_UpDirection)))
        {
            m_Camera->UpdateViewMatrix();
        }

        ImGui::Separator();

        if (ImGui::SliderAngle("Theta (XZ)", &m_Theta, -180.0f, 180.0f))
        {
            m_Camera->SetFrontDirection(GetCameraFrontDirection());
        }

        if (ImGui::SliderAngle("Phi (Y)", &m_Phi, 0.01f, 179.0f))
        {
            m_Camera->SetFrontDirection(GetCameraFrontDirection());
        }
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
    }

    DirectX::XMVECTOR FlyCameraController::GetCameraFrontDirection() const
    {
        // Spherical To Cartesian
        const DirectX::XMVECTOR frontDirection
        {
            DirectX::XMVectorSet(
                DirectX::XMScalarSin(m_Phi) * DirectX::XMScalarSin(m_Theta),
                DirectX::XMScalarCos(m_Phi),
                DirectX::XMScalarSin(m_Phi) * DirectX::XMScalarCos(m_Theta),
                0.0f
            )
        };

        return DirectX::XMVector3Normalize(frontDirection);
    }

    PerspectiveProjection* FlyCameraController::GetPerspectiveProjection()
    {
        const auto* perspectiveProjection = dynamic_cast<const PerspectiveProjection*>(m_Camera->GetProjection());

        if (!perspectiveProjection)
        {
            BENZIN_WARNING("Projection isn't Perspective! FlyCameraController supports only PerspectiveProjection");
            return nullptr;
        }

        return const_cast<PerspectiveProjection*>(perspectiveProjection);
    }

#if 0
    //////////////////////////////////////////////////////////////////////////
    /// PerspectiveCameraController
    //////////////////////////////////////////////////////////////////////////
    PerspectiveCameraController::PerspectiveCameraController(Camera* camera)
        : CameraController{ camera }
    {}

    std::optional<DirectX::XMVECTOR> PerspectiveCameraController::MoveCamera(float dt)
    {
        BENZIN_ASSERT(m_Camera);

        const float delta = m_CameraTranslationSpeed * dt;

        DirectX::XMVECTOR updatedPosition = DirectX::XMVectorZero();

        // Front / Back
        if (Input::IsKeyPressed(KeyCode::W))
        {
            updatedPosition = DirectX::XMVectorAdd(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetFrontDirection(), delta));
        }
        else if (Input::IsKeyPressed(KeyCode::S))
        {
            updatedPosition = DirectX::XMVectorSubtract(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetFrontDirection(), delta));
        }

        // Left / Right
        if (Input::IsKeyPressed(KeyCode::A))
        {
            updatedPosition = DirectX::XMVectorAdd(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetRightDirection(), delta));
        }
        else if (Input::IsKeyPressed(KeyCode::D))
        {
            updatedPosition = DirectX::XMVectorSubtract(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetRightDirection(), delta));
        }

        // Up / Down
        if (Input::IsKeyPressed(KeyCode::Space))
        {
            updatedPosition = DirectX::XMVectorAdd(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetUpDirection(), delta));
        }
        else if (Input::IsKeyPressed(KeyCode::C))
        {
            updatedPosition = DirectX::XMVectorSubtract(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetUpDirection(), delta));
        }

        if (!DirectX::XMVector4Equal(updatedPosition, DirectX::XMVectorZero()))
        {
            return updatedPosition;
        }

        return std::nullopt;
    }

    bool PerspectiveCameraController::OnWindowResized(WindowResizedEvent& event)
    {
        BENZIN_ASSERT(m_Camera);

        auto* perspectiveProjection = const_cast<PerspectiveProjection*>(static_cast<const PerspectiveProjection*>(m_Camera->GetProjection()));

        perspectiveProjection->SetAspectRatio(event.GetWidth<float>() / event.GetHeight<float>());

        return false;
    }

    bool PerspectiveCameraController::OnMouseScrolled(MouseScrolledEvent& event)
    {
        BENZIN_ASSERT(m_Camera);

        static const float minFOV = DirectX::XM_PIDIV4; // 45 degrees
        static const float maxFOV = DirectX::XM_PI * 2.0f / 3.0f; // 120 degrees

        auto* perspectiveProjection = const_cast<PerspectiveProjection*>(static_cast<const PerspectiveProjection*>(m_Camera->GetProjection()));

        const float fov = perspectiveProjection->GetFOV() - m_MouseWheelSensitivity * static_cast<float>(event.GetOffsetX());

        perspectiveProjection->SetFOV(std::clamp(fov, minFOV, maxFOV));

        return false;
    }

    void PerspectiveCameraController::RenderImGuiProjectionProperties()
    {
        auto* perspectiveProjection = const_cast<PerspectiveProjection*>(static_cast<const PerspectiveProjection*>(m_Camera->GetProjection()));

        ImGui::Text("Projection Properties");

        if (ImGui::SliderAngle("FOV", &perspectiveProjection->m_FOV, 45.0f, 120.0f))
        {
            perspectiveProjection->UpdateMatrix();
        }

        ImGui::Text("AspectRatio: %f", perspectiveProjection->m_AspectRatio);
        ImGui::Text("NearPlane: %f", perspectiveProjection->m_NearClip);
        ImGui::Text("FarPlane: %f", perspectiveProjection->m_FarClip);
    }

    //////////////////////////////////////////////////////////////////////////
    /// OrthographicCameraController
    //////////////////////////////////////////////////////////////////////////
    OrthographicCameraController::OrthographicCameraController(Camera* camera)
        : CameraController{ camera }
    {}

    std::optional<DirectX::XMVECTOR> OrthographicCameraController::MoveCamera(float dt)
    {
        BENZIN_ASSERT(m_Camera);

        const float delta = m_CameraTranslationSpeed * dt;

        float x = DirectX::XMVectorGetX(m_Camera->GetPosition());
        float y = DirectX::XMVectorGetY(m_Camera->GetPosition());

        if (Input::IsKeyPressed(KeyCode::W))
        {
            x += std::sin(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
            y += -std::cos(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
        }
        else if (Input::IsKeyPressed(KeyCode::S))
        {
            x -= std::sin(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
            y -= -std::cos(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
        }

        if (Input::IsKeyPressed(KeyCode::A))
        {
            x -= std::cos(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
            y -= std::sin(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
        }
        else if (Input::IsKeyPressed(KeyCode::D))
        {
            x += std::cos(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
            y += std::sin(DirectX::XMConvertToRadians(m_CameraRotation)) * delta;
        }

        return DirectX::XMVECTOR{ x, y, 0.0f, 0.0f };
    }

    bool OrthographicCameraController::OnWindowResized(WindowResizedEvent& event)
    {
        BENZIN_ASSERT(m_Camera);

        m_AspectRatio = event.GetWidth<float>() / event.GetHeight<float>();

        UpdateCameraViewRect();

        return false;
    }

    bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& event)
    {
        BENZIN_ASSERT(m_Camera);

        static const float minCameraZoomLevel = 1.0f;

        m_CameraZoomLevel = std::max(
            minCameraZoomLevel,
            m_CameraZoomLevel + m_MouseWheelSensitivity * static_cast<float>(event.GetOffsetX())
        );

        UpdateCameraViewRect();

        return false;
    }

    void OrthographicCameraController::RenderImGuiProjectionProperties()
    {
        ImGui::Text("Projection Properties");
    }

    void OrthographicCameraController::UpdateCameraViewRect()
    {
        auto* orthographicProjection = const_cast<OrthographicProjection*>(static_cast<const OrthographicProjection*>(m_Camera->GetProjection()));

        orthographicProjection->SetViewRect(Rect
        {
            .X{ -m_AspectRatio * m_CameraZoomLevel },
            .Y{ -m_CameraZoomLevel },
            .Width{ m_AspectRatio * 2.0f * m_CameraZoomLevel },
            .Height{ 2.0f * m_CameraZoomLevel }
        });
    }

#endif

} // namespace benzin
