#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/fly_camera_tool.hpp"

#include "benzin/core/engine_math.hpp"
#include "benzin/tools/render_viewport_tool.hpp"

namespace benzin
{

    static void DrawMatrix4x4(const DirectX::XMMATRIX& matrix)
    {
        constexpr uint32_t matrixSize = 4;

        const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
        if (ImGui::BeginTable("MatrixTable", matrixSize, flags))
        {
            for (const uint32_t rowIndex : std::views::iota(0u, matrixSize))
            {
                ImGui::TableNextRow();

                const DirectX::XMVECTOR& row = matrix.r[rowIndex];

                for (const uint32_t columnIndex : std::views::iota(0u, matrixSize))
                {
                    ImGui::TableSetColumnIndex(columnIndex);

                    const float cellValue = *(reinterpret_cast<const float*>(&row) + columnIndex);
                    ImGui::Text("%0.4f", cellValue);
                }
            }
            ImGui::EndTable();
        }
    }

    //

    FlyCameraTool::FlyCameraTool(RenderViewportTool& renderViewportTool)
        : ImGuiTool{ "FlyCameraTool" }
        , m_Controller{ renderViewportTool.m_FlyCameraController }
    {}

    void FlyCameraTool::DrawWindowContent()
    {
        if (Imgui_MainCollapsingHeader("Controller Props"))
        {
            DrawControllerProperties();
        }
            
        if (Imgui_MainCollapsingHeader("View Props"))
        {
            DrawViewProperties();
        }
            
        if (Imgui_MainCollapsingHeader("Projection Props"))
        {
            DrawProjectionProperties();
        }
    }

    void FlyCameraTool::DrawControllerProperties()
    {
        ImGui::SliderFloat("CameraTranslationSpeed", &m_Controller.m_CameraTranslationSpeed, 0.001f, 0.03f);
        ImGui::SliderFloat("MouseSensitivity", &m_Controller.m_MouseSensitivity, 0.001f, 0.007f, "%.3f");
    }

    void FlyCameraTool::DrawViewProperties()
    {
        auto& camera = m_Controller.m_Camera;

        if (ImGui::DragFloat3("Position", reinterpret_cast<float*>(&camera.m_Position)))
        {
            camera.UpdateWorldToViewMatrix();
        }

        if (ImGui::DragFloat3("Front Direction", reinterpret_cast<float*>(&camera.m_FrontDirection)))
        {
            camera.SetFrontDirection(camera.m_FrontDirection);
            camera.UpdateWorldToViewMatrix();
        }

        if (ImGui::DragFloat3("Up Direction", reinterpret_cast<float*>(&camera.m_UpDirection)))
        {
            camera.UpdateWorldToViewMatrix();
        }

        if (ImGui::SliderAngle("Pitch (X)", &m_Controller.m_Pitch, -89.0f, 89.0f))
        {
            camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Controller.m_Pitch, m_Controller.m_Yaw));
        }

        if (ImGui::SliderAngle("Yaw (Y)", &m_Controller.m_Yaw, -180.0f, 180.0f))
        {
            camera.SetFrontDirection(GetDirectionFromPitchYaw(m_Controller.m_Pitch, m_Controller.m_Yaw));
        }

        ImGui::Separator();
        ImGui::Text("WorldToViewMatrix");
        DrawMatrix4x4(camera.GetWorldToViewMatrix());
    }

    void FlyCameraTool::DrawProjectionProperties()
    {
        auto& camera = m_Controller.m_Camera;
        auto* perspectiveProjection = m_Controller.GetPerspectiveProjection();

        if (!perspectiveProjection)
        {
            ImGui::Text("Projection isn't Perspective! FlyCameraTool supports only PerspectiveProjection");
        }
        else
        {
            if (ImGui::SliderAngle("VerticalFov", &perspectiveProjection->m_VerticalFovInRadians, 45.0f, 120.0f))
            {
                perspectiveProjection->UpdateViewToClipMatrix();
            }

            ImGui::Text("AspectRatio: %f", perspectiveProjection->m_AspectRatio);
            ImGui::Text("NearPlane: %f", perspectiveProjection->m_NearPlane);
            ImGui::Text("FarPlane: %f", perspectiveProjection->m_FarPlane);

            ImGui::Separator();
            ImGui::Text("ViewToClipMatrix");
            DrawMatrix4x4(camera.GetViewToClipMatrix());
        }
    }

}
