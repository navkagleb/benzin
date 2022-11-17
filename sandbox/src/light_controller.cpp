#include "bootstrap.hpp"

#include "light_controller.hpp"

#include <third_party/imgui/imgui.h>

namespace sandbox
{

    struct ObjectConstants
    {
        DirectX::XMMATRIX World{};
    };

    void DirectionalLightController::Init(const spieler::LightConstants& constants, float theta, float phi)
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);

        m_Constants->Strength = constants.Strength;

        m_SphericalCoordinates.Theta = theta;
        m_SphericalCoordinates.Phi = phi;

        Update();
    }

    void DirectionalLightController::OnImGuiRender()
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);


        ImGui::Begin(m_Name.c_str());
        {
            ImGui::ColorEdit3("Strength", reinterpret_cast<float*>(&m_Constants->Strength));
            ImGui::DragFloat("Radius", &m_SphericalCoordinates.Radius, 10.0f, 10.0f, 200.0f);
            ImGui::SliderAngle("Theta (XZ)", &m_SphericalCoordinates.Theta, -180.0f, 180.0f);
            ImGui::SliderAngle("Phi (Y)", &m_SphericalCoordinates.Phi, -180.0f, 180.0f);

            Update();
        }
        ImGui::End();
    }

    void DirectionalLightController::Update()
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);

        DirectX::XMStoreFloat3(&m_Constants->Direction, DirectX::XMVectorSubtract(DirectX::XMVECTOR{}, spieler::math::SphericalToCartesian(m_SphericalCoordinates.Theta, m_SphericalCoordinates.Phi)));

        m_Shape->Transform.Scale = DirectX::XMFLOAT3{ 0.1f, 0.1f, 0.1f };
        m_Shape->Transform.Translation = DirectX::XMFLOAT3
        {
            -m_Constants->Direction.x * m_SphericalCoordinates.Radius,
            -m_Constants->Direction.y * m_SphericalCoordinates.Radius,
            -m_Constants->Direction.z * m_SphericalCoordinates.Radius
        };
    }

    void PointLightController::Init(const spieler::LightConstants& constants)
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);

        m_Constants->Strength = constants.Strength;
        m_Constants->Position = constants.Position;
        m_Constants->FalloffEnd = constants.FalloffStart;
        m_Constants->FalloffEnd = constants.FalloffEnd;

        Update();
    }

    void PointLightController::OnImGuiRender()
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);

        ImGui::Begin(m_Name.c_str());
        {
            ImGui::ColorEdit3("Strength", reinterpret_cast<float*>(&m_Constants->Strength));
            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&m_Constants->Position));
            ImGui::DragFloat("Falloff Start", &m_Constants->FalloffStart);
            ImGui::DragFloat("Falloff End", &m_Constants->FalloffEnd);

            Update();
        }
        ImGui::End();
    }

    void PointLightController::Update()
    {
#if 0
        auto& shapeConstants{ m_Shape->ConstantBuffer.As<ObjectConstants>() };
        shapeConstants.World = DirectX::XMMatrixTranspose(
            DirectX::XMMatrixScaling(0.3f, 0.3f, 0.3f) *
            DirectX::XMMatrixTranslation(m_Constants->Position.x, m_Constants->Position.y, m_Constants->Position.z)
        );
#endif
    }

    void SpotLightController::Init(const spieler::LightConstants& constants, float pitch, float yaw)
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);

        m_Constants->Strength = constants.Strength;
        m_Constants->Position = constants.Position;
        m_Constants->FalloffStart = constants.FalloffStart;
        m_Constants->FalloffEnd = constants.FalloffEnd;
        m_Constants->SpotPower = constants.SpotPower;

        m_Pitch = pitch;
        m_Yaw = yaw;
        
        Update();
    }

    void SpotLightController::OnImGuiRender()
    {
        SPIELER_ASSERT(m_Constants);
        SPIELER_ASSERT(m_Shape);

        ImGui::Begin(m_Name.c_str());
        {
            ImGui::ColorEdit3("Strength", reinterpret_cast<float*>(&m_Constants->Strength));
            ImGui::DragFloat3("Direction", reinterpret_cast<float*>(&m_Constants->Direction), 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&m_Constants->Position));
            ImGui::DragFloat("Falloff Start", &m_Constants->FalloffStart);
            ImGui::DragFloat("Falloff End", &m_Constants->FalloffEnd);
            ImGui::DragFloat("SpotPower", &m_Constants->SpotPower, 0.1f, 1.0f, 100.0f);
            ImGui::Separator();
            ImGui::SliderAngle("Pitch", &m_Pitch);
            ImGui::SliderAngle("Yaw", &m_Yaw);

            Update();
        }
        ImGui::End();
    }

    void SpotLightController::SetPosition(const DirectX::XMVECTOR& position)
    {
        DirectX::XMStoreFloat3(&m_Constants->Position, position);
    }

    void SpotLightController::SetDirection(const DirectX::XMVECTOR& direction)
    {
        DirectX::XMStoreFloat3(&m_Constants->Direction, direction);
    }

    void SpotLightController::Update()
    {
        const DirectX::XMVECTOR direction{ spieler::math::PitchYawRollToCartesian(m_Pitch, m_Yaw, 0.0f) };
        DirectX::XMStoreFloat3(&m_Constants->Direction, direction);

#if 0
        auto& shapeConstants{ m_Shape->ConstantBuffer.As<ObjectConstants>() };
        shapeConstants.World = DirectX::XMMatrixTranspose(
            DirectX::XMMatrixScaling(0.2f, 0.2f, 0.2f) *
            DirectX::XMMatrixRotationRollPitchYaw(-m_Pitch - DirectX::XM_PIDIV2, m_Yaw, 0.0f) * 
            DirectX::XMMatrixTranslation(m_Constants->Position.x, m_Constants->Position.y, m_Constants->Position.z)
        );
#endif
    }

} // namespace sandbox