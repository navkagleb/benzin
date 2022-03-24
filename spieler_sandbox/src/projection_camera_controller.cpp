#include "projection_camera_controller.hpp"

#include <imgui/imgui.h>

#include <spieler/window/input.hpp>
#include <spieler/window/event_dispatcher.hpp>

namespace Sandbox
{

    ProjectionCameraController::ProjectionCameraController(float fov, float aspectRatio)
    {
        m_Camera.Fov = fov;
        m_Camera.AspectRatio = aspectRatio;

        UpdateView();
        UpdateProjection();
    }

    void ProjectionCameraController::OnEvent(Spieler::Event& event)
    {
        Spieler::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        if (!m_IsBlocked)
        {
            dispatcher.Dispatch<Spieler::MouseButtonPressedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseButtonPressed));
            dispatcher.Dispatch<Spieler::MouseMovedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseMoved));
            dispatcher.Dispatch<Spieler::MouseScrolledEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseScrolled));
        }
    }

    void ProjectionCameraController::OnUpdate(float dt)
    {
        if (m_IsBlocked)
        {
            return;
        }

        bool triggered = false;

        if (Spieler::Input::GetInstance().IsKeyPressed(Spieler::KeyCode_W))
        {
            m_Camera.EyePosition    = DirectX::XMVectorAdd(m_Camera.EyePosition, DirectX::XMVectorScale(m_Camera.Front, m_CameraSpeed * dt));
            triggered               = true;
        }

        if (Spieler::Input::GetInstance().IsKeyPressed(Spieler::KeyCode_S))
        {
            m_Camera.EyePosition    = DirectX::XMVectorSubtract(m_Camera.EyePosition, DirectX::XMVectorScale(m_Camera.Front, m_CameraSpeed * dt));
            triggered               = true;
        }

        if (Spieler::Input::GetInstance().IsKeyPressed(Spieler::KeyCode_A))
        {   
            m_Camera.EyePosition    = DirectX::XMVectorAdd(m_Camera.EyePosition, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_Camera.Front, m_Camera.UpDirection)), m_CameraSpeed * dt));
            triggered               = true;
        }

        if (Spieler::Input::GetInstance().IsKeyPressed(Spieler::KeyCode_D))
        {
            m_Camera.EyePosition    = DirectX::XMVectorSubtract(m_Camera.EyePosition, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_Camera.Front, m_Camera.UpDirection)), m_CameraSpeed * dt));
            triggered               = true;
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

            if (ImGui::SliderAngle("Fov", &m_Camera.Fov, 45.0f, 120.0f))
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

    bool ProjectionCameraController::OnWindowResized(Spieler::WindowResizedEvent& event)
    {
        m_Camera.AspectRatio = static_cast<float>(event.GetWidth()) / event.GetHeight();

        UpdateProjection();

        return true;
    }

    bool ProjectionCameraController::OnMouseButtonPressed(Spieler::MouseButtonPressedEvent& event)
    {
        m_LastMousePosition.x = static_cast<float>(event.GetX());
        m_LastMousePosition.y = static_cast<float>(event.GetY());

        return true;
    }

    bool ProjectionCameraController::OnMouseMoved(Spieler::MouseMovedEvent& event)
    {
        if (Spieler::Input::GetInstance().IsMouseButtonPressed(Spieler::MouseButton_Left))
        {
            const float dx = static_cast<float>(event.GetX()) - m_LastMousePosition.x;
            const float dy = static_cast<float>(event.GetY()) - m_LastMousePosition.y;

            m_Theta -= m_MouseSensitivity * dx;
            m_Phi -= m_MouseSensitivity * dy;

            m_Phi = Spieler::Math::Clamp(0.01f, m_Phi, DirectX::XM_PI - 0.01f);

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

        return true;
    }

    bool ProjectionCameraController::OnMouseScrolled(Spieler::MouseScrolledEvent& event)
    {
        static const float min = DirectX::XM_PIDIV4;
        static const float max = DirectX::XM_PI * 2.0f / 3.0f;

        m_Camera.Fov = Spieler::Math::Clamp(min, m_Camera.Fov - m_MouseSensitivity * static_cast<float>(event.GetOffsetX()), max);
    
        UpdateProjection();

        return true;
    }

    void ProjectionCameraController::UpdateView()
    {
        const DirectX::XMVECTOR direction = Spieler::Math::SphericalToCartesian(m_Theta, m_Phi);

        m_Camera.Front = DirectX::XMVector3Normalize(direction);
        m_Camera.View = DirectX::XMMatrixLookAtLH(m_Camera.EyePosition, DirectX::XMVectorAdd(m_Camera.EyePosition, m_Camera.Front), m_Camera.UpDirection);
    }

    void ProjectionCameraController::UpdateProjection()
    {
        m_Camera.Projection = DirectX::XMMatrixPerspectiveFovLH(m_Camera.Fov, m_Camera.AspectRatio, m_Camera.NearPlane, m_Camera.FarPlane);
    }

} // namespace Sandbox