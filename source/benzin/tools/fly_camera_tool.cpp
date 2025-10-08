#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/fly_camera_tool.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/camera.hpp>

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

        PerspectiveCamera* camera = m_Controller.m_Camera;
        if (camera == nullptr)
            return;

        if (ImGui::DragFloat3("Position", reinterpret_cast<float*>(&camera->m_Position)))
        {
            camera->UpdateRightDirection();
            camera->UpdateWorldToViewMatrix();
        }

        bool isFrontDirectionUpdateRequired = false;
        isFrontDirectionUpdateRequired |= ImGui::SliderAngle("Pitch (X)", &m_Controller.m_Pitch, -89.0f, 89.0f);
        isFrontDirectionUpdateRequired |= ImGui::SliderAngle("Yaw (Y)", &m_Controller.m_Yaw, -180.0f, 180.0f);

        if (isFrontDirectionUpdateRequired)
        {
            const DirectX::XMVECTOR frontDirection = GetDirectionFromPitchYaw(m_Controller.m_Pitch, m_Controller.m_Yaw);
            camera->SetFrontDirection(frontDirection);
        }

        ImGui::BeginDisabled();
        ImGui::DragFloat3("Front Direction", reinterpret_cast<float*>(&camera->m_FrontDirection));
        ImGui::DragFloat3("Up Direction", reinterpret_cast<float*>(&camera->m_UpDirection));
        ImGui::EndDisabled();

        DrawMatrix4x4("World To View", camera->GetWorldToViewMatrix());
    }

    void FlyCameraTool::DrawProjectionProperties()
    {
        if (!ImGui::MainCollapsingHeader("Projection Props", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        PerspectiveCamera* camera = m_Controller.m_Camera;
        if (camera == nullptr)
            return;

        {
            ImGui::PushItemWidth(200.0f);
            BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

            bool isMatrixUpdateNeeded = false;
            isMatrixUpdateNeeded |= ImGui::SliderAngle("Vertical FOV", &camera->m_VerticalFovInRadians, 45.0f, 120.0f);
            isMatrixUpdateNeeded |= ImGui::DragFloat("Near plane", &camera->m_NearPlane, 0.001f, 0.001f, std::numeric_limits<float>::max());
            isMatrixUpdateNeeded |= ImGui::DragFloat("Far plane", &camera->m_FarPlane, 0.001f, 0.001f, std::numeric_limits<float>::max());

            if (isMatrixUpdateNeeded)
            {
                camera->UpdateViewToClipMatrix();
            }

            ImGui::BeginDisabled();
            ImGui::DragFloat("Aspect ratio", &camera->m_AspectRatio);
            ImGui::DragFloat("Tan half horizontal FOV", &camera->m_TanHalfFovX);
            ImGui::DragFloat("Tan half vertical FOV", &camera->m_TanHalfFovY);
            ImGui::EndDisabled();
        }

        DrawMatrix4x4("View To Clip", camera->GetViewToClipMatrix());
    }

}
