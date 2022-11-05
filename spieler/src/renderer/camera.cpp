#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/camera.hpp"

#include <third_party/imgui/imgui.h>

#include "spieler/core/application.hpp"

#include "spieler/system/input.hpp"
#include "spieler/system/event_dispatcher.hpp"

namespace spieler::renderer
{

    //////////////////////////////////////////////////////////////////////////
    /// Camera
    //////////////////////////////////////////////////////////////////////////
    Camera::Camera()
    {
        UpdateView();
    }

    void Camera::SetPosition(const DirectX::XMVECTOR& position)
    {
        m_Position = position;

        UpdateView();
    }

    void Camera::SetFrontDirection(const DirectX::XMVECTOR& frontDirection)
    {
        m_FrontDirection = frontDirection;
        DirectX::XMVector3Normalize(m_FrontDirection);
        
        UpdateRightDirection();
        UpdateView();
    }

    void Camera::SetUpDirection(const DirectX::XMVECTOR& upDirection)
    {
        m_UpDirection = upDirection;
        DirectX::XMVector3Normalize(m_UpDirection);

        UpdateRightDirection();
        UpdateView();
    }

    void Camera::SetFOV(float fov)
    {
        m_FOV = fov;

        UpdateProjection();
    }

    void Camera::SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;

        UpdateProjection();
    }

    void Camera::SetNearPlane(float nearPlane)
    {
        m_NearPlane = nearPlane;
        
        UpdateProjection();
    }

    void Camera::SetFarPlane(float farPlane)
    {
        m_FarPlane = farPlane;

        UpdateProjection();
    }

    void Camera::UpdateRightDirection()
    {
        m_RightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_FrontDirection, m_UpDirection));
    }

    void Camera::UpdateView()
    {
        m_View = DirectX::XMMatrixLookAtLH(m_Position, DirectX::XMVectorAdd(m_Position, m_FrontDirection), m_UpDirection);

        DirectX::XMVECTOR viewDeterminant{ DirectX::XMMatrixDeterminant(m_View) };
        m_InverseView = DirectX::XMMatrixInverse(&viewDeterminant, m_View);

        m_ViewProjection = m_View * m_Projection;
    }

    void Camera::UpdateProjection()
    {
        m_Projection = DirectX::XMMatrixPerspectiveFovLH(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);

        DirectX::XMVECTOR projectionDeterminant{ DirectX::XMMatrixDeterminant(m_Projection) };
        m_InverseProjection = DirectX::XMMatrixInverse(&projectionDeterminant, m_Projection);

        DirectX::BoundingFrustum::CreateFromMatrix(m_BoundingFrustum, m_Projection);

        m_ViewProjection = m_View * m_Projection;
    }

    //////////////////////////////////////////////////////////////////////////
    /// CameraController
    //////////////////////////////////////////////////////////////////////////
    CameraController::CameraController(Camera& camera)
        : m_Camera{ &camera }
    {
        m_Camera->SetAspectRatio(Application::GetInstance().GetWindow().GetAspectRatio());
    }

    void CameraController::OnEvent(spieler::Event& event)
    {
        SPIELER_ASSERT(m_Camera);

        spieler::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<spieler::MouseButtonPressedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseButtonPressed));
        dispatcher.Dispatch<spieler::MouseMovedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseMoved));
        dispatcher.Dispatch<spieler::MouseScrolledEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseScrolled));
    }

    void CameraController::OnUpdate(float dt)
    {
        SPIELER_ASSERT(m_Camera);

        const spieler::Input& input{ spieler::Application::GetInstance().GetWindow().GetInput() };

        const float delta{ m_CameraSpeed * dt };

        DirectX::XMVECTOR updatedPosition{ DirectX::XMVectorZero() };

        // Front / Back
        if (input.IsKeyPressed(KeyCode::W))
        {
            updatedPosition = DirectX::XMVectorAdd(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetFrontDirection(), delta));
        }
        else if (input.IsKeyPressed(KeyCode::S))
        {
            updatedPosition = DirectX::XMVectorSubtract(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetFrontDirection(), delta));
        }

        // Left / Right
        if (input.IsKeyPressed(KeyCode::A))
        {
            updatedPosition = DirectX::XMVectorAdd(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetRightDirection(), delta));
        }
        else if (input.IsKeyPressed(KeyCode::D))
        {
            updatedPosition = DirectX::XMVectorSubtract(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetRightDirection(), delta));
        }

        // Up / Down
        if (input.IsKeyPressed(KeyCode::Space))
        {
            updatedPosition = DirectX::XMVectorAdd(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetUpDirection(), delta));
        }
        else if (input.IsKeyPressed(KeyCode::C))
        {
            updatedPosition = DirectX::XMVectorSubtract(m_Camera->GetPosition(), DirectX::XMVectorScale(m_Camera->GetUpDirection(), delta));
        }

        if (!DirectX::XMVector4Equal(updatedPosition, DirectX::XMVectorZero()))
        {
            m_Camera->SetPosition(updatedPosition);
        }
    }

    void CameraController::OnImGuiRender(float dt)
    {
        SPIELER_ASSERT(m_Camera);

        ImGui::Begin("Camera Controller");
        {
            {
                ImGui::Text("View Properties");

                if (ImGui::DragFloat3("Position", reinterpret_cast<float*>(&m_Camera->m_Position)))
                {
                    m_Camera->UpdateView();
                }

                if (ImGui::DragFloat3("Front Direction", reinterpret_cast<float*>(&m_Camera->m_FrontDirection)))
                {
                    m_Camera->UpdateView();
                }

                if (ImGui::DragFloat3("Up Direction", reinterpret_cast<float*>(&m_Camera->m_UpDirection)))
                {
                    m_Camera->UpdateView();
                }
            }

            {
                ImGui::Separator();
                ImGui::Text("Projection Properties");

                if (ImGui::SliderAngle("FOV", &m_Camera->m_FOV, 45.0f, 120.0f))
                {
                    m_Camera->UpdateProjection();
                }

                ImGui::Text("AspectRatio: %f", m_Camera->m_AspectRatio);
                ImGui::Text("NearPlane: %f", m_Camera->m_NearPlane);
                ImGui::Text("FarPlane: %f", m_Camera->m_FarPlane);
            }

            {
                ImGui::Separator();
                ImGui::Text("Controller Properties");

                if (ImGui::SliderAngle("Theta (XZ)", &m_Theta, -180.0f, 180.0f))
                {
                    m_Camera->SetFrontDirection(GetFrontDirection());
                }

                if (ImGui::SliderAngle("Phi (Y)", &m_Phi, 0.01f, 179.0f))
                {
                    m_Camera->SetFrontDirection(GetFrontDirection());
                }

                ImGui::SliderFloat("CameraSpeed", &m_CameraSpeed, 20.0f, 100.0f);
                ImGui::SliderFloat("MouseSensitivity", &m_MouseSensitivity, 0.001f, 0.007f, "%.3f");
            }
        }
        ImGui::End();
    }

    bool CameraController::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        m_Camera->SetAspectRatio(event.GetWidth<float>() / event.GetHeight<float>());

        return false;
    }

    bool CameraController::OnMouseButtonPressed(spieler::MouseButtonPressedEvent& event)
    {
        m_LastMousePosition.x = event.GetX<float>();
        m_LastMousePosition.y = event.GetY<float>();

        return false;
    }

    bool CameraController::OnMouseMoved(spieler::MouseMovedEvent& event)
    {
        const spieler::Input& input{ Application::GetInstance().GetWindow().GetInput() };

        if (input.IsMouseButtonPressed(MouseButton::Left))
        {
            const float deltaX{ event.GetX<float>() - m_LastMousePosition.x };
            const float deltaY{ event.GetY<float>() - m_LastMousePosition.y };

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

            m_Camera->SetFrontDirection(GetFrontDirection());
        }

        m_LastMousePosition.x = event.GetX<float>();
        m_LastMousePosition.y = event.GetY<float>();

        return false;
    }

    bool CameraController::OnMouseScrolled(spieler::MouseScrolledEvent& event)
    {
        static const float minFOV{ DirectX::XM_PIDIV4 }; // 45 degrees
        static const float maxFOV{ DirectX::XM_PI * 2.0f / 3.0f }; // 120 degrees

        m_Camera->SetFOV(std::clamp(m_Camera->GetFOV() - m_MouseWheelSensitivity * static_cast<float>(event.GetOffsetX()), minFOV, maxFOV));
    
        return false;
    }

    DirectX::XMVECTOR CameraController::GetFrontDirection() const
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

} // namespace spieler::renderer
