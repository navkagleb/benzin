#include "projection_camera_controller.h"

#include <imgui/imgui.h>

#include <spieler/window/input.h>
#include <spieler/window/event_dispatcher.h>

namespace Sandbox
{

    ProjectionCameraController::ProjectionCameraController(float fov, float aspectRatio)
    {
        m_Camera.Fov            = fov;
        m_Camera.AspectRatio    = aspectRatio;

        UpdateView();
        UpdateProjection();
    }

    void ProjectionCameraController::OnEvent(Spieler::Event& event)
    {
        Spieler::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<Spieler::MouseButtonPressedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseButtonPressed));
        dispatcher.Dispatch<Spieler::MouseMovedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseMoved));
        dispatcher.Dispatch<Spieler::MouseScrolledEvent>(SPIELER_BIND_EVENT_CALLBACK(OnMouseScrolled));
    }

    void ProjectionCameraController::OnUpdate(float dt)
    {
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

            if (ImGui::SliderFloat("Pitch (X)", &m_Pitch, -89.0f, 89.0f))
            {
                UpdateView();
            }

            if (ImGui::SliderFloat("Yaw (Y)", &m_Yaw, -180.0f, 180.0f))
            {
                UpdateView();
            }

            if (ImGui::SliderFloat("Roll (Z)", &m_Roll, -180.0f, 180.0f))
            {
                UpdateView();
            }

            ImGui::SliderFloat("CameraSpeed", &m_CameraSpeed, 20.0f, 100.0f);
            ImGui::SliderFloat("MouseSensitivity", &m_MouseSensitivity, 0.05f, 0.4f, "%.2f");
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
            float dx = static_cast<float>(event.GetX()) - m_LastMousePosition.x;
            float dy = static_cast<float>(event.GetY()) - m_LastMousePosition.y;

            m_Yaw   += m_MouseSensitivity * dx;
            m_Pitch += m_MouseSensitivity * dy;

            m_Pitch = std::min<float>(std::max<float>(-89.0f, m_Pitch), 89.0f);

            if (m_Yaw > 180.0f)
            {
                m_Yaw = -180.0f;
            }

            if (m_Yaw < -180.0f)
            {
                m_Yaw = 180.0f;
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

        m_Camera.Fov = std::min<float>(std::max<float>(min, m_Camera.Fov - m_MouseSensitivity * static_cast<float>(event.GetOffsetX())), max);
    
        UpdateProjection();

        return true;
    }

    void ProjectionCameraController::UpdateView()
    {
        DirectX::XMVECTOR direction = DirectX::XMVectorSet(
           DirectX::XMScalarCos(DirectX::XMConvertToRadians(m_Yaw)) * DirectX::XMScalarCos(DirectX::XMConvertToRadians(m_Pitch)),
           DirectX::XMScalarSin(DirectX::XMConvertToRadians(m_Pitch)),
           DirectX::XMScalarSin(DirectX::XMConvertToRadians(m_Yaw)) * DirectX::XMScalarCos(DirectX::XMConvertToRadians(m_Pitch)),
           1.0f
       );

        m_Camera.Front  = DirectX::XMVector3Normalize(direction);
        m_Camera.View   = DirectX::XMMatrixLookAtLH(m_Camera.EyePosition, DirectX::XMVectorAdd(m_Camera.EyePosition, m_Camera.Front), m_Camera.UpDirection);
    }

    void ProjectionCameraController::UpdateProjection()
    {
        m_Camera.Projection = DirectX::XMMatrixPerspectiveFovLH(m_Camera.Fov, m_Camera.AspectRatio, m_Camera.NearPlane, m_Camera.FarPlane);
    }

} // namespace Sandbox