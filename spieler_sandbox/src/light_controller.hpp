#pragma once

#include <spieler/renderer/constant_buffer.hpp>
#include <spieler/renderer/light.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/math.hpp>

namespace Sandbox
{

    class LightController
    {
    public:
        void SetConstants(Spieler::LightConstants* constants) { m_Constants = constants; }
        void SetShape(Spieler::RenderItem* shape) { m_Shape = shape; }
        void SetName(const std::string& name) { m_Name = name; }

    public:
        virtual void OnImGuiRender() = 0;

    protected:
        Spieler::LightConstants* m_Constants{ nullptr };
        Spieler::RenderItem* m_Shape{ nullptr };
        std::string m_Name{ "LightController" };
    };

    class DirectionalLightController : public LightController
    {
    public:
        void Init(const Spieler::LightConstants& constants = {}, float theta = 0.0f, float phi = 0.0f);
        void OnImGuiRender() override;

    private:
        void Update();

    private:
        Spieler::Math::SphericalVector m_SphericalCoordinates{ DirectX::XM_PI * 0.8f, DirectX::XM_PI, 200.0f };
    };

    class PointLightController : public LightController
    {
    public:
        void Init(const Spieler::LightConstants& constants = {});
        void OnImGuiRender() override;

    private:
        void Update();
    };

    class SpotLightController : public LightController
    {
    public:
        void Init(const Spieler::LightConstants& constants = {}, float pitch = 0.0f, float yaw = 0.0f);
        void OnImGuiRender() override;

        void SetPosition(const DirectX::XMVECTOR& position);
        void SetDirection(const DirectX::XMVECTOR& direction);

    private:
        void Update();

    private:
        float m_Pitch{ 0.0f };
        float m_Yaw{ 0.0f };
     };

} // namespace Sandbox