#include "bootstrap.hpp"

#include "projection_camera_controller.hpp"

#include <third_party/imgui/imgui.h>

#include <spieler/core/application.hpp>

#include <spieler/system/input.hpp>
#include <spieler/system/event_dispatcher.hpp>

namespace sandbox
{

    ProjectionCameraController::ProjectionCameraController(float fov, float aspectRatio)
        : m_Camera
        {
            .FOV{ fov },
            .AspectRatio{ aspectRatio }
        }
    {
        UpdateView();
        UpdateProjection();
    }

    void ProjectionCameraController::OnEvent(spieler::Event& event)
    {
        spieler::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        if (!m_IsBlocked)
        {
            dispatcher.Dispatch<spieler::MouseButtonPressedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseButtonPressed));
            dispatcher.Dispatch<spieler::MouseMovedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseMoved));
            dispatcher.Dispatch<spieler::MouseScrolledEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseScrolled));
        }
    }

    void ProjectionCameraController::OnUpdate(float dt)
    {
        const spieler::Input& input{ spieler::Application::GetInstance()->GetWindow().GetInput() };

        if (m_IsBlocked)
        {
            return;
        }

        bool triggered{ false };

        if (input.IsKeyPressed(spieler::KeyCode::W))
        {
            m_Camera.EyePosition = DirectX::XMVectorAdd(m_Camera.EyePosition, DirectX::XMVectorScale(m_Camera.Front, m_CameraSpeed * dt));
            triggered = true;
        }

        if (input.IsKeyPressed(spieler::KeyCode::S))
        {
            m_Camera.EyePosition = DirectX::XMVectorSubtract(m_Camera.EyePosition, DirectX::XMVectorScale(m_Camera.Front, m_CameraSpeed * dt));
            triggered = true;
        }

        if (input.IsKeyPressed(spieler::KeyCode::A))
        {   
            m_Camera.EyePosition = DirectX::XMVectorAdd(m_Camera.EyePosition, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_Camera.Front, m_Camera.UpDirection)), m_CameraSpeed * dt));
            triggered = true;
        }

        if (input.IsKeyPressed(spieler::KeyCode::D))
        {
            m_Camera.EyePosition = DirectX::XMVectorSubtract(m_Camera.EyePosition, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_Camera.Front, m_Camera.UpDirection)), m_CameraSpeed * dt));
            triggered = true;
        }

        if (triggered)
        {
            UpdateView();
        }
    }

    void ProjectionCameraController::OnImGuiRender(float dt)
    {
        ImGui::Begin("Projection Camera");
        {
            if (ImGui::DragFloat3("EyePosition", reinterpret_cast<float*>(&m_Camera.EyePosition)))
            {
                UpdateView();
            }

            if (ImGui::DragFloat3("Front", reinterpret_cast<float*>(&m_Camera.Front)))
            {
                UpdateView();
            }

            if (ImGui::DragFloat3("Up", reinterpret_cast<float*>(&m_Camera.UpDirection)))
            {
                UpdateView();
            }

            ImGui::Separator();

            if (ImGui::SliderAngle("FOV", &m_Camera.FOV, 45.0f, 120.0f))
            {
                UpdateProjection();
            }

            ImGui::Separator();

            if (ImGui::SliderAngle("Theta (XZ)", &m_Theta, -180.0f, 180.0f))
            {
                UpdateView();
            }

            if (ImGui::SliderAngle("Phi (Y)", &m_Phi, 0.01f, 179.0f))
            {
                UpdateView();
            }

            ImGui::SliderFloat("CameraSpeed", &m_CameraSpeed, 20.0f, 100.0f);
            ImGui::SliderFloat("MouseSensitivity", &m_MouseSensitivity, 0.001f, 0.007f, "%.3f");
            ImGui::Checkbox("IsBlocked", &m_IsBlocked);
        }
        ImGui::End();
    }

    bool ProjectionCameraController::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        m_Camera.AspectRatio = event.GetWidth<float>() / event.GetHeight<float>();

        UpdateProjection();

        return false;
    }

    bool ProjectionCameraController::OnMouseButtonPressed(spieler::MouseButtonPressedEvent& event)
    {
        m_LastMousePosition.x = event.GetX<float>();
        m_LastMousePosition.y = event.GetY<float>();

        return false;
    }

    bool ProjectionCameraController::OnMouseMoved(spieler::MouseMovedEvent& event)
    {
        const spieler::Input& input{ spieler::Application::GetInstance()->GetWindow().GetInput() };

        if (input.IsMouseButtonPressed(spieler::MouseButton::Left))
        {
            const float dx{ event.GetX<float>() - m_LastMousePosition.x };
            const float dy{ event.GetY<float>() - m_LastMousePosition.y };

            m_Theta -= m_MouseSensitivity * dx;
            m_Phi -= m_MouseSensitivity * dy;

            m_Phi = spieler::math::Clamp(0.01f, m_Phi, DirectX::XM_PI - 0.01f);

            if (m_Theta > DirectX::XM_PI)
            {
                m_Theta = -DirectX::XM_PI;
            }

            if (m_Theta < -DirectX::XM_PI)
            {
                m_Theta = DirectX::XM_PI;
            }

            UpdateView();
        }

        m_LastMousePosition.x = static_cast<float>(event.GetX());
        m_LastMousePosition.y = static_cast<float>(event.GetY());

        return false;
    }

    bool ProjectionCameraController::OnMouseScrolled(spieler::MouseScrolledEvent& event)
    {
        static const float min{ DirectX::XM_PIDIV4 }; // 45 degrees
        static const float max{ DirectX::XM_PI * 2.0f / 3.0f }; // 120 degrees

        m_Camera.FOV = std::clamp(m_Camera.FOV - m_MouseWheelSensitivity * static_cast<float>(event.GetOffsetX()), min, max);
    
        UpdateProjection();

        return false;
    }

    void ProjectionCameraController::UpdateView()
    {
        const DirectX::XMVECTOR direction{ spieler::math::SphericalToCartesian(m_Theta, m_Phi) };

        m_Camera.Front = DirectX::XMVector3Normalize(direction);
        m_Camera.View = DirectX::XMMatrixLookAtLH(m_Camera.EyePosition, DirectX::XMVectorAdd(m_Camera.EyePosition, m_Camera.Front), m_Camera.UpDirection);
    }

    void ProjectionCameraController::UpdateProjection()
    {
        m_Camera.Projection = DirectX::XMMatrixPerspectiveFovLH(m_Camera.FOV, m_Camera.AspectRatio, m_Camera.NearPlane, m_Camera.FarPlane);
    }

} // namespace sandbox