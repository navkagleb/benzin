#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/procedural_grass_resources.hpp>

namespace sandbox
{

    class ProceduralGrassPass : public benzin::RenderPass
    {
    public:
        ProceduralGrassPass();
        ~ProceduralGrassPass() override;

    private:
        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        std::unique_ptr<benzin::Texture> m_PerlinNoiseTexture;
        joint::ProceduralGrassConsts m_Consts{};
    };

}
