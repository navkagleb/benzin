#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/fly_camera_tool.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/tools/render_viewport_tool.hpp>

namespace benzin
{

    static void DrawMatrix4x4(const char* matrixName, const DirectX::XMMATRIX& matrix)
    {
        constexpr uint32_t matrixSize = 4;

        ImGui::Spacing();
        ImGui::Text(matrixName);

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
                    ImGui::Text(BenzinFormatData("{:.4f}", cellValue));
                }
            }
            ImGui::EndTable();
        }
    }

    static void DrawFrustumPlaneTable(const char* tableName, const DirectX::BoundingFrustum& frustum)
    {
        const auto planeNames = std::to_array(
        {
            "Near",
            "Far",
            "Right",
            "Left",
            "Top",
            "Bottom",
        });

        std::array<DirectX::XMVECTOR, 6> planes{};
        frustum.GetPlanes(&planes[0], &planes[1], &planes[2], &planes[3], &planes[4], &planes[5]);

        ImGui::Spacing();
        ImGui::Text(tableName);

        ImGui::WarningBox("NOTE: The frustum planes are directed outside the frustum", tableName);

        const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
        if (ImGui::BeginTable("FrustumTable", 5, flags))
        {
            for (uint32_t rowIndex = 0; rowIndex < 6; ++rowIndex)
            {
                ImGui::TableNextRow();

                const DirectX::XMVECTOR& plane = planes[rowIndex];

                for (uint32_t columnIndex = 0; columnIndex < 5; ++columnIndex)
                {
                    ImGui::TableSetColumnIndex(columnIndex);

                    if (columnIndex == 0)
                    {
                        ImGui::Text(planeNames[rowIndex]);
                        continue;
                    }

                    ImGui::Text(BenzinFormatData("{:.4f}", DirectX::XMVectorGetByIndex(plane, columnIndex - 1)));
                }
            }
            ImGui::EndTable();
        }
    }

    //

    FlyCameraTool::FlyCameraTool(FlyCameraController& controller)
        : ImGuiTool{ "Engine/FlyCameraTool" }
        , m_Controller{ controller }
    {}

    void FlyCameraTool::DrawWindowContent()
    {
        DrawControllerProperties();
        DrawViewProperties();
        DrawProjectionProperties();
    }

    void FlyCameraTool::DrawControllerProperties()
    {
        if (!ImGui::MainCollapsingHeader("Controller Props"))
            return;

        ImGui::SliderFloat("Camera translation speed", &m_Controller.m_CameraTranslationSpeed, 0.001f, 0.03f);
        ImGui::SliderFloat("Mouse sensitivity", &m_Controller.m_MouseSensitivity, 0.001f, 0.007f, "%.3f");
    }

    void FlyCameraTool::DrawViewProperties()
    {
        if (!ImGui::MainCollapsingHeader("View Props", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        Camera* camera = m_Controller.m_Camera;
        if (camera == nullptr)
            return;

        if (ImGui::DragFloat3("Position", reinterpret_cast<float*>(&camera->m_Position)))
        {
            camera->UpdateWorldToViewMatrix();
        }

        if (ImGui::DragFloat3("Front Direction", reinterpret_cast<float*>(&camera->m_FrontDirection)))
        {
            camera->SetFrontDirection(camera->m_FrontDirection);
            camera->UpdateWorldToViewMatrix();
        }

        if (ImGui::DragFloat3("Up Direction", reinterpret_cast<float*>(&camera->m_UpDirection)))
        {
            camera->UpdateWorldToViewMatrix();
        }

        if (ImGui::SliderAngle("Pitch (X)", &m_Controller.m_Pitch, -89.0f, 89.0f))
        {
            const DirectX::XMVECTOR frontDirection = GetDirectionFromPitchYaw(m_Controller.m_Pitch, m_Controller.m_Yaw);
            camera->SetFrontDirection(frontDirection);
        }

        if (ImGui::SliderAngle("Yaw (Y)", &m_Controller.m_Yaw, -180.0f, 180.0f))
        {
            const DirectX::XMVECTOR frontDirection = GetDirectionFromPitchYaw(m_Controller.m_Pitch, m_Controller.m_Yaw);
            camera->SetFrontDirection(frontDirection);
        }

        DrawMatrix4x4("World To View", camera->GetWorldToViewMatrix());
        DrawFrustumPlaneTable("World frustum", camera->GetWorldFrustum());
    }

    void FlyCameraTool::DrawProjectionProperties()
    {
        if (!ImGui::MainCollapsingHeader("Projection Props", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        Camera* camera = m_Controller.m_Camera;
        if (camera == nullptr)
            return;

        PerspectiveProjection* perspectiveProjection = m_Controller.GetPerspectiveProjection();
        if (perspectiveProjection == nullptr)
            return;

        bool isNeedToUpdateViewToClipMatrix = false;
        isNeedToUpdateViewToClipMatrix |= ImGui::SliderAngle("Vertical FOV", &perspectiveProjection->m_VerticalFovInRadians, 45.0f, 120.0f);
        isNeedToUpdateViewToClipMatrix |= ImGui::DragFloat("Near plane", &perspectiveProjection->m_NearPlane, 0.001f, 0.001f, std::numeric_limits<float>::max());
        isNeedToUpdateViewToClipMatrix |= ImGui::DragFloat("Far plane", &perspectiveProjection->m_FarPlane, 0.001f, 0.001f, std::numeric_limits<float>::max());

        if (isNeedToUpdateViewToClipMatrix)
        {
            perspectiveProjection->UpdateViewToClipMatrix();
        }

        ImGui::BeginDisabled();
        ImGui::DragFloat("Aspect ratio", &perspectiveProjection->m_AspectRatio);
        ImGui::EndDisabled();

        DrawMatrix4x4("View To Clip", camera->GetViewToClipMatrix());
        DrawFrustumPlaneTable("View frustum", perspectiveProjection->GetViewFrustum());
    }

}
