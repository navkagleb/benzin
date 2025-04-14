#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/procedural_grass_resources.hpp>

namespace benzin
{
    class Scene;
}

namespace sandbox
{

    class ProceduralGrassPass : public benzin::RenderPass
    {
    public:
        explicit ProceduralGrassPass(const benzin::Scene& scene);
        ~ProceduralGrassPass() override;

    private:
        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        const benzin::Scene& m_Scene;

        std::unique_ptr<benzin::Texture> m_PerlinNoiseTexture;
        joint::ProceduralGrassConsts m_Consts{};
    };

}
