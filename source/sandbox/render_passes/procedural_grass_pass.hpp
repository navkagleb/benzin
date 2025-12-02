#pragma once

#include <benzin/graphics2/render_pass.hpp>
#include <shaders/joint/procedural_grass_resources.hpp>

namespace benzin
{
    class Buffer;
}

namespace sandbox
{

    class ProceduralGrassPass : public benzin::RenderPass
    {
    public:
        ProceduralGrassPass();
        ~ProceduralGrassPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        joint::ProceduralGrassPassConsts m_Consts = {};
        std::unique_ptr<benzin::Texture> m_PerlinNoiseTexture;
        std::unique_ptr<benzin::Buffer> m_GrassPatchBuffer;
    };

}
